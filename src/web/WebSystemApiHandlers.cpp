// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSystemApiHandlers.h"

#include <time.h>

#include <ArduinoJson.h>

#include "config/AppConfig.h"
#include "core/AppVersion.h"
#include "core/BoardInit.h"
#include "core/BootDiagnostics.h"
#include "core/BootHelpers.h"
#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "core/WebRuntimeState.h"
#include "lvgl_v8_port.h"
#include "web/WebDiagApiUtils.h"
#include "web/WebEventsApiUtils.h"
#include "web/WebResponseUtils.h"
#include "web/WebRuntimeCapture.h"
#include "web/WebStateApiUtils.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace {

constexpr size_t kEventsApiMaxEntries = 48;
constexpr size_t kDiagMaxErrorItems = 12;
constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";
Logger::RecentEntry g_events_snapshot[kEventsApiMaxEntries];

const char *refresh_callback_semantics_text(
    lvgl_port_refresh_callback_semantics_t semantics) {
    switch (semantics) {
        case LVGL_PORT_REFRESH_CALLBACK_VSYNC:
            return "vsync";
        case LVGL_PORT_REFRESH_CALLBACK_BOUNCE_FRAME_FINISH:
            return "bounce_frame_finish";
        case LVGL_PORT_REFRESH_CALLBACK_FRAME_BUFFER_COMPLETE:
            return "frame_buffer_complete";
        case LVGL_PORT_REFRESH_CALLBACK_DSI_REFRESH_DONE:
            return "dsi_refresh_done";
        case LVGL_PORT_REFRESH_CALLBACK_UNKNOWN:
        default:
            return "unknown";
    }
}

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

}  // namespace

namespace WebSystemApiHandlers {

void handleDiagRoot(WebHandlerContext &context,
                    const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!WebDiagApiUtils::accessAllowed(connectivity.wifi_ap_mode, connectivity.wifi_connected)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }
    WebResponseUtils::sendHtmlStreamProgmem(*context.server,
                                            reinterpret_cast<const uint8_t *>(
                                                WebTemplates::kDiagPageTemplate),
                                            sizeof(WebTemplates::kDiagPageTemplate) - 1,
                                            false,
                                            stream_context);
}

