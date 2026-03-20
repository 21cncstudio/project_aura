// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>
#include <stdint.h>

namespace OtaDeferredRestart {

// Unsigned wrap-safe deadline comparison for millis()-style counters.
bool deadline_reached(uint32_t now_ms, uint32_t due_ms);

class Controller {
public:
    void reset();
    void schedule(uint32_t now_ms, uint32_t delay_ms);
    // Returns true when restart request transitions to "requested" in this poll.
    bool poll(uint32_t now_ms);
    bool consume_request();
    bool is_busy(bool upload_active) const;

    bool is_scheduled() const { return deferred_restart_; }
    bool is_requested() const { return restart_requested_; }
    uint32_t due_ms() const { return deferred_restart_due_ms_; }

private:
    std::atomic<bool> deferred_restart_{false};
    std::atomic<bool> restart_requested_{false};
    std::atomic<uint32_t> deferred_restart_due_ms_{0};
};

} // namespace OtaDeferredRestart
