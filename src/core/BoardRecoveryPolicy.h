// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace BoardRecoveryPolicy {

enum class Decision : uint8_t {
    NotNeeded = 0,
    Restart,
    SuppressAlreadyAttempted,
    SuppressNotEligible,
    SuppressRestartUnavailable,
};

Decision decide(bool board_ready,
                bool recovery_eligible,
                bool auto_recovery_boot,
                bool restart_task_ready);
const char *decisionText(Decision decision);

} // namespace BoardRecoveryPolicy
