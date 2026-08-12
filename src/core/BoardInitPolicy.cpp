// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardInitPolicy.h"

namespace BoardInitPolicy {

CompletionAction completionAction(BeginOutcome outcome) {
    switch (outcome) {
        case BeginOutcome::Success:
            return CompletionAction::UseBoard;
        case BeginOutcome::Timeout:
            return CompletionAction::RetainUntilRestart;
        case BeginOutcome::Failed:
        case BeginOutcome::TaskCreateFailed:
        default:
            return CompletionAction::DeleteBoard;
    }
}

PreInitAction preInitAction(bool recovery_boot,
                            const PreInitI2cSamples &samples) {
    if (recovery_boot &&
        (!samples.pre_init_sda_high || !samples.pre_init_scl_high)) {
        return PreInitAction::RecoverThenVendorInit;
    }
    return PreInitAction::VendorInit;
}

} // namespace BoardInitPolicy
