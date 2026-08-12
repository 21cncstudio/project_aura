// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/RuntimeReadinessPolicy.h"

namespace RuntimeReadinessPolicy {

bool operational(bool board_ready, bool lvgl_ready) {
    return board_ready && lvgl_ready;
}

bool canConfirmOta(bool board_ready, bool lvgl_ready, bool lvgl_runtime_healthy) {
    return operational(board_ready, lvgl_ready) && lvgl_runtime_healthy;
}

bool canManageLvglRuntime(bool shared_i2c_ready,
                          bool lvgl_ready,
                          bool ui_lvgl_ready) {
    // A permanently disabled shared I2C bus also disables touch. Never resume
    // a quiesced LVGL task after that transition, even if the port and UI were
    // initialized successfully earlier in the boot.
    return shared_i2c_ready && lvgl_ready && ui_lvgl_ready;
}

} // namespace RuntimeReadinessPolicy
