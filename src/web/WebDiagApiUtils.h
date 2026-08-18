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
#include "web/WebNetworkUtils.h"
#include "web/WebStreamState.h"

namespace WebDiagApiUtils {

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
