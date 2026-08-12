// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace BoardInitPolicy {

enum class BeginOutcome : uint8_t {
    Success = 0,
    Failed,
    TaskCreateFailed,
    Timeout,
};

enum class CompletionAction : uint8_t {
    UseBoard = 0,
    DeleteBoard,
    RetainUntilRestart,
};

enum class PreInitAction : uint8_t {
    VendorInit = 0,
    RecoverThenVendorInit,
};

struct PreInitI2cSamples {
    bool early_diagnostic_sda_high = false;
    bool early_diagnostic_scl_high = false;
    bool pre_init_sda_high = false;
    bool pre_init_scl_high = false;
};

CompletionAction completionAction(BeginOutcome outcome);
PreInitAction preInitAction(bool recovery_boot,
                            const PreInitI2cSamples &samples);

} // namespace BoardInitPolicy
