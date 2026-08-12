// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace BoardInitPolicy {

enum class AttemptOutcome : uint8_t {
    Success = 0,
    Failed,
    TaskCreateFailed,
    Timeout,
};

enum class Action : uint8_t {
    ReturnSuccess = 0,
    RetryFresh,
    Abort,
};

struct PreInitI2cSamples {
    bool early_diagnostic_sda_high = false;
    bool early_diagnostic_scl_high = false;
    bool pre_init_sda_high = false;
    bool pre_init_scl_high = false;
};

Action decide(AttemptOutcome outcome, uint8_t round, uint8_t max_rounds);
uint32_t coldPowerSettleDelayMs(bool cold_start,
                                uint32_t uptime_ms,
                                uint32_t settle_until_ms);
bool shouldRecoverI2cBeforeInit(bool recovery_boot,
                                const PreInitI2cSamples &samples);

} // namespace BoardInitPolicy
