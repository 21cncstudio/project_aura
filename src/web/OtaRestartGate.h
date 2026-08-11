// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>
#include <stdint.h>

// Serializes OTA upload admission with controlled restart shutdown across the
// HTTP and main-loop tasks. Exactly one side may leave Idle.
class OtaRestartGate {
public:
    enum class State : uint8_t {
        Idle = 0,
        Upload,
        Restart,
    };

    void reset() {
        state_.store(State::Idle, std::memory_order_release);
    }

    bool tryBeginUpload() {
        State expected = State::Idle;
        return state_.compare_exchange_strong(expected,
                                              State::Upload,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
    }

    void endUpload() {
        State expected = State::Upload;
        (void)state_.compare_exchange_strong(expected,
                                             State::Idle,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
    }

    bool tryBeginRestart() {
        State expected = State::Idle;
        return state_.compare_exchange_strong(expected,
                                              State::Restart,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
    }

    bool uploadActive() const {
        return state_.load(std::memory_order_acquire) == State::Upload;
    }

    bool busy() const {
        return state_.load(std::memory_order_acquire) != State::Idle;
    }

    State state() const {
        return state_.load(std::memory_order_acquire);
    }

private:
    std::atomic<State> state_{State::Idle};
};
