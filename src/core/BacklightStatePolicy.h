// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

namespace BacklightStatePolicy {

struct Transition {
    bool actual_on;
    bool wake_probe_enabled;
    bool command_succeeded;
};

static constexpr uint32_t RETRY_BASE_MS = 500;
static constexpr uint32_t RETRY_MAX_MS = 8000;

inline Transition resolve(bool current_on, bool requested_on, bool driver_succeeded) {
    const bool actual_on = driver_succeeded ? requested_on : current_on;
    return {actual_on, !actual_on, driver_succeeded};
}

inline uint32_t retryDelayMs(uint8_t consecutive_failures) {
    if (consecutive_failures == 0) {
        return 0;
    }
    const uint8_t shift = consecutive_failures > 5 ? 4 : consecutive_failures - 1;
    const uint32_t delay_ms = RETRY_BASE_MS << shift;
    return delay_ms > RETRY_MAX_MS ? RETRY_MAX_MS : delay_ms;
}

inline bool retryReady(uint32_t now_ms, uint32_t retry_not_before_ms, bool retry_pending) {
    return !retry_pending ||
           static_cast<int32_t>(now_ms - retry_not_before_ms) >= 0;
}

class PendingCommand {
public:
    bool active() const { return active_; }
    bool targetOn() const { return target_on_; }
    uint8_t consecutiveFailures() const { return consecutive_failures_; }
    uint32_t retryNotBeforeMs() const { return retry_not_before_ms_; }

    void clear() {
        active_ = false;
        consecutive_failures_ = 0;
        retry_not_before_ms_ = 0;
    }

    void recordFailure(bool target_on, uint32_t now_ms) {
        if (!active_ || target_on_ != target_on) {
            consecutive_failures_ = 0;
        }
        active_ = true;
        target_on_ = target_on;
        if (consecutive_failures_ < UINT8_MAX) {
            ++consecutive_failures_;
        }
        retry_not_before_ms_ = now_ms + retryDelayMs(consecutive_failures_);
    }

    bool ready(uint32_t now_ms) const {
        return active_ && retryReady(now_ms, retry_not_before_ms_, true);
    }

private:
    bool active_ = false;
    bool target_on_ = true;
    uint8_t consecutive_failures_ = 0;
    uint32_t retry_not_before_ms_ = 0;
};

inline bool inputActivitySince(uint32_t previous_inactive_ms, uint32_t inactive_ms) {
    return previous_inactive_ms != 0 && inactive_ms < previous_inactive_ms;
}

} // namespace BacklightStatePolicy
