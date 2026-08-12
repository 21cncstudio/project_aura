// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <cstddef>
#include <cstdint>

namespace StartupProbePolicy {

constexpr uint32_t kAttemptOffsetsMs[] = {
    0U,
    1000U,
    3000U,
    10000U,
    30000U,
};

constexpr uint8_t kMaxAttempts =
    static_cast<uint8_t>(sizeof(kAttemptOffsetsMs) / sizeof(kAttemptOffsetsMs[0]));

constexpr bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

class State {
public:
    void reset(uint32_t start_ms) {
        start_ms_ = start_ms;
        attempts_ = 0;
        initialized_ = true;
        succeeded_ = false;
    }

    bool shouldAttempt(uint32_t now_ms) const {
        return pending() && deadlineReached(now_ms, nextDueMs());
    }

    void recordAttempt(bool success) {
        if (!pending()) {
            return;
        }
        ++attempts_;
        succeeded_ = success;
    }

    bool initialized() const { return initialized_; }
    bool succeeded() const { return succeeded_; }
    bool exhausted() const {
        return initialized_ && !succeeded_ && attempts_ >= kMaxAttempts;
    }
    bool pending() const {
        return initialized_ && !succeeded_ && attempts_ < kMaxAttempts;
    }
    uint8_t attempts() const { return attempts_; }

    uint32_t nextDueMs() const {
        const uint8_t index = attempts_ < kMaxAttempts
                                  ? attempts_
                                  : static_cast<uint8_t>(kMaxAttempts - 1U);
        return start_ms_ + kAttemptOffsetsMs[index];
    }

private:
    uint32_t start_ms_ = 0;
    uint8_t attempts_ = 0;
    bool initialized_ = false;
    bool succeeded_ = false;
};

} // namespace StartupProbePolicy
