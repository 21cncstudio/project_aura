// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/RgbDmaDiagnostics.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_private/gdma.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error "RGB DMA diagnostics are pinned to the ESP32-S3 driver ABI"
#endif

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 3, 2)
#error "RGB DMA diagnostics require the audited ESP-IDF 5.3.2 driver"
#endif

#if defined(CONFIG_LCD_RGB_RESTART_IN_VSYNC) && \
    CONFIG_LCD_RGB_RESTART_IN_VSYNC
#error "RGB DMA diagnostics require conditional, not unconditional, VSYNC restart"
#endif

namespace {

// These offsets are from esp_rgb_panel_t in the exact audited libesp_lcd.a.
// scripts/check_rgb_dma_diagnostics.py verifies the archive hash before every
// firmware build, so a framework update fails closed instead of reading an
// unknown private layout.
constexpr size_t kDmaChannelOffset = 88;
constexpr size_t kExpectedEofCountOffset = 268;
constexpr uint32_t kAuraExpectedEofPerFrame = 48;

static_assert(sizeof(size_t) == sizeof(uint32_t),
              "Audited RGB driver uses 32-bit size_t fields");
static_assert(sizeof(gdma_channel_handle_t) == sizeof(uint32_t),
              "Audited RGB driver uses 32-bit handles");
static_assert(sizeof(esp_lcd_panel_handle_t) == sizeof(uint32_t),
              "Audited RGB driver uses 32-bit panel handles");

struct DiagnosticState {
    uint32_t instrumented;
    uint32_t layout_valid;
    uint32_t multiple_panels;
    esp_lcd_panel_handle_t rgb_panel;
    gdma_channel_handle_t rgb_dma_channel;
    uint32_t expected_eof_per_frame;
    uint32_t event_sequence;
    uint32_t rgb_isr_dma_start_count;
    uint32_t rgb_isr_dma_start_last_ms;
    uint32_t rgb_isr_dma_start_failure_count;
};

DRAM_ATTR volatile DiagnosticState g_state{};

static inline __attribute__((always_inline)) void memoryBarrier() {
    __asm__ __volatile__("memw" ::: "memory");
}

uint32_t readDriverU32(esp_lcd_panel_handle_t panel, size_t offset) {
    const auto *address = reinterpret_cast<volatile const uint32_t *>(
        reinterpret_cast<const uint8_t *>(panel) + offset);
    return *address;
}

gdma_channel_handle_t readDriverDmaChannel(
    esp_lcd_panel_handle_t panel) {
    const auto *address =
        reinterpret_cast<gdma_channel_handle_t volatile const *>(
            reinterpret_cast<const uint8_t *>(panel) + kDmaChannelOffset);
    return *address;
}

}  // namespace

extern "C" esp_err_t __real_esp_lcd_rgb_panel_register_event_callbacks(
    esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_callbacks_t *callbacks,
    void *user_ctx);

extern "C" esp_err_t __wrap_esp_lcd_rgb_panel_register_event_callbacks(
    esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_callbacks_t *callbacks,
    void *user_ctx) {
    const esp_err_t result =
        __real_esp_lcd_rgb_panel_register_event_callbacks(
            panel, callbacks, user_ctx);
    if (result != ESP_OK || panel == nullptr) {
        return result;
    }

    const gdma_channel_handle_t dma_channel =
        readDriverDmaChannel(panel);
    const uint32_t expected_eof =
        readDriverU32(panel, kExpectedEofCountOffset);
    const bool layout_valid =
        dma_channel != nullptr && expected_eof == kAuraExpectedEofPerFrame;

    memoryBarrier();
    if (g_state.multiple_panels != 0 ||
        (g_state.instrumented != 0 &&
         g_state.rgb_panel != nullptr &&
         g_state.rgb_panel != panel)) {
        // Aura owns one boot-lifetime RGB panel. Disable the channel match if
        // that lifecycle ever changes instead of attributing another panel's
        // DMA activity to the original display.
        g_state.instrumented = 0;
        memoryBarrier();
        g_state.multiple_panels = 1;
        g_state.rgb_panel = nullptr;
        g_state.rgb_dma_channel = nullptr;
        g_state.expected_eof_per_frame = expected_eof;
        g_state.layout_valid = 0;
        g_state.event_sequence = 0;
        g_state.rgb_isr_dma_start_count = 0;
        g_state.rgb_isr_dma_start_last_ms = 0;
        g_state.rgb_isr_dma_start_failure_count = 0;
        memoryBarrier();
        g_state.instrumented = 1;
        return result;
    }

    // Publish the immutable channel identity last. The wrapper does not alter
    // the callback table and adds no work to the per-frame VSYNC path.
    g_state.instrumented = 0;
    memoryBarrier();
    g_state.rgb_panel = panel;
    g_state.rgb_dma_channel = layout_valid ? dma_channel : nullptr;
    g_state.expected_eof_per_frame = expected_eof;
    g_state.layout_valid = layout_valid ? 1U : 0U;
    memoryBarrier();
    g_state.instrumented = 1;

    return result;
}

