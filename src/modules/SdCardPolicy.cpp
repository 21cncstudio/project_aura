// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modules/SdCardPolicy.h"

namespace SdCardPolicy {

State stateForMountOutcome(MountOutcome outcome) {
    switch (outcome) {
        case MountOutcome::Success: return State::Mounted;
        case MountOutcome::NoResponse: return State::NotDetected;
        case MountOutcome::Error:
        default: return State::Fault;
    }
}

const char *stateText(State state) {
    switch (state) {
        case State::NotAttempted: return "not_attempted";
        case State::NotDetected: return "not_detected";
        case State::Mounted: return "mounted";
        case State::BoardUnavailable: return "board_unavailable";
        case State::Fault: return "fault";
        default: return "unknown";
    }
}

bool isFault(State state) {
    return state == State::Fault;
}

bool mayResumeHistory(bool initial_mount, bool known_card, bool same_card,
                      bool history_present, bool session_ready) {
    if (initial_mount) return true; // Daily history restores before its first save.
    if (known_card && !same_card) return false;
    return (known_card && session_ready) || !history_present;
}

void Recovery::schedule(uint32_t now, uint32_t delay) {
    pending_ = true;
    scheduled_ms_ = now;
    delay_ms_ = delay;
}

void Recovery::ioFailure(DailyStorageFailureKind kind, uint32_t now) {
    if (pending_ || active_ || blocked_) return;
    if (kind != DailyStorageFailureKind::Io) {
        if (kind != DailyStorageFailureKind::Busy && kind != DailyStorageFailureKind::Missing)
            io_failures_ = 0;
        return;
    }
    if (++io_failures_ >= 2) schedule(now, 5000);
}

void Recovery::request(uint32_t now) {
    if (active_) return;
    blocked_ = false;
    mount_failures_ = 0;
    schedule(now, 0);
}

uint32_t Recovery::remainingMs(uint32_t now) const {
    if (!pending_) return 0;
    const uint32_t elapsed = now - scheduled_ms_;
    return elapsed >= delay_ms_ ? 0 : delay_ms_ - elapsed;
}

bool Recovery::start(uint32_t now) {
    if (!pending_ || active_ || remainingMs(now)) return false;
    pending_ = false;
    active_ = true;
    ++attempts_;
    return true;
}

void Recovery::defer(uint32_t now) {
    if (active_ && attempts_) --attempts_;
    active_ = false;
    schedule(now, 1000);
}

void Recovery::mountResult(bool ok, bool retryable, uint32_t now) {
    if (ok && active_) ++successes_;
    active_ = false;
    pending_ = false;
    blocked_ = !ok && !retryable;
    io_failures_ = 0;
    if (ok) {
        mount_failures_ = 0;
    } else if (retryable) {
        if (mount_failures_ < 4) ++mount_failures_;
        const uint32_t delays[] = {5000, 30000, 60000, 300000};
        schedule(now, delays[mount_failures_ - 1]);
    }
}

} // namespace SdCardPolicy
