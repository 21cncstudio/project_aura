// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace DfrGasProbeLogPolicy {

struct ProbeFailure {
    bool optional_slot = false;
    bool sensor_was_present = false;
    bool address_nack = false;
    bool lines_before_idle = false;
    bool lines_after_idle = false;
};

inline bool isExpectedOptionalAbsence(const ProbeFailure &failure) {
    return failure.optional_slot &&
           !failure.sensor_was_present &&
           failure.address_nack &&
           failure.lines_before_idle &&
           failure.lines_after_idle;
}

} // namespace DfrGasProbeLogPolicy
