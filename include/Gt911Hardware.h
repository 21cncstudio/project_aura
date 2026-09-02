// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace Gt911Hardware {

// Runs the configured RESET/INT sequence through CH422G. Each production
// profile selects and uses only its configured address.
bool selectConfiguredAddress(esp_panel::board::Board *board);

} // namespace Gt911Hardware
