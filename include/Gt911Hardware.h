// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace Gt911Hardware {

// Runs the board-specific reset/address-select sequence through CH422G.
// No transaction is sent to the conflicting default address 0x5D.
bool selectBackupAddress(esp_panel::board::Board *board);

} // namespace Gt911Hardware
