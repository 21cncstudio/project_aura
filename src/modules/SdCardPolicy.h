// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

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

} // namespace SdCardPolicy
