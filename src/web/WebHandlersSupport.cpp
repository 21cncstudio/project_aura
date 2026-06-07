// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebHandlersSupport.h"

#include <atomic>
#include <Update.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "core/Logger.h"
#include "core/SafeRestart.h"
#include "core/Watchdog.h"
#include "lvgl_v8_port.h"
#include "web/WebOtaState.h"
#include "web/WebUiBridge.h"

namespace {

constexpr uint32_t kDeferredRestartDelayMs = 1500;
constexpr uint32_t kHttpStreamSlowWriteWarnMs = 200;
constexpr uint32_t kOtaPreflightUiLeaseMs = 15000;
constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";

WebHandlerContext *g_ctx = nullptr;
OtaDeferredRestart::Controller g_restart_controller;
WebDeferredActionsState g_deferred_actions;
WebOtaState g_ota_state;
WebStreamState g_web_stream_state;
std::atomic<bool> g_restart_in_progress{false};
bool g_ota_wifi_ps_saved = false;
wifi_ps_type_t g_ota_wifi_ps_prev = WIFI_PS_NONE;
std::atomic<uint32_t> g_ota_preflight_ui_due_ms{0};

uint32_t web_stream_now_ms(void *) {
    return millis();
}

void web_stream_delay_ms(void *, uint16_t delay_ms) {
    delay(delay_ms);
}

void web_stream_kick_watchdog(void *) {
    Watchdog::kick();
}

const WebStreamRuntime &default_web_stream_runtime() {
    static const WebStreamRuntime runtime = {
        nullptr,
        web_stream_now_ms,
        web_stream_delay_ms,
        web_stream_kick_watchdog,
        nullptr,
        0,
    };
    return runtime;
}

uint32_t web_response_now_ms(void *) {
    return millis();
}

uint32_t web_response_wifi_sta_connected_elapsed_ms(void *) {
    return (g_ctx && g_ctx->wifi_sta_connected_elapsed_ms)
               ? g_ctx->wifi_sta_connected_elapsed_ms()
               : 0;
}

uint32_t ota_upload_timeout_ms(size_t image_size_bytes) {
    constexpr uint32_t kUnknownSizeTimeoutMs = 15UL * 60UL * 1000UL;
    constexpr uint32_t kBaseTimeoutMs = 10UL * 60UL * 1000UL;
    constexpr uint32_t kMaxTimeoutMs = 15UL * 60UL * 1000UL;
    constexpr size_t kExtraTimeoutStepBytes = 1024UL * 1024UL;
    constexpr uint32_t kExtraTimeoutStepMs = 60UL * 1000UL;

    if (image_size_bytes == 0) {
        return kUnknownSizeTimeoutMs;
    }

    const uint64_t extra_steps =
        (static_cast<uint64_t>(image_size_bytes) + kExtraTimeoutStepBytes - 1) / kExtraTimeoutStepBytes;
    const uint64_t timeout_ms = kBaseTimeoutMs + (extra_steps * kExtraTimeoutStepMs);
    if (timeout_ms >= kMaxTimeoutMs) {
        return kMaxTimeoutMs;
    }
    return static_cast<uint32_t>(timeout_ms);
}

void ota_reset_state() {
    g_ota_state.reset();
}

void ota_set_ui_screen(bool active) {
    if (!g_ctx || !g_ctx->web_ui_bridge) {
        return;
    }
    g_ctx->web_ui_bridge->requestFirmwareUpdateScreen(active);
}

void ota_arm_preflight_ui() {
    ota_set_ui_screen(true);
    g_ota_preflight_ui_due_ms.store(millis() + kOtaPreflightUiLeaseMs, std::memory_order_release);
}

void ota_cancel_preflight_ui() {
    g_ota_preflight_ui_due_ms.store(0, std::memory_order_release);
}

void ota_restore_wifi_power_save();

void ota_set_error(const String &error) {
    g_ota_state.setErrorOnce(error, millis());
}

void ota_disable_wifi_power_save_for_upload() {
    const wifi_mode_t mode = WiFi.getMode();
    if ((mode & WIFI_MODE_STA) == 0) {
        g_ota_wifi_ps_saved = false;
        g_ota_wifi_ps_prev = WIFI_PS_NONE;
        return;
    }

    wifi_ps_type_t current_ps = WIFI_PS_NONE;
    const esp_err_t get_err = esp_wifi_get_ps(&current_ps);
    if (get_err != ESP_OK) {
        LOGW("OTA", "failed to read WiFi power-save mode: %s", esp_err_to_name(get_err));
        g_ota_wifi_ps_saved = false;
        g_ota_wifi_ps_prev = WIFI_PS_NONE;
        return;
    }

    g_ota_wifi_ps_prev = current_ps;
    g_ota_wifi_ps_saved = true;
    if (current_ps == WIFI_PS_NONE) {
        return;
    }

    const esp_err_t set_err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (set_err == ESP_OK) {
        LOGI("OTA", "WiFi power-save disabled for OTA (prev=%d)", static_cast<int>(current_ps));
        return;
    }

    LOGW("OTA", "failed to disable WiFi power-save for OTA: %s", esp_err_to_name(set_err));
    g_ota_wifi_ps_saved = false;
    g_ota_wifi_ps_prev = WIFI_PS_NONE;
}

void ota_restore_wifi_power_save() {
    if (!g_ota_wifi_ps_saved) {
        return;
    }

    const wifi_ps_type_t restore_mode = g_ota_wifi_ps_prev;
    const esp_err_t set_err = esp_wifi_set_ps(restore_mode);
    if (set_err == ESP_OK) {
        LOGI("OTA", "WiFi power-save restored after OTA (mode=%d)", static_cast<int>(restore_mode));
    } else {
        LOGW("OTA", "failed to restore WiFi power-save mode=%d: %s",
             static_cast<int>(restore_mode),
             esp_err_to_name(set_err));
    }
    g_ota_wifi_ps_saved = false;
    g_ota_wifi_ps_prev = WIFI_PS_NONE;
}

}  // namespace

