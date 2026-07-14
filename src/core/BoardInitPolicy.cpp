// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardInitPolicy.h"

namespace BoardInitPolicy {

Action decide(AttemptOutcome outcome, uint8_t round, uint8_t max_rounds) {
    if (outcome == AttemptOutcome::Success) {
        return Action::ReturnSuccess;
    }
    if (outcome == AttemptOutcome::Timeout || round >= max_rounds) {
        return Action::Abort;
    }
    return Action::RetryFresh;
}

} // namespace BoardInitPolicy
