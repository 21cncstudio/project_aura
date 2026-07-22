// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "drivers/DfrMultiGasSensor.h"

class DfrOptionalGasSensor : public DfrMultiGasSensor {
public:
    enum class OptionalGasType : uint8_t {
        None = 0,
        NH3 = 1,
        SO2 = 2,
        NO2 = 3,
        H2S = 4,
        O3 = 5,
        O2 = 6,
    };

    DfrOptionalGasSensor();

    void poll();

    OptionalGasType optionalGasType() const;
    const char *optionalGasLabel() const;

    static const char *optionalGasLabel(OptionalGasType type);
    static const char *unitForType(OptionalGasType type);
    static float minPpmForType(OptionalGasType type);
    static float maxPpmForType(OptionalGasType type);
};
