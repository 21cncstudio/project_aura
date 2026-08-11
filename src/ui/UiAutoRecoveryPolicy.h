// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace UiAutoRecoveryPolicy {

enum class Decision : uint8_t {
    RequestRestart = 0,
    SuppressRecoveryBoot,
    SuppressAlreadyRequested,
};

constexpr Decision decide(bool recovery_boot, bool restart_already_requested) {
    if (restart_already_requested) {
        return Decision::SuppressAlreadyRequested;
    }
    if (recovery_boot) {
        return Decision::SuppressRecoveryBoot;
    }
    return Decision::RequestRestart;
}

} // namespace UiAutoRecoveryPolicy
