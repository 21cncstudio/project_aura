// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardRecoveryPolicy.h"

namespace BoardRecoveryPolicy {

Decision decide(bool board_ready,
                bool power_on_reset,
                bool auto_recovery_boot,
                bool restart_task_ready) {
    if (board_ready) {
        return Decision::NotNeeded;
    }
    if (auto_recovery_boot) {
        return Decision::SuppressAlreadyAttempted;
    }
    if (!power_on_reset) {
        return Decision::SuppressResetReason;
    }
    if (!restart_task_ready) {
        return Decision::SuppressRestartUnavailable;
    }
    return Decision::Restart;
}

const char *decisionText(Decision decision) {
    switch (decision) {
        case Decision::NotNeeded: return "not_needed";
        case Decision::Restart: return "restart_requested";
        case Decision::SuppressAlreadyAttempted: return "already_attempted";
        case Decision::SuppressResetReason: return "reset_reason_not_poweron";
        case Decision::SuppressRestartUnavailable: return "restart_task_unavailable";
        default: return "unknown";
    }
}

} // namespace BoardRecoveryPolicy
