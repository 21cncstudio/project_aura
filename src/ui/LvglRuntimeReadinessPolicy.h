// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace LvglRuntimeReadinessPolicy {

constexpr uint32_t AGE_UNKNOWN_MS = UINT32_MAX;
constexpr uint32_t HANDLER_MAX_AGE_MS = 15000;
constexpr uint32_t VSYNC_MAX_AGE_MS = 5000;
constexpr uint32_t FLUSH_MAX_AGE_MS = 15000;

struct Diagnostics {
    uint32_t timer_handler_count = 0;
    uint32_t timer_handler_age_ms = AGE_UNKNOWN_MS;
    uint32_t flush_count = 0;
    uint32_t flush_age_ms = AGE_UNKNOWN_MS;
    uint32_t vsync_count = 0;
    uint32_t vsync_age_ms = AGE_UNKNOWN_MS;
    bool display_sync_fault = false;
    bool touch_offline = false;
    bool paused = false;
};

inline bool hasFreshActivity(uint32_t count,
                             uint32_t age_ms,
                             uint32_t max_age_ms) {
    return count > 0 &&
           age_ms != AGE_UNKNOWN_MS &&
           age_ms < max_age_ms;
}

inline bool isReady(bool lvgl_ready,
                    bool stall_active,
                    bool recovery_restart_requested,
                    bool recovery_suppressed,
                    const Diagnostics &diagnostics) {
    if (!lvgl_ready ||
        stall_active ||
        recovery_restart_requested ||
        recovery_suppressed ||
        diagnostics.display_sync_fault ||
        diagnostics.touch_offline ||
        diagnostics.paused) {
        return false;
    }

    return hasFreshActivity(diagnostics.timer_handler_count,
                            diagnostics.timer_handler_age_ms,
                            HANDLER_MAX_AGE_MS) &&
           diagnostics.flush_count > 0 &&
           hasFreshActivity(diagnostics.vsync_count,
                            diagnostics.vsync_age_ms,
                            VSYNC_MAX_AGE_MS);
}

}  // namespace LvglRuntimeReadinessPolicy
