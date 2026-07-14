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

} // namespace SdCardPolicy
