// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace SharedI2cShutdownPolicy {

enum class SafeOutputDecision : uint8_t {
    SkipOwnersActive = 0,
    Attempt,
    AttemptAfterUnconfirmedLvglPause,
};

constexpr SafeOutputDecision decideSafeOutput(bool lvgl_quiesced,
                                              bool owners_drained) {
    if (!owners_drained) {
        return SafeOutputDecision::SkipOwnersActive;
    }
    return lvgl_quiesced
        ? SafeOutputDecision::Attempt
        : SafeOutputDecision::AttemptAfterUnconfirmedLvglPause;
}

constexpr bool shouldAttemptSafeOutput(SafeOutputDecision decision) {
    return decision != SafeOutputDecision::SkipOwnersActive;
}

constexpr bool shouldWarnUnconfirmedLvglPause(SafeOutputDecision decision) {
    return decision == SafeOutputDecision::AttemptAfterUnconfirmedLvglPause;
}

} // namespace SharedI2cShutdownPolicy
