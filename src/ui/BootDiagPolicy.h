// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

namespace BootDiagPolicy {

inline bool sen66Pending(bool sensor_ok, bool sensor_busy, bool startup_probe_pending) {
    return !sensor_ok && (sensor_busy || startup_probe_pending);
}

inline bool shouldAutoAdvance(bool has_errors,
                              bool sen66_pending,
                              uint32_t elapsed_ms,
                              uint32_t auto_advance_ms) {
    return !has_errors && !sen66_pending && elapsed_ms >= auto_advance_ms;
}

} // namespace BootDiagPolicy