void handleDiagData(WebHandlerContext &context,
                    bool ota_busy,
                    const WebTransferSnapshot &web_stream_snapshot) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!WebDiagApiUtils::accessAllowed(connectivity.wifi_ap_mode, connectivity.wifi_connected)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    ArduinoJson::JsonDocument doc;
    const size_t event_count = Logger::copyRecentAlerts(g_events_snapshot, kEventsApiMaxEntries);
    WebDiagApiUtils::Payload payload{};
    const BootDiagnostics::Snapshot &boot = BootDiagnostics::state;
    payload.device.firmware = AppVersion::fullVersion();
    payload.device.build_id = AppVersion::buildId();
    payload.device.hardware_profile = AppVersion::hardwareProfile();
    payload.device.hardware_target = AppVersion::hardwareTarget();
    payload.panel_i2c.port = static_cast<int>(Config::I2C_PORT);
    payload.panel_i2c.sda_gpio = Config::I2C_SDA_PIN;
    payload.panel_i2c.scl_gpio = Config::I2C_SCL_PIN;
    payload.sensor_i2c.port = static_cast<int>(Config::SENSOR_I2C_PORT);
    payload.sensor_i2c.sda_gpio = Config::SENSOR_I2C_SDA_PIN;
    payload.sensor_i2c.scl_gpio = Config::SENSOR_I2C_SCL_PIN;
    payload.sensor_i2c_shared_with_panel = !Config::SENSOR_I2C_SEPARATE;
    lvgl_port_diagnostics_t display{};
    payload.display.available = lvgl_port_get_diagnostics(&display);
    if (payload.display.available) {
        payload.display.sample_ms = display.sample_ms;
        payload.display.timer_handler_count = display.timer_handler_count;
        payload.display.timer_handler_age_ms = display.timer_handler_age_ms;
        payload.display.flush_count = display.flush_count;
        payload.display.flush_age_ms = display.flush_age_ms;
        payload.display.refresh_callback_semantics =
            refresh_callback_semantics_text(display.refresh_callback_semantics);
        payload.display.refresh_callback_count = display.vsync_count;
        payload.display.refresh_callback_age_ms = display.vsync_age_ms;
        payload.display.refresh_callback_max_gap_ms =
            display.refresh_callback_max_gap_ms;
        payload.display.framebuffer_handoff_count =
            display.presented_frame_count;
        payload.display.framebuffer_wait_timeout_count =
            display.vsync_wait_timeout_count;
        payload.display.display_sync_fault = display.display_sync_fault;
        payload.display.runtime_lock_failures = display.lock_fail_count;
        payload.display.startup_logo_lock_misses =
            display.startup_lock_miss_count;
        payload.display.touch_read_errors = display.touch_read_error_count;
        payload.display.touch_offline = display.touch_offline;
        payload.display.touch_polling.mode = display.touch_mode;
        payload.display.touch_polling.irq_registered =
            display.touch_irq_registered;
        payload.display.touch_polling.irq_armed = display.touch_irq_armed;
        payload.display.touch_polling.irq_config_verified =
            display.touch_irq_config_verified;
        payload.display.touch_polling.irq_config_mode =
            display.touch_irq_config_mode;
        payload.display.touch_polling.idle_enabled =
            display.touch_screen_idle_enabled;
        payload.display.touch_polling.idle_active =
            display.touch_screen_idle_active;
        payload.display.touch_polling.fail_safe =
            display.touch_screen_idle_fail_safe;
        payload.display.touch_polling.status_reads =
            display.touch_status_read_count;
        payload.display.touch_polling.full_reads =
            display.touch_full_read_count;
        payload.display.touch_polling.skipped_callbacks =
            display.touch_idle_skip_count;
        payload.display.touch_polling.idle_entries =
            display.touch_idle_entry_count;
        payload.display.touch_polling.irq_exits =
            display.touch_idle_irq_exit_count;
        payload.display.touch_polling.fallback_probes =
            display.touch_idle_fallback_probe_count;
        payload.display.touch_polling.missed_irq_presses =
            display.touch_idle_missed_irq_press_count;
        payload.display.touch_polling.irq_arm_failures =
            display.touch_irq_arm_failure_count;
        payload.display.touch_polling.irq_no_frame =
            display.touch_irq_no_frame_count;
        payload.display.screen_flip_180 = display.screen_flip_180;
        payload.display.rotation_pipeline_active =
            display.rotation_pipeline_active;
        payload.display.rotated_copy_switch_count =
            display.rotated_copy_switch_count;
        payload.display.framebuffer_ownership_violation_count =
            display.framebuffer_ownership_violation_count;
        payload.display.paused = display.paused;
    }
    payload.uptime_s = millis() / 1000UL;
    payload.ota_busy = ota_busy;
    payload.heap_free = ESP.getFreeHeap();
    payload.heap_min_free = ESP.getMinFreeHeap();
    payload.network = WebRuntimeCapture::captureNetworkSnapshot(context);
    payload.web_stream = web_stream_snapshot;
    payload.boot.reset_reason = BootHelpers::resetReasonText(boot.reset_reason);
    payload.boot.auto_recovery_boot = boot.auto_recovery_boot;
    payload.boot.i2c_status = I2cBusRecovery::statusText(boot.i2c_status);
    payload.boot.sda_high = boot.sda_high;
    payload.boot.scl_high = boot.scl_high;
    payload.boot.board_ready = boot.board_ready;
    payload.boot.board_rounds = boot.board_rounds;
    payload.boot.board_begin_attempts = boot.board_begin_attempts;
    payload.boot.cold_power_start = boot.cold_power_start;
    payload.boot.cold_power_wait_ms = boot.cold_power_wait_ms;
    payload.boot.expander_probe_status = Ch422gReadyProbe::statusText(boot.expander_probe_status);
    payload.boot.expander_probe_result_valid =
        boot.expander_probe_status != Ch422gReadyProbe::Status::NotRun;
    payload.boot.expander_probe_attempts = boot.expander_probe_attempts;
    payload.boot.expander_probe_wait_ms = boot.expander_probe_wait_ms;
    payload.boot.expander_probe_error = boot.expander_probe_error;
    payload.boot.expander_probe_phase = Ch422gReadyProbe::phaseText(boot.expander_probe_phase);
    payload.boot.expander_probe_failed_address = boot.expander_probe_failed_address;
    payload.boot.expander_probe_failed_value = boot.expander_probe_failed_value;
    payload.boot.expander_probe_bus_recoveries = boot.expander_probe_bus_recoveries;
    payload.boot.expander_probe_failure_lines_valid = boot.expander_probe_failure_lines_valid;
    payload.boot.expander_probe_failure_sda_high = boot.expander_probe_failure_sda_high;
    payload.boot.expander_probe_failure_scl_high = boot.expander_probe_failure_scl_high;
    payload.boot.expander_probe_recovery_sda_high = boot.expander_probe_recovery_sda_high;
    payload.boot.expander_probe_recovery_scl_high = boot.expander_probe_recovery_scl_high;
    payload.boot.expander_probe_recovery_pulses = boot.expander_probe_recovery_pulses;
    payload.boot.board_stage = BoardInit::stageText(boot.board_stage);
    payload.boot.board_failure = BoardInit::failureText(boot.board_failure);
    payload.boot.lvgl_ready = boot.lvgl_ready;
    payload.boot.gt911_startup = boot.gt911_startup;
    const BacklightWakeBreadcrumbs::BootSnapshot &previous_trace =
        boot.previous_backlight_trace;
    payload.boot.previous_backlight_trace_status =
        BacklightWakeBreadcrumbs::statusText(previous_trace.status);
    payload.boot.previous_backlight_trace_valid = previous_trace.has_trace;
    payload.boot.previous_backlight_trace_retention_uncertain =
        previous_trace.retention_uncertain;
    if (previous_trace.has_trace) {
        const BacklightWakeBreadcrumbs::Trace &trace = previous_trace.trace;
        payload.boot.previous_backlight_trace_event =
            BacklightWakeBreadcrumbs::eventText(trace.event);
        payload.boot.previous_backlight_trace_stage =
            BacklightWakeBreadcrumbs::stageText(trace.stage);
        payload.boot.previous_backlight_trace_driver_result =
            BacklightWakeBreadcrumbs::driverResultText(trace.driver_result);
        payload.boot.previous_backlight_trace_command_result =
            BacklightWakeBreadcrumbs::commandResultText(trace.command_result);
        payload.boot.previous_backlight_trace_sequence = trace.sequence;
        payload.boot.previous_backlight_trace_uptime_ms = trace.uptime_ms;
        payload.boot.previous_backlight_trace_epoch_s = trace.epoch_s;
        payload.boot.previous_backlight_trace_driver_duration_us = trace.driver_duration_us;
        payload.boot.previous_backlight_trace_pre_quiet_elapsed_ms =
            trace.pre_quiet_elapsed_ms;
        payload.boot.previous_backlight_trace_pre_quiet_active_operations =
            trace.pre_quiet_active_operations;
        payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded =
            trace.pre_quiet_wait_exceeded;
        payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded_active_operations =
            trace.pre_quiet_wait_exceeded_active_operations;
        payload.boot.previous_backlight_trace_pre_quiet_forced_by_timeout =
            trace.pre_quiet_forced_by_timeout;
        payload.boot.previous_backlight_trace_expected_network_manager_addr =
            trace.expected_network_manager_addr;
        payload.boot.previous_backlight_trace_post_backlight_network_manager_addr =
            trace.post_backlight_network_manager_addr;
        payload.boot.previous_backlight_trace_pre_render_network_manager_addr =
            trace.pre_render_network_manager_addr;
        payload.boot.previous_backlight_trace_post_backlight_task_handle =
            trace.post_backlight_task_handle;
        payload.boot.previous_backlight_trace_pre_render_task_handle =
            trace.pre_render_task_handle;
        payload.boot.previous_backlight_trace_target_on = trace.target_on;
        payload.boot.previous_backlight_trace_previous_on = trace.previous_on;
        payload.boot.previous_backlight_trace_before_valid = trace.before.valid;
        payload.boot.previous_backlight_trace_before_sda_high = trace.before.sda_high;
        payload.boot.previous_backlight_trace_before_scl_high = trace.before.scl_high;
        payload.boot.previous_backlight_trace_after_driver_valid = trace.after_driver.valid;
        payload.boot.previous_backlight_trace_after_driver_sda_high = trace.after_driver.sda_high;
        payload.boot.previous_backlight_trace_after_driver_scl_high = trace.after_driver.scl_high;
        payload.boot.previous_backlight_trace_after_probe_valid = trace.after_wake_probe.valid;
        payload.boot.previous_backlight_trace_after_probe_sda_high =
            trace.after_wake_probe.sda_high;
        payload.boot.previous_backlight_trace_after_probe_scl_high =
            trace.after_wake_probe.scl_high;
    }
    WebDiagApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(),
                              payload,
                              g_events_snapshot,
                              event_count,
                              kDiagMaxErrorItems);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

