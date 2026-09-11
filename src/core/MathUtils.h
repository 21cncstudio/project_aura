// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <cmath>

namespace MathUtils {

inline float compute_dew_point_c(float temp_c, float rh) {
    if (!std::isfinite(temp_c) || !std::isfinite(rh) || rh <= 0.0f) {
        return NAN;
    }
    float rh_clamped = fminf(fmaxf(rh, 1.0f), 100.0f);
    constexpr float kA = 17.62f;
    constexpr float kB = 243.12f;
    float gamma = logf(rh_clamped / 100.0f) + (kA * temp_c) / (kB + temp_c);
    return (kB * gamma) / (kA - gamma);
}

inline float compute_absolute_humidity_gm3(float temp_c, float rh) {
    if (!std::isfinite(temp_c) || !std::isfinite(rh) || rh <= 0.0f) {
        return NAN;
    }
    float rh_clamped = fminf(fmaxf(rh, 1.0f), 100.0f);
    // Saturation vapor pressure (hPa)
    float es = 6.112f * expf((17.67f * temp_c) / (temp_c + 243.5f));
    float e = (rh_clamped / 100.0f) * es;
    // Absolute humidity (g/m^3)
    return 216.7f * (e / (temp_c + 273.15f));
}

inline int compute_mold_risk_index(float temp_c, float rh) {
    if (!std::isfinite(temp_c) || !std::isfinite(rh) || rh < 0.0f || rh > 100.0f) {
        return -1;
    }

    // Practical 0..10 indoor mold risk heuristic driven by RH + temperature.
    // RH is the main driver; warmer air slightly increases risk.
    float risk = ((rh - 55.0f) / 4.0f) + ((temp_c - 18.0f) / 7.0f);
    risk = fminf(fmaxf(risk, 0.0f), 10.0f);
    return static_cast<int>(lroundf(risk));
}

} // namespace MathUtils
