// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardRecoveryPolicy.h"

namespace BoardRecoveryPolicy {

Decision decide(bool board_ready,
                bool lvgl_ready,
                bool board_recovery_eligible,
                bool auto_recovery_boot,
                bool restart_task_ready) {
    if (board_ready && lvgl_ready) {
        return Decision::NotNeeded;
    }
    if (auto_recovery_boot) {
        return Decision::SuppressAlreadyAttempted;
    }
    const bool recovery_eligible = board_ready ? !lvgl_ready : board_recovery_eligible;
    if (!recovery_eligible) {
        return Decision::SuppressNotEligible;
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
        case Decision::SuppressNotEligible: return "not_recovery_eligible";
        case Decision::SuppressRestartUnavailable: return "restart_task_unavailable";
        default: return "unknown";
    }
}

} // namespace BoardRecoveryPolicy
