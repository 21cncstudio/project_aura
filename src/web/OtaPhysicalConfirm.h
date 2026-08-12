// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace OtaPhysicalConfirm {

constexpr uint32_t kPendingTimeoutMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kAllowedTimeoutMs = 60UL * 1000UL;
constexpr uint32_t kRetryAfterMs = 1000UL;
constexpr uint32_t kTerminalRetentionMs = 5UL * 1000UL;

enum class State : uint8_t {
    Idle = 0,
    Pending,
    Allowed,
    Denied,
    Expired,
    Consumed,
};

enum class PrepareStatus : uint8_t {
    Ready = 0,
    Required,
    Busy,
    Denied,
    Expired,
    Mismatch,
};

enum class ConsumeStatus : uint8_t {
    Consumed = 0,
    Missing,
    NotAllowed,
    Denied,
    Expired,
    Mismatch,
};

struct Snapshot {
    State state = State::Idle;
    uint32_t confirm_id = 0;
    size_t expected_size = 0;
    uint32_t created_ms = 0;
    uint32_t pending_until_ms = 0;
    uint32_t allowed_until_ms = 0;
};

struct PrepareDecision {
    PrepareStatus status = PrepareStatus::Required;
    uint32_t confirm_id = 0;
    uint32_t retry_after_ms = kRetryAfterMs;
    uint32_t confirm_timeout_ms = kPendingTimeoutMs;
};

struct ConsumeDecision {
    ConsumeStatus status = ConsumeStatus::Missing;
    uint32_t confirm_id = 0;
};

class StateMachine {
public:
    void reset();
    Snapshot snapshot() const;
    PrepareDecision prepare(size_t expected_size,
                            bool has_confirm_id,
                            uint32_t confirm_id,
                            uint32_t now_ms);
    ConsumeDecision consumeForUpload(size_t expected_size,
                                     bool has_confirm_id,
                                     uint32_t confirm_id,
                                     uint32_t now_ms);
    bool allowCurrent(uint32_t expected_confirm_id, uint32_t now_ms);
    bool denyCurrent(uint32_t expected_confirm_id, uint32_t now_ms);
    uint32_t pollExpired(uint32_t now_ms);

private:
    bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) const;
    bool isInactiveForNewRequest(uint32_t now_ms) const;
    void expireIfDue(uint32_t now_ms);
    PrepareDecision createPending(size_t expected_size, uint32_t now_ms);
    bool matches(uint32_t confirm_id, size_t expected_size) const;
    void enterTerminal(State state, uint32_t now_ms);
    void noteTerminalObserved();
    void recordTerminal(State state, uint32_t now_ms);
    bool terminalRecordMatches(uint32_t confirm_id, size_t expected_size, uint32_t now_ms) const;

    State state_ = State::Idle;
    uint32_t next_confirm_id_ = 1;
    uint32_t confirm_id_ = 0;
    size_t expected_size_ = 0;
    uint32_t created_ms_ = 0;
    uint32_t pending_until_ms_ = 0;
    uint32_t allowed_until_ms_ = 0;
    uint32_t terminal_until_ms_ = 0;
    bool terminal_observed_ = false;
    uint32_t expired_event_confirm_id_ = 0;
    State terminal_state_ = State::Idle;
    uint32_t terminal_confirm_id_ = 0;
    size_t terminal_expected_size_ = 0;
    uint32_t terminal_record_until_ms_ = 0;
};

void reset();
Snapshot snapshot();
PrepareDecision prepare(size_t expected_size, bool has_confirm_id, uint32_t confirm_id);
ConsumeDecision consumeForUpload(size_t expected_size, bool has_confirm_id, uint32_t confirm_id);
bool allowCurrent(uint32_t expected_confirm_id);
bool denyCurrent(uint32_t expected_confirm_id);
uint32_t pollExpired();

const char *stateText(State state);
const char *prepareStatusText(PrepareStatus status);
const char *consumeStatusText(ConsumeStatus status);

}  // namespace OtaPhysicalConfirm
