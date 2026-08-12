// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

#include "core/Ch422gReadyProbe.h"
#include "core/I2cBusRecovery.h"

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace BoardInit {

enum class Failure : uint8_t {
    None = 0,
    BusStuck,
    ExpanderNotReady,
    Allocation,
    Init,
    TaskCreate,
    Begin,
    Timeout,
};

enum class Stage : uint8_t {
    Bus = 0,
    Expander,
    Lcd,
    Touch,
    Backlight,
    Complete,
};

struct Result {
    esp_panel::board::Board *board = nullptr;
    Failure failure = Failure::None;
    Stage last_stage = Stage::Bus;
    uint8_t rounds = 0;
    uint8_t begin_attempts = 0;
    bool cold_power_start = false;
    uint32_t cold_power_wait_ms = 0;
    Ch422gReadyProbe::Result expander_probe{};
    I2cBusRecovery::Result last_recovery{};

    bool ready() const { return board != nullptr && failure == Failure::None; }
};

Result initBoard(const I2cBusRecovery::LineState &early_state,
                 const I2cBusRecovery::LineState &pre_init_state,
                 bool auto_recovery_boot);
void noteStage(Stage stage);
const char *failureText(Failure failure);
const char *stageText(Stage stage);

}
