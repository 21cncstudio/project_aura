// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace DfrGasProbeLogPolicy {

struct ProbeFailure {
    bool optional_installation = false;
    bool sensor_was_present = false;
    bool address_nack = false;
    bool lines_before_idle = false;
    bool lines_after_idle = false;
};

inline bool isExpectedOptionalAbsence(const ProbeFailure &failure) {
    return failure.optional_installation &&
           !failure.sensor_was_present &&
           failure.address_nack &&
           failure.lines_before_idle &&
           failure.lines_after_idle;
}

inline bool isExpectedOptionalAbsenceAfterProbe(const ProbeFailure &failure) {
    // The pre-probe GPIO snapshot can overlap unrelated legal traffic on a
    // shared bus. A never-present optional device that NACKed and left the bus
    // idle is still an expected absence, while a low post-probe line remains a
    // real warning.
    return failure.optional_installation &&
           !failure.sensor_was_present &&
           failure.address_nack &&
           failure.lines_after_idle;
}

} // namespace DfrGasProbeLogPolicy
