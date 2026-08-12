// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/OtaPhysicalConfirm.h"

#include <Arduino.h>

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace OtaPhysicalConfirm {

namespace {

#ifndef UNIT_TEST
portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

class CriticalGuard {
public:
    CriticalGuard() { portENTER_CRITICAL(&g_lock); }
    ~CriticalGuard() { portEXIT_CRITICAL(&g_lock); }
};
#else
class CriticalGuard {
public:
    CriticalGuard() = default;
    ~CriticalGuard() = default;
};
#endif

StateMachine g_state;

}  // namespace

void StateMachine::reset() {
    state_ = State::Idle;
    next_confirm_id_ = 1;
    confirm_id_ = 0;
    expected_size_ = 0;
    created_ms_ = 0;
    pending_until_ms_ = 0;
    allowed_until_ms_ = 0;
    terminal_until_ms_ = 0;
    terminal_observed_ = false;
    expired_event_confirm_id_ = 0;
    terminal_state_ = State::Idle;
    terminal_confirm_id_ = 0;
    terminal_expected_size_ = 0;
    terminal_record_until_ms_ = 0;
}

Snapshot StateMachine::snapshot() const {
    Snapshot out;
    out.state = state_;
    out.confirm_id = confirm_id_;
    out.expected_size = expected_size_;
    out.created_ms = created_ms_;
    out.pending_until_ms = pending_until_ms_;
    out.allowed_until_ms = allowed_until_ms_;
    return out;
}

bool StateMachine::deadlineReached(uint32_t now_ms, uint32_t deadline_ms) const {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

bool StateMachine::isInactiveForNewRequest(uint32_t now_ms) const {
    if (state_ == State::Idle || state_ == State::Consumed) {
        return true;
    }
    if (state_ == State::Denied || state_ == State::Expired) {
        return terminal_observed_ || deadlineReached(now_ms, terminal_until_ms_);
    }
    return false;
}

void StateMachine::enterTerminal(State state, uint32_t now_ms) {
    state_ = state;
    allowed_until_ms_ = 0;
    terminal_until_ms_ = now_ms + kTerminalRetentionMs;
    terminal_observed_ = false;
    if (state == State::Expired) {
        // Expiry may first be observed by prepare(), consumeForUpload(),
        // allowCurrent(), or denyCurrent(). Keep the UI event pending until
        // pollExpired() consumes it so the confirmation screen cannot remain
        // visible indefinitely.
        expired_event_confirm_id_ = confirm_id_;
    }
    recordTerminal(state, now_ms);
}

void StateMachine::noteTerminalObserved() {
    if (state_ == State::Denied || state_ == State::Expired) {
        terminal_observed_ = true;
        terminal_until_ms_ = 0;
    }
}

void StateMachine::recordTerminal(State state, uint32_t now_ms) {
    if (state != State::Denied && state != State::Expired) {
        return;
    }
    terminal_state_ = state;
    terminal_confirm_id_ = confirm_id_;
    terminal_expected_size_ = expected_size_;
    terminal_record_until_ms_ = now_ms + kPendingTimeoutMs;
}

bool StateMachine::terminalRecordMatches(uint32_t confirm_id,
                                         size_t expected_size,
                                         uint32_t now_ms) const {
    if (terminal_state_ != State::Denied && terminal_state_ != State::Expired) {
        return false;
    }
    if (deadlineReached(now_ms, terminal_record_until_ms_)) {
        return false;
    }
    return terminal_confirm_id_ != 0 &&
           confirm_id == terminal_confirm_id_ &&
           expected_size == terminal_expected_size_;
}

void StateMachine::expireIfDue(uint32_t now_ms) {
    if (state_ == State::Pending && deadlineReached(now_ms, pending_until_ms_)) {
        enterTerminal(State::Expired, now_ms);
        return;
    }

    if (state_ == State::Allowed && deadlineReached(now_ms, allowed_until_ms_)) {
        enterTerminal(State::Expired, now_ms);
    }
}

PrepareDecision StateMachine::createPending(size_t expected_size, uint32_t now_ms) {
    if (next_confirm_id_ == 0) {
        next_confirm_id_ = 1;
    }
    state_ = State::Pending;
    confirm_id_ = next_confirm_id_++;
    if (next_confirm_id_ == 0) {
        next_confirm_id_ = 1;
    }
    expected_size_ = expected_size;
    created_ms_ = now_ms;
    pending_until_ms_ = now_ms + kPendingTimeoutMs;
    allowed_until_ms_ = 0;
    terminal_until_ms_ = 0;
    terminal_observed_ = false;

    PrepareDecision decision;
    decision.status = PrepareStatus::Required;
    decision.confirm_id = confirm_id_;
    return decision;
}

bool StateMachine::matches(uint32_t confirm_id, size_t expected_size) const {
    return confirm_id_ != 0 &&
           confirm_id == confirm_id_ &&
           expected_size == expected_size_;
}

PrepareDecision StateMachine::prepare(size_t expected_size,
                                      bool has_confirm_id,
                                      uint32_t confirm_id,
                                      uint32_t now_ms) {
    expireIfDue(now_ms);

    if (!has_confirm_id) {
        if (isInactiveForNewRequest(now_ms)) {
            return createPending(expected_size, now_ms);
        }

        PrepareDecision decision;
        decision.status = PrepareStatus::Busy;
        decision.confirm_id = confirm_id_;
        return decision;
    }

    PrepareDecision decision;
    decision.confirm_id = confirm_id_;

    if (!matches(confirm_id, expected_size)) {
        if (terminalRecordMatches(confirm_id, expected_size, now_ms)) {
            decision.confirm_id = confirm_id;
            decision.status = terminal_state_ == State::Denied
                                  ? PrepareStatus::Denied
                                  : PrepareStatus::Expired;
            return decision;
        }
        decision.status = PrepareStatus::Mismatch;
        return decision;
    }

    switch (state_) {
        case State::Pending:
            decision.status = PrepareStatus::Required;
            decision.confirm_id = confirm_id_;
            return decision;
        case State::Allowed:
            decision.status = PrepareStatus::Ready;
            decision.confirm_id = confirm_id_;
            return decision;
        case State::Denied:
            decision.status = PrepareStatus::Denied;
            noteTerminalObserved();
            return decision;
        case State::Expired:
            decision.status = PrepareStatus::Expired;
            noteTerminalObserved();
            return decision;
        case State::Consumed:
        case State::Idle:
        default:
            decision.status = PrepareStatus::Expired;
            return decision;
    }
}

ConsumeDecision StateMachine::consumeForUpload(size_t expected_size,
                                               bool has_confirm_id,
                                               uint32_t confirm_id,
                                               uint32_t now_ms) {
    expireIfDue(now_ms);

    ConsumeDecision decision;
    decision.confirm_id = confirm_id_;

    if (!has_confirm_id) {
        decision.status = ConsumeStatus::Missing;
        return decision;
    }

    decision.confirm_id = confirm_id;
    if (!matches(confirm_id, expected_size)) {
        if (terminalRecordMatches(confirm_id, expected_size, now_ms)) {
            decision.status = terminal_state_ == State::Denied
                                  ? ConsumeStatus::Denied
                                  : ConsumeStatus::Expired;
            return decision;
        }
        decision.status = ConsumeStatus::Mismatch;
        return decision;
    }

    switch (state_) {
        case State::Allowed:
            state_ = State::Consumed;
            allowed_until_ms_ = 0;
            terminal_until_ms_ = 0;
            terminal_observed_ = false;
            decision.status = ConsumeStatus::Consumed;
            return decision;
        case State::Pending:
            decision.status = ConsumeStatus::NotAllowed;
            return decision;
        case State::Denied:
            decision.status = ConsumeStatus::Denied;
            noteTerminalObserved();
            return decision;
        case State::Expired:
            noteTerminalObserved();
            decision.status = ConsumeStatus::Expired;
            return decision;
        case State::Consumed:
        case State::Idle:
        default:
            decision.status = ConsumeStatus::Expired;
            return decision;
    }
}

bool StateMachine::allowCurrent(uint32_t expected_confirm_id, uint32_t now_ms) {
    expireIfDue(now_ms);
    if (state_ != State::Pending ||
        expected_confirm_id == 0 ||
        expected_confirm_id != confirm_id_) {
        return false;
    }
    state_ = State::Allowed;
    allowed_until_ms_ = now_ms + kAllowedTimeoutMs;
    terminal_until_ms_ = 0;
    terminal_observed_ = false;
    return true;
}

bool StateMachine::denyCurrent(uint32_t expected_confirm_id, uint32_t now_ms) {
    expireIfDue(now_ms);
    if (state_ != State::Pending ||
        expected_confirm_id == 0 ||
        expected_confirm_id != confirm_id_) {
        return false;
    }
    enterTerminal(State::Denied, now_ms);
    return true;
}

uint32_t StateMachine::pollExpired(uint32_t now_ms) {
    expireIfDue(now_ms);
    const uint32_t expired_confirm_id = expired_event_confirm_id_;
    expired_event_confirm_id_ = 0;
    return expired_confirm_id;
}

void reset() {
    CriticalGuard guard;
    g_state.reset();
}

Snapshot snapshot() {
    CriticalGuard guard;
    return g_state.snapshot();
}

PrepareDecision prepare(size_t expected_size, bool has_confirm_id, uint32_t confirm_id) {
    const uint32_t now_ms = millis();
    CriticalGuard guard;
    return g_state.prepare(expected_size, has_confirm_id, confirm_id, now_ms);
}

ConsumeDecision consumeForUpload(size_t expected_size, bool has_confirm_id, uint32_t confirm_id) {
    const uint32_t now_ms = millis();
    CriticalGuard guard;
    return g_state.consumeForUpload(expected_size, has_confirm_id, confirm_id, now_ms);
}

bool allowCurrent(uint32_t expected_confirm_id) {
    const uint32_t now_ms = millis();
    CriticalGuard guard;
    return g_state.allowCurrent(expected_confirm_id, now_ms);
}

bool denyCurrent(uint32_t expected_confirm_id) {
    const uint32_t now_ms = millis();
    CriticalGuard guard;
    return g_state.denyCurrent(expected_confirm_id, now_ms);
}

uint32_t pollExpired() {
    const uint32_t now_ms = millis();
    CriticalGuard guard;
    return g_state.pollExpired(now_ms);
}

const char *stateText(State state) {
    switch (state) {
        case State::Idle:
            return "idle";
        case State::Pending:
            return "pending";
        case State::Allowed:
            return "allowed";
        case State::Denied:
            return "denied";
        case State::Expired:
            return "expired";
        case State::Consumed:
            return "consumed";
        default:
            return "unknown";
    }
}

const char *prepareStatusText(PrepareStatus status) {
    switch (status) {
        case PrepareStatus::Ready:
            return "ready";
        case PrepareStatus::Required:
            return "required";
        case PrepareStatus::Busy:
            return "busy";
        case PrepareStatus::Denied:
            return "denied";
        case PrepareStatus::Expired:
            return "expired";
        case PrepareStatus::Mismatch:
            return "mismatch";
        default:
            return "unknown";
    }
}

const char *consumeStatusText(ConsumeStatus status) {
    switch (status) {
        case ConsumeStatus::Consumed:
            return "consumed";
        case ConsumeStatus::Missing:
            return "missing";
        case ConsumeStatus::NotAllowed:
            return "not_allowed";
        case ConsumeStatus::Denied:
            return "denied";
        case ConsumeStatus::Expired:
            return "expired";
        case ConsumeStatus::Mismatch:
            return "mismatch";
        default:
            return "unknown";
    }
}

}  // namespace OtaPhysicalConfirm
