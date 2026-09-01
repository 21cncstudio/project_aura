// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace Gt911Hardware {

// Runs the configured RESET/INT sequence through CH422G. Normal profiles probe
// only their configured address; dual-address reads are diagnostic-only.
bool selectConfiguredAddress(esp_panel::board::Board *board);

} // namespace Gt911Hardware
