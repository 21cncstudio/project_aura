// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Logger.h"
#include "core/Gt911StartupDiagnostics.h"
#include "web/WebNetworkUtils.h"
#include "web/WebStreamState.h"

namespace WebDiagApiUtils {

struct DevicePayload {
    const char *firmware = "unknown";
    const char *build_id = "unknown";
    const char *hardware_profile = "unknown";
    const char *hardware_target = "unknown";
};

// Compile-time routing supplied by the handler, not a live bus measurement.
struct I2cBusPayload {
    int port = -1;
    int sda_gpio = -1;
    int scl_gpio = -1;
};

// Runtime LVGL/RGB observations. The refresh callback name is supplied by the
// firmware so a bounce-buffer completion is never presented as physical VSYNC.
struct DisplayPayload {
    bool available = false;
    uint32_t sample_ms = 0;
    uint32_t timer_handler_count = 0;
    uint32_t timer_handler_age_ms = UINT32_MAX;
    uint32_t flush_count = 0;
    uint32_t flush_age_ms = UINT32_MAX;
    const char *refresh_callback_semantics = "unknown";
    uint32_t refresh_callback_count = 0;
    uint32_t refresh_callback_age_ms = UINT32_MAX;
    uint32_t refresh_callback_max_gap_ms = 0;
    uint32_t framebuffer_handoff_count = 0;
    uint32_t framebuffer_wait_timeout_count = 0;
    bool display_sync_fault = false;
    uint32_t runtime_lock_failures = 0;
    uint32_t startup_logo_lock_misses = 0;
    uint32_t touch_read_errors = 0;
    bool touch_offline = false;
    bool screen_flip_180 = false;
    bool rotation_pipeline_active = false;
    uint32_t rotated_copy_switch_count = 0;
    uint32_t framebuffer_ownership_violation_count = 0;
    bool paused = false;
};

struct BootPayload {
    const char *reset_reason = "UNKNOWN";
    bool auto_recovery_boot = false;
    const char *i2c_status = "unknown";
    bool sda_high = false;
    bool scl_high = false;
    bool board_ready = false;
    uint8_t board_rounds = 0;
    uint8_t board_begin_attempts = 0;
    bool cold_power_start = false;
    uint32_t cold_power_wait_ms = 0;
    const char *expander_probe_status = "not_run";
    bool expander_probe_result_valid = false;
    uint16_t expander_probe_attempts = 0;
    uint32_t expander_probe_wait_ms = 0;
    int32_t expander_probe_error = 0;
    const char *expander_probe_phase = "not_run";
    uint8_t expander_probe_failed_address = 0;
    uint8_t expander_probe_failed_value = 0;
    uint16_t expander_probe_bus_recoveries = 0;
    bool expander_probe_failure_lines_valid = false;
    bool expander_probe_failure_sda_high = true;
    bool expander_probe_failure_scl_high = true;
    bool expander_probe_recovery_sda_high = true;
    bool expander_probe_recovery_scl_high = true;
    uint8_t expander_probe_recovery_pulses = 0;
    const char *board_stage = "bus";
    const char *board_failure = "none";
    bool lvgl_ready = false;
    Gt911StartupDiagnostics::Snapshot gt911_startup{};
    const char *previous_backlight_trace_status = "empty";
    bool previous_backlight_trace_valid = false;
    const char *previous_backlight_trace_event = "none";
    const char *previous_backlight_trace_stage = "none";
    const char *previous_backlight_trace_driver_result = "unknown";
    const char *previous_backlight_trace_command_result = "unknown";
    uint32_t previous_backlight_trace_sequence = 0;
    uint32_t previous_backlight_trace_uptime_ms = 0;
    uint32_t previous_backlight_trace_epoch_s = 0;
    uint32_t previous_backlight_trace_driver_duration_us = 0;
    uint32_t previous_backlight_trace_pre_quiet_elapsed_ms = 0;
    uint32_t previous_backlight_trace_pre_quiet_active_operations = 0;
    bool previous_backlight_trace_pre_quiet_wait_exceeded = false;
    uint32_t previous_backlight_trace_pre_quiet_wait_exceeded_active_operations = 0;
    bool previous_backlight_trace_pre_quiet_forced_by_timeout = false;
    bool previous_backlight_trace_retention_uncertain = false;
    uint32_t previous_backlight_trace_expected_network_manager_addr = 0;
    uint32_t previous_backlight_trace_post_backlight_network_manager_addr = 0;
    uint32_t previous_backlight_trace_pre_render_network_manager_addr = 0;
    uint32_t previous_backlight_trace_post_backlight_task_handle = 0;
    uint32_t previous_backlight_trace_pre_render_task_handle = 0;
    bool previous_backlight_trace_target_on = false;
    bool previous_backlight_trace_previous_on = false;
    bool previous_backlight_trace_before_valid = false;
    bool previous_backlight_trace_before_sda_high = false;
    bool previous_backlight_trace_before_scl_high = false;
    bool previous_backlight_trace_after_driver_valid = false;
    bool previous_backlight_trace_after_driver_sda_high = false;
    bool previous_backlight_trace_after_driver_scl_high = false;
    bool previous_backlight_trace_after_probe_valid = false;
    bool previous_backlight_trace_after_probe_sda_high = false;
    bool previous_backlight_trace_after_probe_scl_high = false;
};

struct Payload {
    DevicePayload device{};
    I2cBusPayload panel_i2c{};
    I2cBusPayload sensor_i2c{};
    bool sensor_i2c_shared_with_panel = false;
    DisplayPayload display{};
    uint32_t uptime_s = 0;
    bool ota_busy = false;
    uint32_t heap_free = 0;
    uint32_t heap_min_free = 0;
    WebNetworkUtils::Snapshot network{};
    WebTransferSnapshot web_stream{};
    BootPayload boot{};
};

bool accessAllowed(bool ap_mode, bool sta_connected);
void fillJson(ArduinoJson::JsonObject root,
              const Payload &payload,
              const Logger::RecentEntry *recent_errors,
              size_t recent_error_count,
              size_t max_error_items);

} // namespace WebDiagApiUtils
