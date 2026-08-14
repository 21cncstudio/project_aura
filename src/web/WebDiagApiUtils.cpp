// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDiagApiUtils.h"

#include "web/WebEventsUtils.h"
#include "web/WebNetworkUtils.h"
#include "web/WebStreamPolicy.h"

namespace WebDiagApiUtils {

bool accessAllowed(bool ap_mode, bool sta_connected) {
    return ap_mode || sta_connected;
}

void fillJson(ArduinoJson::JsonObject root,
              const Payload &payload,
              const Logger::RecentEntry *recent_errors,
              size_t recent_error_count,
              size_t max_error_items) {
    root["success"] = true;
    root["uptime_s"] = payload.uptime_s;
    root["ota_busy"] = payload.ota_busy;

    ArduinoJson::JsonObject heap = root["heap"].to<ArduinoJson::JsonObject>();
    heap["free"] = payload.heap_free;
    heap["min_free"] = payload.heap_min_free;

    ArduinoJson::JsonObject boot = root["boot"].to<ArduinoJson::JsonObject>();
    boot["reset_reason"] = payload.boot.reset_reason;
    boot["auto_recovery_boot"] = payload.boot.auto_recovery_boot;
    boot["i2c_status"] = payload.boot.i2c_status;
    boot["sda_high"] = payload.boot.sda_high;
    boot["scl_high"] = payload.boot.scl_high;
    boot["board_ready"] = payload.boot.board_ready;
    boot["board_rounds"] = payload.boot.board_rounds;
    boot["board_begin_attempts"] = payload.boot.board_begin_attempts;
    boot["cold_power_start"] = payload.boot.cold_power_start;
    boot["cold_power_wait_ms"] = payload.boot.cold_power_wait_ms;
    boot["expander_probe_status"] = payload.boot.expander_probe_status;
    if (payload.boot.expander_probe_result_valid) {
        boot["expander_probe_attempts"] = payload.boot.expander_probe_attempts;
        boot["expander_probe_wait_ms"] = payload.boot.expander_probe_wait_ms;
        boot["expander_probe_error"] = payload.boot.expander_probe_error;
        boot["expander_probe_phase"] = payload.boot.expander_probe_phase;
        boot["expander_probe_failed_address"] = payload.boot.expander_probe_failed_address;
        boot["expander_probe_failed_value"] = payload.boot.expander_probe_failed_value;
        boot["expander_probe_bus_recoveries"] = payload.boot.expander_probe_bus_recoveries;
        boot["expander_probe_failure_lines_valid"] = payload.boot.expander_probe_failure_lines_valid;
        boot["expander_probe_failure_sda_high"] = payload.boot.expander_probe_failure_sda_high;
        boot["expander_probe_failure_scl_high"] = payload.boot.expander_probe_failure_scl_high;
        boot["expander_probe_recovery_sda_high"] = payload.boot.expander_probe_recovery_sda_high;
        boot["expander_probe_recovery_scl_high"] = payload.boot.expander_probe_recovery_scl_high;
        boot["expander_probe_recovery_pulses"] = payload.boot.expander_probe_recovery_pulses;
    } else {
        boot["expander_probe_attempts"] = nullptr;
        boot["expander_probe_wait_ms"] = nullptr;
        boot["expander_probe_error"] = nullptr;
        boot["expander_probe_phase"] = nullptr;
        boot["expander_probe_failed_address"] = nullptr;
        boot["expander_probe_failed_value"] = nullptr;
        boot["expander_probe_bus_recoveries"] = nullptr;
        boot["expander_probe_failure_lines_valid"] = nullptr;
        boot["expander_probe_failure_sda_high"] = nullptr;
        boot["expander_probe_failure_scl_high"] = nullptr;
        boot["expander_probe_recovery_sda_high"] = nullptr;
        boot["expander_probe_recovery_scl_high"] = nullptr;
        boot["expander_probe_recovery_pulses"] = nullptr;
    }
    boot["board_stage"] = payload.boot.board_stage;
    boot["board_failure"] = payload.boot.board_failure;
    boot["lvgl_ready"] = payload.boot.lvgl_ready;
    boot["previous_backlight_trace_status"] = payload.boot.previous_backlight_trace_status;
    if (payload.boot.previous_backlight_trace_valid) {
        ArduinoJson::JsonObject trace =
            boot["previous_backlight_trace"].to<ArduinoJson::JsonObject>();
        trace["event"] = payload.boot.previous_backlight_trace_event;
        trace["stage"] = payload.boot.previous_backlight_trace_stage;
        trace["driver_result"] = payload.boot.previous_backlight_trace_driver_result;
        trace["sequence"] = payload.boot.previous_backlight_trace_sequence;
        trace["uptime_ms"] = payload.boot.previous_backlight_trace_uptime_ms;
        trace["epoch_s"] = payload.boot.previous_backlight_trace_epoch_s;
        trace["driver_duration_us"] = payload.boot.previous_backlight_trace_driver_duration_us;
        trace["expected_network_manager_addr"] =
            payload.boot.previous_backlight_trace_expected_network_manager_addr;
        trace["post_backlight_network_manager_addr"] =
            payload.boot.previous_backlight_trace_post_backlight_network_manager_addr;
        trace["pre_render_network_manager_addr"] =
            payload.boot.previous_backlight_trace_pre_render_network_manager_addr;
        trace["post_backlight_task_handle"] =
            payload.boot.previous_backlight_trace_post_backlight_task_handle;
        trace["pre_render_task_handle"] =
            payload.boot.previous_backlight_trace_pre_render_task_handle;
        trace["target_on"] = payload.boot.previous_backlight_trace_target_on;
        trace["previous_on"] = payload.boot.previous_backlight_trace_previous_on;
        if (payload.boot.previous_backlight_trace_before_valid) {
            trace["before_sda_high"] = payload.boot.previous_backlight_trace_before_sda_high;
            trace["before_scl_high"] = payload.boot.previous_backlight_trace_before_scl_high;
        } else {
            trace["before_sda_high"] = nullptr;
            trace["before_scl_high"] = nullptr;
        }
        if (payload.boot.previous_backlight_trace_after_driver_valid) {
            trace["after_driver_sda_high"] =
                payload.boot.previous_backlight_trace_after_driver_sda_high;
            trace["after_driver_scl_high"] =
                payload.boot.previous_backlight_trace_after_driver_scl_high;
        } else {
            trace["after_driver_sda_high"] = nullptr;
            trace["after_driver_scl_high"] = nullptr;
        }
        if (payload.boot.previous_backlight_trace_after_probe_valid) {
            trace["after_probe_sda_high"] =
                payload.boot.previous_backlight_trace_after_probe_sda_high;
            trace["after_probe_scl_high"] =
                payload.boot.previous_backlight_trace_after_probe_scl_high;
        } else {
            trace["after_probe_sda_high"] = nullptr;
            trace["after_probe_scl_high"] = nullptr;
        }
    } else {
        boot["previous_backlight_trace"] = nullptr;
    }

    ArduinoJson::JsonObject network = root["network"].to<ArduinoJson::JsonObject>();
    WebNetworkUtils::fillDiagJson(network, payload.network);

    ArduinoJson::JsonArray last_errors = root["last_errors"].to<ArduinoJson::JsonArray>();
    root["error_count"] = WebEventsUtils::fillRecentErrorsJson(
        last_errors, recent_errors, recent_error_count, max_error_items);

    const WebTransferSnapshot &web_stream_snapshot = payload.web_stream;
    ArduinoJson::JsonObject web_stream = root["web_stream"].to<ArduinoJson::JsonObject>();
    web_stream["ok_count"] = web_stream_snapshot.stats.ok_count;
    web_stream["abort_count"] = web_stream_snapshot.stats.abort_count;
    web_stream["slow_count"] = web_stream_snapshot.stats.slow_count;
    web_stream["active_transfers"] = web_stream_snapshot.active_transfers;
    web_stream["mqtt_pause_remaining_ms"] = web_stream_snapshot.mqtt_pause_remaining_ms;
    web_stream["mqtt_connect_deferred_count"] = web_stream_snapshot.stats.mqtt_connect_deferred_count;
    web_stream["mqtt_publish_deferred_count"] = web_stream_snapshot.stats.mqtt_publish_deferred_count;
    web_stream["last_abort_reason"] = stream_abort_reason_text(web_stream_snapshot.stats.last_abort_reason);
    web_stream["last_errno"] = web_stream_snapshot.stats.last_errno;
    web_stream["last_sent"] = static_cast<uint32_t>(web_stream_snapshot.stats.last_sent);
    web_stream["last_total"] = static_cast<uint32_t>(web_stream_snapshot.stats.last_total);
    if (web_stream_snapshot.stats.last_total > 0) {
        web_stream["last_sent_ratio"] =
            static_cast<float>(web_stream_snapshot.stats.last_sent) /
            static_cast<float>(web_stream_snapshot.stats.last_total);
    } else {
        web_stream["last_sent_ratio"] = 1.0f;
    }
    web_stream["last_max_write_ms"] = web_stream_snapshot.stats.last_max_write_ms;
    web_stream["last_uri"] = web_stream_snapshot.stats.last_uri;
}

} // namespace WebDiagApiUtils
