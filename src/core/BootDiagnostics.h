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
    BoardInit::Stage board_stage = BoardInit::Stage::Bus;
    BoardInit::Failure board_failure = BoardInit::Failure::None;
    bool lvgl_ready = false;
};

extern Snapshot state;

} // namespace BootDiagnostics
