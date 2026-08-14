// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>
#include <stdint.h>

namespace BacklightStatePolicy {

struct Transition {
    bool actual_on;
    bool wake_probe_enabled;
    bool command_succeeded;
};

struct WakeProbePlan {
    bool driver_call_required;
    bool mask_before_driver;
};

static constexpr uint32_t RETRY_BASE_MS = 500;
static constexpr uint32_t RETRY_MAX_MS = 8000;

inline Transition resolve(bool current_on, bool requested_on, bool driver_succeeded) {
    const bool actual_on = driver_succeeded ? requested_on : current_on;
    return {actual_on, !actual_on, driver_succeeded};
}

// Every real backlight transition crosses the shared CH422G bus. Keep the
// touch interrupt physically masked across that call even when the logical
// wake policy already says that it should be disabled.
inline WakeProbePlan planWakeProbe(bool current_on, bool requested_on) {
    const bool driver_call_required = current_on != requested_on;
    return {driver_call_required, driver_call_required};
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

inline bool beforeDeadline(uint32_t now_ms, uint32_t deadline_ms) {
    return deadline_ms != 0 &&
           static_cast<int32_t>(now_ms - deadline_ms) < 0;
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

class RuntimeGate {
public:
    class Access {
    public:
        Access() = default;
        Access(const Access &) = delete;
        Access &operator=(const Access &) = delete;

        Access(Access &&other) noexcept : gate_(other.gate_) {
            other.gate_ = nullptr;
        }

        Access &operator=(Access &&other) noexcept {
            if (this == &other) {
                return *this;
            }
            release();
            gate_ = other.gate_;
            other.gate_ = nullptr;
            return *this;
        }

        ~Access() { release(); }

        explicit operator bool() const { return gate_ != nullptr; }

    private:
        friend class RuntimeGate;

        explicit Access(RuntimeGate *gate) : gate_(gate) {}

        void release() {
            if (!gate_) {
                return;
            }
            gate_->releaseAccess();
            gate_ = nullptr;
        }

        RuntimeGate *gate_ = nullptr;
    };

    RuntimeGate() = default;
    RuntimeGate(const RuntimeGate &) = delete;
    RuntimeGate &operator=(const RuntimeGate &) = delete;

    bool available() const {
        return available_.load(std::memory_order_acquire);
    }

    Access acquire() {
        if (!available_.load(std::memory_order_acquire)) {
            return Access{};
        }

        active_users_.fetch_add(1U, std::memory_order_acq_rel);
        if (!available_.load(std::memory_order_acquire)) {
            active_users_.fetch_sub(1U, std::memory_order_acq_rel);
            return Access{};
        }
        return Access{this};
    }

    bool disable() {
        // Close admission immediately. A caller which raced between its first
        // availability check and incrementing the counter will fail the second
        // check and release its provisional count without running an operation.
        return available_.exchange(false, std::memory_order_acq_rel);
    }

    bool idle() const {
        return active_users_.load(std::memory_order_acquire) == 0U;
    }

    uint32_t activeUsers() const {
        return active_users_.load(std::memory_order_acquire);
    }

private:
    void releaseAccess() {
        active_users_.fetch_sub(1U, std::memory_order_release);
    }

    std::atomic<bool> available_{true};
    std::atomic<uint32_t> active_users_{0};
};

inline bool drainWaitExpired(uint32_t start_ms,
                             uint32_t now_ms,
                             uint32_t timeout_ms) {
    return static_cast<uint32_t>(now_ms - start_ms) >= timeout_ms;
}

inline bool clearPendingAfterDrain(const RuntimeGate &gate,
                                   PendingCommand &pending_command) {
    // Finalization is safe only after admission was permanently closed and
    // every operation admitted before that close has released its lease.
    if (gate.available() || !gate.idle()) {
        return false;
    }
    pending_command.clear();
    return true;
}

inline bool inputActivitySince(uint32_t previous_inactive_ms, uint32_t inactive_ms) {
    return previous_inactive_ms != 0 && inactive_ms < previous_inactive_ms;
}

} // namespace BacklightStatePolicy
