// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace PowerStartPolicy {

struct Classification {
    bool board_cold_start = false;
    bool peripherals_cold_start = false;
    bool peripherals_may_have_lost_power = false;
};

constexpr Classification classify(bool retained_settle_complete,
                                  bool reset_is_power_on,
                                  bool reset_is_brownout) {
    const bool retained_power_lost = !retained_settle_complete;
    const bool peripherals_cold_start =
        !reset_is_brownout && (retained_power_lost || reset_is_power_on);
    return {
        retained_power_lost || reset_is_power_on || reset_is_brownout,
        peripherals_cold_start,
        peripherals_cold_start || reset_is_brownout,
    };
}

}  // namespace PowerStartPolicy
