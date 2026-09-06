// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/TouchWakePolicy.h"

namespace TouchWakePolicy {

uint8_t ErrorStreak::recordError(uint32_t now_ms) {
    if (count_ == 0 ||
        static_cast<uint32_t>(now_ms - last_error_ms_) > ERROR_STREAK_WINDOW_MS) {
        count_ = 1;
    } else if (count_ < UINT8_MAX) {
        ++count_;
    }
    last_error_ms_ = now_ms;
    return count_;
}

void ErrorStreak::reset() {
    last_error_ms_ = 0;
    count_ = 0;
}

void StateMachine::setEnabled(bool enabled, bool touch_released, uint32_t now_ms) {
    if (enabled == enabled_) {
        return;
    }

    enabled_ = enabled;
    pending_wake_ = false;
    fast_retry_pending_ = false;
    armed_ = enabled ? touch_released : true;
    last_probe_ms_ = enabled ? now_ms : 0;
}

bool StateMachine::shouldProbe(bool interrupt_gated,
                               bool interrupt_pending,
                               uint32_t now_ms) const {
    (void)interrupt_gated;
    if (!enabled_ || pending_wake_) {
        return false;
    }
    if (interrupt_pending) {
        return true;
    }
    return static_cast<uint32_t>(now_ms - last_probe_ms_) >=
           FALLBACK_PROBE_INTERVAL_MS;
}

void StateMachine::recordProbe(Sample sample,
                               uint32_t now_ms,
                               bool selected_by_fresh_interrupt) {
    if (!enabled_) {
        return;
    }

    last_probe_ms_ = now_ms;
    fast_retry_pending_ =
        selected_by_fresh_interrupt && sample == Sample::NoData;
    if (sample == Sample::Released) {
        armed_ = true;
    } else if (sample == Sample::Pressed && armed_) {
        pending_wake_ = true;
    }
}

bool StateMachine::takeFastRetry() {
    const bool pending = fast_retry_pending_;
    fast_retry_pending_ = false;
    return pending;
}

bool StateMachine::takePendingWake() {
    const bool pending = pending_wake_;
    pending_wake_ = false;
    return pending;
}

bool RecoveryStateMachine::canAttempt(uint32_t now_ms) const {
    if (!attempted_) {
        return true;
    }
    return static_cast<uint32_t>(now_ms - last_attempt_ms_) >= cooldownMs();
}

void RecoveryStateMachine::recordAttempt(bool read_succeeded, uint32_t now_ms) {
    attempted_ = true;
    last_attempt_ms_ = now_ms;
    if (attempts_ != UINT32_MAX) {
        ++attempts_;
    }

    if (read_succeeded) {
        if (successes_ != UINT32_MAX) {
            ++successes_;
        }
        fail_streak_ = 0;
        offline_ = false;
        return;
    }

    if (fail_streak_ != UINT8_MAX) {
        ++fail_streak_;
    }
    offline_ = true;
}

void RecoveryStateMachine::suspendUntilRetry() {
    offline_ = true;
}

void RecoveryStateMachine::reset() {
    attempted_ = false;
    offline_ = false;
    last_attempt_ms_ = 0;
    attempts_ = 0;
    successes_ = 0;
    fail_streak_ = 0;
}

uint32_t RecoveryStateMachine::cooldownMs() const {
    if (fail_streak_ == 0) {
        return RECOVERY_SUCCESS_COOLDOWN_MS;
    }
    uint8_t shift = fail_streak_ > 0 ? static_cast<uint8_t>(fail_streak_ - 1) : 0;
    if (shift > RECOVERY_MAX_BACKOFF_SHIFT) {
        shift = RECOVERY_MAX_BACKOFF_SHIFT;
    }

    uint32_t cooldown_ms = RECOVERY_COOLDOWN_MS << shift;
    if (cooldown_ms > RECOVERY_MAX_COOLDOWN_MS) {
        cooldown_ms = RECOVERY_MAX_COOLDOWN_MS;
    }
    return cooldown_ms;
}

} // namespace TouchWakePolicy
