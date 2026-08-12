// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace LvglWaitPolicy {

constexpr uint32_t VSYNC_WAIT_TIMEOUT_MS = 250;
constexpr int SCREEN_FLIP_LOCK_TIMEOUT_MS = 1000;
constexpr int DEINIT_LOCK_TIMEOUT_MS = 1000;

constexpr bool hasNewVsync(uint32_t baseline_count, uint32_t current_count) {
    return current_count != baseline_count;
}

constexpr uint32_t remainingWaitTicks(uint32_t start_tick,
                                      uint32_t now_tick,
                                      uint32_t timeout_ticks) {
    const uint32_t elapsed_ticks = now_tick - start_tick;
    return elapsed_ticks >= timeout_ticks
        ? 0
        : timeout_ticks - elapsed_ticks;
}

constexpr bool shouldLogVsyncTimeout(uint32_t timeout_count) {
    return timeout_count != 0 && ((timeout_count & (timeout_count - 1U)) == 0);
}

}  // namespace LvglWaitPolicy