void handleStateData(WebHandlerContext &context, bool ota_busy, const WebOtaSnapshot &ota_snapshot) {
    if (!context.server || !context.web_runtime) {
        return;
    }

    const WebRuntimeSnapshot runtime = context.web_runtime->snapshot();
    const uint32_t uptime_s = millis() / 1000UL;
    const time_t now_epoch = time(nullptr);

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::Payload payload{};
    payload.data = runtime.data;
    payload.gas_warmup = runtime.gas_warmup;
    payload.uptime_s = uptime_s;
    payload.timestamp_ms = millis();
    payload.has_time_epoch = now_epoch > 0;
    payload.time_epoch_s = static_cast<int64_t>(now_epoch);
    payload.network = WebRuntimeCapture::captureNetworkSnapshot(context);
    const WebUiBridge::Snapshot ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    payload.settings = WebUiBridgeAdapters::captureSettingsSnapshot(ui_snapshot);
    payload.thresholds = context.display_thresholds
        ? context.display_thresholds->snapshot()
        : DisplayThresholds::defaults();
    payload.ntp_active = ui_snapshot.ntp_active;
    payload.ntp_syncing = ui_snapshot.ntp_syncing;
    payload.ntp_error = ui_snapshot.ntp_error;
    payload.ntp_last_sync_ms = ui_snapshot.ntp_last_sync_ms;
    payload.dac_available = runtime.fan.available;
    payload.ota_busy = ota_busy;
    payload.ota = ota_snapshot;
    payload.firmware = AppVersion::fullVersion();
    payload.build_date = __DATE__;
    payload.build_time = __TIME__;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

void handleEventsData(WebHandlerContext &context, bool ota_busy) {
    if (!context.server) {
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    const size_t count = Logger::copyRecent(g_events_snapshot, kEventsApiMaxEntries);

    ArduinoJson::JsonDocument doc;
    WebEventsApiUtils::fillJson(
        doc.to<ArduinoJson::JsonObject>(), g_events_snapshot, count, millis() / 1000UL);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

}  // namespace WebSystemApiHandlers
