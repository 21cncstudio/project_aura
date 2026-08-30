// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>

namespace BoardInitTaskLifecycle {

enum class State : uint8_t {
    Running = 0,
    Completed,
    CancelOwned,
    CompletionAcknowledged,
    DeleteReady,
};

enum class TimeoutClaim : uint8_t {
    CancelOwned = 0,
    CompletionOwned,
    Invalid,
};

// One-attempt ownership handshake between the board-init child and its parent.
// The child publishes completion with a CAS before notifying the parent. At the
// timeout boundary the parent performs the competing CAS. Therefore exactly
// one path owns the task: either timeout cancellation, or the two-phase
// completion acknowledgement which parks the child before parent deletion.
class Lifecycle {
public:
    Lifecycle() = default;
    Lifecycle(const Lifecycle &) = delete;
    Lifecycle &operator=(const Lifecycle &) = delete;

    bool childPublishCompletion() {
        State expected = State::Running;
        return state_.compare_exchange_strong(
            expected,
            State::Completed,
            std::memory_order_release,
            std::memory_order_relaxed);
    }

    TimeoutClaim parentClaimTimeout() {
        State expected = State::Running;
        if (state_.compare_exchange_strong(
                expected,
                State::CancelOwned,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return TimeoutClaim::CancelOwned;
        }
        return expected == State::Completed
            ? TimeoutClaim::CompletionOwned
            : TimeoutClaim::Invalid;
    }

    bool parentAcknowledgeCompletion() {
        State expected = State::Completed;
        return state_.compare_exchange_strong(
            expected,
            State::CompletionAcknowledged,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    bool childPublishDeleteReady() {
        State expected = State::CompletionAcknowledged;
        return state_.compare_exchange_strong(
            expected,
            State::DeleteReady,
            std::memory_order_release,
            std::memory_order_relaxed);
    }

    State state() const {
        return state_.load(std::memory_order_acquire);
    }

    bool parentOwnsDeletion() const {
        const State current = state();
        return current == State::CancelOwned ||
               current == State::DeleteReady;
    }

private:
    std::atomic<State> state_{State::Running};
};

} // namespace BoardInitTaskLifecycle
