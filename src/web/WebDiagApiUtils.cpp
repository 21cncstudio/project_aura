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

    ArduinoJson::JsonObject device = root["device"].to<ArduinoJson::JsonObject>();
    device["firmware"] = payload.device.firmware;
    device["build_id"] = payload.device.build_id;
    device["hardware_profile"] = payload.device.hardware_profile;
    device["hardware_target"] = payload.device.hardware_target;

    ArduinoJson::JsonObject i2c_buses = root["i2c_buses"].to<ArduinoJson::JsonObject>();
    ArduinoJson::JsonObject panel = i2c_buses["panel"].to<ArduinoJson::JsonObject>();
    panel["port"] = payload.panel_i2c.port;
    panel["sda_gpio"] = payload.panel_i2c.sda_gpio;
    panel["scl_gpio"] = payload.panel_i2c.scl_gpio;
    ArduinoJson::JsonObject sensors = i2c_buses["sensors"].to<ArduinoJson::JsonObject>();
    sensors["port"] = payload.sensor_i2c.port;
    sensors["sda_gpio"] = payload.sensor_i2c.sda_gpio;
    sensors["scl_gpio"] = payload.sensor_i2c.scl_gpio;
    sensors["shared_with_panel"] = payload.sensor_i2c_shared_with_panel;

    ArduinoJson::JsonObject display = root["display"].to<ArduinoJson::JsonObject>();
    display["available"] = payload.display.available;
    if (payload.display.available) {
        display["sample_ms"] = payload.display.sample_ms;
        display["timer_handler_count"] = payload.display.timer_handler_count;
        display["timer_handler_age_ms"] = payload.display.timer_handler_age_ms;
        display["flush_count"] = payload.display.flush_count;
        display["flush_age_ms"] = payload.display.flush_age_ms;
        display["refresh_callback_semantics"] =
            payload.display.refresh_callback_semantics;
        display["refresh_callback_count"] =
            payload.display.refresh_callback_count;
        display["refresh_callback_age_ms"] =
            payload.display.refresh_callback_age_ms;
        display["refresh_callback_max_gap_ms"] =
            payload.display.refresh_callback_max_gap_ms;
        display["framebuffer_handoff_count"] =
            payload.display.framebuffer_handoff_count;
        display["framebuffer_wait_timeout_count"] =
            payload.display.framebuffer_wait_timeout_count;
        display["display_sync_fault"] = payload.display.display_sync_fault;
        display["runtime_lock_failures"] =
            payload.display.runtime_lock_failures;
        display["startup_logo_lock_misses"] =
            payload.display.startup_logo_lock_misses;
        display["touch_read_errors"] = payload.display.touch_read_errors;
        display["touch_offline"] = payload.display.touch_offline;
        ArduinoJson::JsonObject touch_polling =
            display["touch_polling"].to<ArduinoJson::JsonObject>();
        touch_polling["mode"] = payload.display.touch_polling.mode;
        touch_polling["irq_registered"] =
            payload.display.touch_polling.irq_registered;
        touch_polling["irq_armed"] = payload.display.touch_polling.irq_armed;
        touch_polling["irq_config_verified"] =
            payload.display.touch_polling.irq_config_verified;
        if (payload.display.touch_polling.irq_config_mode < 0) {
            touch_polling["irq_config_mode"] = nullptr;
        } else {
            touch_polling["irq_config_mode"] =
                payload.display.touch_polling.irq_config_mode;
        }
        touch_polling["idle_enabled"] =
            payload.display.touch_polling.idle_enabled;
        touch_polling["idle_active"] =
            payload.display.touch_polling.idle_active;
        touch_polling["fail_safe"] = payload.display.touch_polling.fail_safe;
        touch_polling["status_reads"] =
            payload.display.touch_polling.status_reads;
        touch_polling["full_reads"] =
            payload.display.touch_polling.full_reads;
        touch_polling["skipped_callbacks"] =
            payload.display.touch_polling.skipped_callbacks;
        touch_polling["idle_entries"] =
            payload.display.touch_polling.idle_entries;
        touch_polling["irq_exits"] = payload.display.touch_polling.irq_exits;
        touch_polling["fallback_probes"] =
            payload.display.touch_polling.fallback_probes;
        touch_polling["missed_irq_presses"] =
            payload.display.touch_polling.missed_irq_presses;
        touch_polling["irq_arm_failures"] =
            payload.display.touch_polling.irq_arm_failures;
        touch_polling["irq_no_frame"] =
            payload.display.touch_polling.irq_no_frame;
        display["screen_flip_180"] = payload.display.screen_flip_180;
        display["rotation_pipeline_active"] =
            payload.display.rotation_pipeline_active;
        display["rotated_copy_switch_count"] =
            payload.display.rotated_copy_switch_count;
        display["framebuffer_ownership_violation_count"] =
            payload.display.framebuffer_ownership_violation_count;
        display["paused"] = payload.display.paused;
    }

    ArduinoJson::JsonObject heap = root["heap"].to<ArduinoJson::JsonObject>();
    heap["free"] = payload.heap_free;
    heap["min_free"] = payload.heap_min_free;

    ArduinoJson::JsonObject boot = root["boot"].to<ArduinoJson::JsonObject>();
    boot["reset_reason"] = payload.boot.reset_reason;
    boot["auto_recovery_boot"] = payload.boot.auto_recovery_boot;
    boot["i2c_status"] = payload.boot.i2c_status;
    boot["sda_high"] = payload.boot.sda_high;
    boot["scl_high"] = payload.boot.scl_high;
    // Keep the raw legacy values above; describe their historical origin separately.
    ArduinoJson::JsonObject i2c_snapshot = boot["i2c_snapshot"].to<ArduinoJson::JsonObject>();
    i2c_snapshot["phase"] = "before_board_init";
    i2c_snapshot["bus"] = "panel";
    i2c_snapshot["port"] = payload.panel_i2c.port;
    i2c_snapshot["sda_gpio"] = payload.panel_i2c.sda_gpio;
    i2c_snapshot["scl_gpio"] = payload.panel_i2c.scl_gpio;
    i2c_snapshot["live"] = false;
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
    boot["previous_backlight_trace_retention_uncertain"] =
        payload.boot.previous_backlight_trace_retention_uncertain;
    if (payload.boot.previous_backlight_trace_valid) {
        ArduinoJson::JsonObject trace =
            boot["previous_backlight_trace"].to<ArduinoJson::JsonObject>();
        trace["event"] = payload.boot.previous_backlight_trace_event;
        trace["stage"] = payload.boot.previous_backlight_trace_stage;
        trace["driver_result"] = payload.boot.previous_backlight_trace_driver_result;
        trace["command_result"] = payload.boot.previous_backlight_trace_command_result;
        trace["sequence"] = payload.boot.previous_backlight_trace_sequence;
        trace["uptime_ms"] = payload.boot.previous_backlight_trace_uptime_ms;
        trace["epoch_s"] = payload.boot.previous_backlight_trace_epoch_s;
        trace["driver_duration_us"] = payload.boot.previous_backlight_trace_driver_duration_us;
        trace["pre_quiet_elapsed_ms"] =
            payload.boot.previous_backlight_trace_pre_quiet_elapsed_ms;
        trace["pre_quiet_active_operations"] =
            payload.boot.previous_backlight_trace_pre_quiet_active_operations;
        trace["pre_quiet_wait_exceeded"] =
            payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded;
        trace["pre_quiet_wait_exceeded_active_operations"] =
            payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded_active_operations;
        trace["pre_quiet_forced_by_timeout"] =
            payload.boot.previous_backlight_trace_pre_quiet_forced_by_timeout;
        trace["retention_uncertain"] =
            payload.boot.previous_backlight_trace_retention_uncertain;
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
