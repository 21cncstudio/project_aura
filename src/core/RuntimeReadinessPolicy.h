// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace RuntimeReadinessPolicy {

bool operational(bool board_ready, bool lvgl_ready);
bool canConfirmOta(bool board_ready, bool lvgl_ready, bool lvgl_runtime_healthy);

} // namespace RuntimeReadinessPolicy
