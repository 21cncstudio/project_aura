// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/OtaDeferredRestart.h"

namespace OtaDeferredRestart {

bool deadline_reached(uint32_t now_ms, uint32_t due_ms) {
    return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

void Controller::reset() {
    deferred_restart_.store(false, std::memory_order_release);
    restart_requested_.store(false, std::memory_order_release);
    deferred_restart_due_ms_.store(0, std::memory_order_release);
}

void Controller::schedule(uint32_t now_ms, uint32_t delay_ms) {
    restart_requested_.store(false, std::memory_order_release);
    deferred_restart_due_ms_.store(now_ms + delay_ms, std::memory_order_release);
    deferred_restart_.store(true, std::memory_order_release);
}

bool Controller::poll(uint32_t now_ms) {
    if (!deferred_restart_.load(std::memory_order_acquire) ||
        !deadline_reached(now_ms, deferred_restart_due_ms_.load(std::memory_order_acquire))) {
        return false;
    }
    deferred_restart_.store(false, std::memory_order_release);
    deferred_restart_due_ms_.store(0, std::memory_order_release);
    restart_requested_.store(true, std::memory_order_release);
    return true;
}

bool Controller::consume_request() {
    return restart_requested_.exchange(false, std::memory_order_acq_rel);
}

bool Controller::is_busy(bool upload_active) const {
    return upload_active ||
           deferred_restart_.load(std::memory_order_acquire) ||
           restart_requested_.load(std::memory_order_acquire);
}

} // namespace OtaDeferredRestart
