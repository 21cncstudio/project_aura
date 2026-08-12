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

uint32_t coldPowerSettleDelayMs(bool cold_start,
                                uint32_t uptime_ms,
                                uint32_t settle_until_ms) {
    if (!cold_start || uptime_ms >= settle_until_ms) {
        return 0;
    }
    return settle_until_ms - uptime_ms;
}

bool shouldRecoverI2cBeforeInit(bool recovery_boot,
                                const PreInitI2cSamples &samples) {
    return recovery_boot &&
           (!samples.pre_init_sda_high || !samples.pre_init_scl_high);
}

} // namespace BoardInitPolicy
