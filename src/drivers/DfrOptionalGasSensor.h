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
        O3_SEN0321 = 7, // O3, canonical ppm storage, ppb presentation
    };

    DfrOptionalGasSensor();

    void poll();

    OptionalGasType optionalGasType() const;
    const char *optionalGasLabel() const;

    static const char *optionalGasLabel(OptionalGasType type);
    static float displayScale(OptionalGasType type) {
        return type == OptionalGasType::O3_SEN0321 ? 1000.0f : 1.0f;
    }
    static float displayValue(OptionalGasType type, float ppm) { return ppm * displayScale(type); }
    static bool isOzone(OptionalGasType type) {
        return type == OptionalGasType::O3 || type == OptionalGasType::O3_SEN0321;
    }
    static const char *unitForType(OptionalGasType type);
    static float minPpmForType(OptionalGasType type);
    static float maxPpmForType(OptionalGasType type);
};
