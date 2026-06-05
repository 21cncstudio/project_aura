// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/MemoryMonitor.h"

#include <string.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include "core/Logger.h"

void MemoryMonitor::begin(uint32_t interval_ms) {
    interval_ms_ = interval_ms;
    last_log_ms_ = millis();
}

void MemoryMonitor::poll(uint32_t now_ms) {
    if (!enabled_ || interval_ms_ == 0) {
        return;
    }
    if (now_ms - last_log_ms_ >= interval_ms_) {
        last_log_ms_ = now_ms;
        logNow("periodic");
    }
}

void MemoryMonitor::logNow(const char *reason) {
    if (!enabled_) {
        return;
    }
    uint32_t heap_free = ESP.getFreeHeap();
    uint32_t heap_min = ESP.getMinFreeHeap();
    uint32_t heap_max = ESP.getMaxAllocHeap();
    uint32_t cap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t cap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    uint32_t cap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t psram_physical = esp_psram_get_size();
    const uint32_t psram_heap_total = ESP.getPsramSize();
    const uint32_t psram_free = ESP.getFreePsram();
    const uint32_t spiram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const uint32_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const uint32_t spiram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    const uint32_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    const char *tag = "Mem";
    const char *reason_text = (reason && reason[0] != '\0') ? reason : "manual";

    // Keep memory telemetry visible in serial logs by default.
    Logger::Level level = Logger::Info;

    Logger::log(level, tag,
                "%s heap free=%u min=%u max=%u cap free=%u min=%u largest=%u int free=%u min=%u largest=%u psram physical=%u heap_total=%u free=%u spiram total=%u free=%u min=%u largest=%u",
                reason_text,
                heap_free, heap_min, heap_max,
                cap_free, cap_min, cap_largest,
                internal_free, internal_min, internal_largest,
                psram_physical, psram_heap_total, psram_free,
                spiram_total, spiram_free, spiram_min, spiram_largest);
}
