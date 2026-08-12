// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace TouchWakePolicy {

constexpr uint32_t FALLBACK_PROBE_INTERVAL_MS = 2500;
constexpr uint32_t ERROR_STREAK_WINDOW_MS = FALLBACK_PROBE_INTERVAL_MS + 1000U;
constexpr uint32_t RECOVERY_SUCCESS_COOLDOWN_MS = 5000;
constexpr uint32_t RECOVERY_COOLDOWN_MS = 90UL * 1000UL;
constexpr uint8_t RECOVERY_MAX_BACKOFF_SHIFT = 4;
constexpr uint32_t RECOVERY_MAX_COOLDOWN_MS = 8UL * 60UL * 1000UL;

enum class Sample : uint8_t {
    Error = 0,
    Released,
    Pressed,
};

class StateMachine {
public:
    void setEnabled(bool enabled, bool touch_released, uint32_t now_ms);
    bool isEnabled() const { return enabled_; }

    bool shouldProbe(bool interrupt_gated,
                     bool interrupt_pending,
                     uint32_t now_ms) const;
    void recordProbe(Sample sample, uint32_t now_ms);

    bool hasPendingWake() const { return pending_wake_; }
    bool takePendingWake();

private:
    bool enabled_ = false;
    bool armed_ = true;
    bool pending_wake_ = false;
    uint32_t last_probe_ms_ = 0;
};

class ErrorStreak {
public:
    uint8_t recordError(uint32_t now_ms);
    void reset();
    uint8_t count() const { return count_; }

private:
    uint32_t last_error_ms_ = 0;
    uint8_t count_ = 0;
};

class RecoveryStateMachine {
public:
    bool canAttempt(uint32_t now_ms) const;
    void recordAttempt(bool read_succeeded, uint32_t now_ms);
    void suspendUntilRetry();
    void reset();

    bool isOffline() const { return offline_; }
    uint32_t attempts() const { return attempts_; }
    uint32_t successes() const { return successes_; }
    uint8_t failStreak() const { return fail_streak_; }
    uint32_t cooldownMs() const;

private:
    bool attempted_ = false;
    bool offline_ = false;
    uint32_t last_attempt_ms_ = 0;
    uint32_t attempts_ = 0;
    uint32_t successes_ = 0;
    uint8_t fail_streak_ = 0;
};

} // namespace TouchWakePolicy