namespace WebHandlersSupport {

const char *otaBusyJson() {
    return kApiErrorOtaBusyJson;
}

void init(WebHandlerContext *context) {
    g_ctx = context;
    g_deferred_actions.reset();
    g_web_stream_state.reset();
    g_restart_controller.reset();
    g_restart_in_progress.store(false, std::memory_order_release);
    ota_cancel_preflight_ui();
    ota_reset_state();
}

WebHandlerContext *context() {
    return g_ctx;
}

bool isOtaBusy() {
    return g_restart_in_progress.load(std::memory_order_acquire) ||
           g_restart_controller.is_busy(g_ota_state.isBusy());
}

bool isOtaStatusBusy(const WebOtaSnapshot &ota_snapshot) {
    return g_ota_state.isBusy() || ota_snapshot.reboot_pending;
}

bool consumeRestartRequest() {
    return g_restart_controller.consume_request();
}

void requestRestart(uint32_t delay_ms) {
    g_restart_controller.schedule(millis(), delay_ms);
}

void beginRestartShutdown() {
    g_restart_in_progress.store(true, std::memory_order_release);
}

bool shouldPauseMqttForTransfer() {
    return WebResponseUtils::shouldPauseMqttForTransfer(responseContext());
}

void noteMqttConnectDeferred() {
    g_web_stream_state.noteMqttConnectDeferred();
}

void noteMqttPublishDeferred() {
    g_web_stream_state.noteMqttPublishDeferred();
}

void pollDeferred() {
    if (!g_ctx) {
        return;
    }

    const uint32_t now_ms = millis();
    const uint32_t ota_preflight_ui_due_ms =
        g_ota_preflight_ui_due_ms.load(std::memory_order_acquire);
    if (ota_preflight_ui_due_ms != 0 &&
        static_cast<int32_t>(now_ms - ota_preflight_ui_due_ms) >= 0 &&
        !isOtaBusy()) {
        ota_set_ui_screen(false);
        ota_cancel_preflight_ui();
    }

    const WebDeferredActionsDue due = g_deferred_actions.pollDue(now_ms);

    if (due.wifi_start_sta && g_ctx->wifi_start_sta) {
        g_ctx->wifi_start_sta();
    }

    if (due.mqtt_sync && g_ctx->mqtt_sync_with_wifi) {
        g_ctx->mqtt_sync_with_wifi();
    }

    g_restart_controller.poll(now_ms);
    g_ota_state.poll(now_ms);
}

WebOtaSnapshot otaSnapshot() {
    return g_ota_state.snapshot();
}

WebTransferSnapshot streamSnapshot(uint32_t now_ms) {
    return g_web_stream_state.snapshot(now_ms);
}

WebResponseUtils::StreamContext responseContext() {
    WebResponseUtils::StreamContext context;
    context.stream_state = &g_web_stream_state;
    context.stream_runtime = &default_web_stream_runtime();
    context.slow_write_warn_ms = kHttpStreamSlowWriteWarnMs;
    context.nowMs = web_response_now_ms;
    context.wifiStaConnectedElapsedMs = web_response_wifi_sta_connected_elapsed_ms;
    return context;
}

WebDeferredActionsState &deferredActions() {
    return g_deferred_actions;
}

OtaDeferredRestart::Controller &restartController() {
    return g_restart_controller;
}

WebOtaHandlers::Runtime otaRuntime(WebHandlerContext &context) {
    return WebOtaHandlers::Runtime{
        context,
        g_ota_state,
        g_restart_controller,
        kDeferredRestartDelayMs,
        ota_upload_timeout_ms,
        ota_disable_wifi_power_save_for_upload,
        ota_restore_wifi_power_save,
        ota_arm_preflight_ui,
        ota_cancel_preflight_ui,
        ota_set_ui_screen,
        ota_set_error,
    };
}

}  // namespace WebHandlersSupport
