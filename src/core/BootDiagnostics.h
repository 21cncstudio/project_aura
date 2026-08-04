// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <esp_system.h>
#include <stdint.h>

#include "core/BoardInit.h"

namespace BootDiagnostics {

struct Snapshot {
    esp_reset_reason_t reset_reason = ESP_RST_UNKNOWN;
    bool auto_recovery_boot = false;
    I2cBusRecovery::Status i2c_status = I2cBusRecovery::Status::BothStuckLow;
    bool sda_high = false;
    bool scl_high = false;
    bool board_ready = false;
    uint8_t board_rounds = 0;
    uint8_t board_begin_attempts = 0;
    bool cold_power_start = false;
    uint32_t cold_power_wait_ms = 0;
    Ch422gReadyProbe::Status expander_probe_status = Ch422gReadyProbe::Status::NotRun;
    uint16_t expander_probe_attempts = 0;
    uint32_t expander_probe_wait_ms = 0;
    int32_t expander_probe_error = ESP_OK;
    Ch422gReadyProbe::Phase expander_probe_phase = Ch422gReadyProbe::Phase::NotRun;
    uint8_t expander_probe_failed_address = 0;
    uint8_t expander_probe_failed_value = 0;
    uint16_t expander_probe_bus_recoveries = 0;
    bool expander_probe_failure_lines_valid = false;
    bool expander_probe_failure_sda_high = true;
    bool expander_probe_failure_scl_high = true;
    bool expander_probe_recovery_sda_high = true;
    bool expander_probe_recovery_scl_high = true;
    uint8_t expander_probe_recovery_pulses = 0;
    BoardInit::Stage board_stage = BoardInit::Stage::Bus;
    BoardInit::Failure board_failure = BoardInit::Failure::None;
    bool lvgl_ready = false;
};

extern Snapshot state;

} // namespace BootDiagnostics
