// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace Gt911ProbePolicy {

struct Plan {
    uint8_t address;
};

constexpr Plan configuredAddressOnly(uint8_t configured_address) {
    return Plan{configured_address};
}

} // namespace Gt911ProbePolicy
