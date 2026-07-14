// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

enum class AuraBoardInitStage : uint8_t {
    Bus = 0,
    Expander,
    Lcd,
    Touch,
    Backlight,
    Complete,
};

// Kept in the public include tree because ESP32_Display_Panel compiles the
// custom callbacks with the dependency's include paths, not the application's
// private src/ paths.
void auraBoardInitNoteStage(AuraBoardInitStage stage);
bool auraBoardInitStageResult(AuraBoardInitStage stage, bool success);
