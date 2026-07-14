// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/RuntimeReadinessPolicy.h"

namespace RuntimeReadinessPolicy {

bool operational(bool board_ready, bool lvgl_ready) {
    return board_ready && lvgl_ready;
}

bool canConfirmOta(bool board_ready, bool lvgl_ready) {
    return operational(board_ready, lvgl_ready);
}

} // namespace RuntimeReadinessPolicy
