// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace LvglWaitPolicy {

constexpr uint32_t VSYNC_WAIT_TIMEOUT_MS = 250;
constexpr int SCREEN_FLIP_LOCK_TIMEOUT_MS = 1000;

constexpr bool hasNewVsync(uint32_t baseline_count, uint32_t current_count) {
    return current_count != baseline_count;
}

constexpr bool shouldLogVsyncTimeout(uint32_t timeout_count) {
    return timeout_count != 0 && ((timeout_count & (timeout_count - 1U)) == 0);
}

}  // namespace LvglWaitPolicy
