// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include "modules/DailyHistoryStorage.h"

namespace SdCardPolicy {

enum class State : uint8_t {
    NotAttempted = 0,
    NotDetected,
    Mounted,
    BoardUnavailable,
    Fault,
};

enum class MountOutcome : uint8_t {
    Success = 0,
    NoResponse,
    Error,
};

State stateForMountOutcome(MountOutcome outcome);
const char *stateText(State state);
bool isFault(State state);
bool mayResumeHistory(bool initial_mount, bool known_card, bool same_card,
                      bool history_present, bool session_ready);

// Scheduling only; callers serialize access. Never performs I/O or formats media.
class Recovery {
public:
    void ioFailure(DailyStorageFailureKind kind, uint32_t now);
    void ioSuccess() { io_failures_ = 0; }
    void request(uint32_t now);
    void cancelPending() { pending_ = false; }
    bool start(uint32_t now);
    void defer(uint32_t now); // A file is still open, or task allocation failed.
    void mountResult(bool ok, bool retryable, uint32_t now);
    uint32_t remainingMs(uint32_t now) const;
    bool pending() const { return pending_; }
    bool active() const { return active_; }
    bool blocked() const { return blocked_; }
    uint32_t attempts() const { return attempts_; }
    uint32_t successes() const { return successes_; }
private:
    void schedule(uint32_t now, uint32_t delay);
    bool pending_ = false, active_ = false, blocked_ = false;
    uint8_t io_failures_ = 0, mount_failures_ = 0;
    uint32_t scheduled_ms_ = 0, delay_ms_ = 0;
    uint32_t attempts_ = 0, successes_ = 0;
};

} // namespace SdCardPolicy
