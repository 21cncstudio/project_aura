// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#include "esp_cpu.h"
#else
#include <atomic>
#endif

namespace TouchWakePolicy {

constexpr uint32_t FALLBACK_PROBE_INTERVAL_MS = 2500;
constexpr uint32_t ERROR_STREAK_WINDOW_MS = FALLBACK_PROBE_INTERVAL_MS + 1000U;
constexpr uint32_t BOOT_QUIET_COLD_MS = 5000;
constexpr uint32_t BOOT_QUIET_WARM_MS = 10000;
constexpr uint32_t RECOVERY_SUCCESS_COOLDOWN_MS = 5000;
constexpr uint32_t RECOVERY_COOLDOWN_MS = 90UL * 1000UL;
constexpr uint8_t RECOVERY_MAX_BACKOFF_SHIFT = 4;
constexpr uint32_t RECOVERY_MAX_COOLDOWN_MS = 8UL * 60UL * 1000UL;

// Coalesces touch IRQs without adding a second Project Aura FreeRTOS semaphore
// to the vendor interrupt path. On ESP32-S3 the one-shot compare-and-set is a
// bounded S32C1I operation in internal RAM.
class InterruptLatch {
public:
#if defined(ESP_PLATFORM)
    __attribute__((always_inline)) void signal() noexcept {
        (void)esp_cpu_compare_and_set(&pending_, 0U, 1U);
    }

    __attribute__((always_inline)) bool take() noexcept {
        return esp_cpu_compare_and_set(&pending_, 1U, 0U);
    }

    __attribute__((always_inline)) void clear() noexcept {
        (void)esp_cpu_compare_and_set(&pending_, 1U, 0U);
    }

private:
    alignas(uint32_t) volatile uint32_t pending_ = 0U;
#else
    void signal() noexcept {
        pending_.store(1U, std::memory_order_release);
    }

    bool take() noexcept {
        return pending_.exchange(0U, std::memory_order_acq_rel) != 0U;
    }

    void clear() noexcept {
        pending_.store(0U, std::memory_order_release);
    }

private:
    alignas(uint32_t) std::atomic<uint32_t> pending_{0U};
#endif
};

enum class Sample : uint8_t {
    Error = 0,
    NoData,
    Released,
    Pressed,
};

constexpr uint32_t bootQuietWindowMs(bool cold_start) {
    return cold_start ? BOOT_QUIET_COLD_MS : BOOT_QUIET_WARM_MS;
}

constexpr bool interruptLineActive(bool configured_active_high,
                                   int sampled_level) {
    return sampled_level == (configured_active_high ? 1 : 0);
}

class StateMachine {
public:
    void setEnabled(bool enabled, bool touch_released, uint32_t now_ms);
    bool isEnabled() const { return enabled_; }

    bool shouldProbe(bool interrupt_gated,
                     bool interrupt_pending,
                     uint32_t now_ms) const;
    void recordProbe(Sample sample,
                     uint32_t now_ms,
                     bool selected_by_fresh_interrupt = false);

    // A GT911 edge can precede the ready bit. Consume at most one deferred
    // callback-speed retry for that edge before returning to sparse probing.
    bool takeFastRetry();

    bool hasPendingWake() const { return pending_wake_; }
    bool takePendingWake();

private:
    bool enabled_ = false;
    bool armed_ = true;
    bool pending_wake_ = false;
    bool fast_retry_pending_ = false;
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
