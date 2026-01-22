#pragma once

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace BoardInit {
    esp_panel::board::Board *initBoard();
}
