// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>

namespace LvglLockDiagnostics {

enum class Purpose { Runtime, StartupLogo };

struct Snapshot {
    uint32_t runtime_failures;
    uint32_t startup_logo_misses;
};

// Counts are retained for this boot. A startup retry must never clear or rebase
// failures from other callers, including runtime calls made during UI begin.
class Counters {
public:
    bool recordAttempt(Purpose purpose, bool acquired) {
        if (!acquired) {
            auto &counter = purpose == Purpose::StartupLogo
                ? startup_logo_misses_ : runtime_failures_;
            counter.fetch_add(1, std::memory_order_relaxed);
        }
        return acquired;
    }

    Snapshot snapshot() const {
        // These independent totals need not represent one atomic pair. The
        // counters carry no synchronization or mutex-ownership semantics.
        return {runtime_failures_.load(std::memory_order_relaxed),
                startup_logo_misses_.load(std::memory_order_relaxed)};
    }

private:
    std::atomic<uint32_t> runtime_failures_{0};
    std::atomic<uint32_t> startup_logo_misses_{0};
};

} // namespace LvglLockDiagnostics