extern "C" esp_err_t __real_gdma_start(
    gdma_channel_handle_t dma_chan, intptr_t desc_base_addr);

extern "C" IRAM_ATTR esp_err_t __wrap_gdma_start(
    gdma_channel_handle_t dma_chan, intptr_t desc_base_addr) {
    // Keep every wrapped GDMA caller behaviorally identical. The diagnostic
    // record is made only after the real start and only for the registered RGB
    // channel when the driver calls it from interrupt context.
    const esp_err_t result =
        __real_gdma_start(dma_chan, desc_base_addr);
    if (g_state.instrumented == 0) {
        return result;
    }

    memoryBarrier();
    if (g_state.layout_valid == 0 ||
        g_state.rgb_dma_channel != dma_chan) {
        return result;
    }
    if (xPortInIsrContext() == 0) {
        return result;
    }

    ++g_state.event_sequence;
    memoryBarrier();
    g_state.rgb_isr_dma_start_last_ms =
        static_cast<uint32_t>(xTaskGetTickCountFromISR()) *
        portTICK_PERIOD_MS;
    if (result != ESP_OK &&
        g_state.rgb_isr_dma_start_failure_count != UINT32_MAX) {
        ++g_state.rgb_isr_dma_start_failure_count;
    }
    if (g_state.rgb_isr_dma_start_count != UINT32_MAX) {
        ++g_state.rgb_isr_dma_start_count;
    }
    memoryBarrier();
    ++g_state.event_sequence;
    memoryBarrier();

    return result;
}

namespace RgbDmaDiagnostics {

bool getSnapshot(Snapshot *out) {
    if (out == nullptr) {
        return false;
    }

    out->instrumented = g_state.instrumented != 0;
    memoryBarrier();
    out->layout_valid = g_state.layout_valid != 0;
    out->expected_eof_per_frame = g_state.expected_eof_per_frame;

    uint32_t count = 0;
    uint32_t last_ms = 0;
    uint32_t failure_count = 0;
    bool coherent = false;
    for (uint32_t attempt = 0; attempt < 8; ++attempt) {
        const uint32_t sequence_before = g_state.event_sequence;
        if ((sequence_before & 1U) != 0) {
            continue;
        }
        memoryBarrier();
        count = g_state.rgb_isr_dma_start_count;
        last_ms = g_state.rgb_isr_dma_start_last_ms;
        failure_count = g_state.rgb_isr_dma_start_failure_count;
        memoryBarrier();
        const uint32_t sequence_after = g_state.event_sequence;
        if (sequence_before == sequence_after &&
            (sequence_after & 1U) == 0) {
            coherent = true;
            break;
        }
    }
    out->snapshot_coherent = coherent;
    out->rgb_isr_dma_start_count = count;
    out->rgb_isr_dma_start_failure_count = failure_count;
    if (!coherent || count == 0) {
        out->rgb_isr_dma_start_last_ms = UINT32_MAX;
        out->rgb_isr_dma_start_age_ms = UINT32_MAX;
    } else {
        const uint32_t now_ms =
            static_cast<uint32_t>(xTaskGetTickCount()) *
            portTICK_PERIOD_MS;
        out->rgb_isr_dma_start_last_ms = last_ms;
        out->rgb_isr_dma_start_age_ms = now_ms - last_ms;
    }
    return out->instrumented;
}

}  // namespace RgbDmaDiagnostics
