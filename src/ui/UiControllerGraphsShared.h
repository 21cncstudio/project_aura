// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <cmath>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "config/AppConfig.h"

inline float temperature_to_display(float celsius, bool units_c) {
    if (units_c) {
        return celsius;
    }
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

inline float graph_nice_step(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 1.0f;
    }
    const float exponent = floorf(log10f(value));
    const float base = powf(10.0f, exponent);
    const float normalized = value / base;
    float nice = 1.0f;
    if (normalized <= 1.0f) {
        nice = 1.0f;
    } else if (normalized <= 2.0f) {
        nice = 2.0f;
    } else if (normalized <= 5.0f) {
        nice = 5.0f;
    } else {
        nice = 10.0f;
    }
    return nice * base;
}

inline bool format_epoch_hhmm(time_t epoch, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0 || epoch <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    tm local_tm = {};
    if (!localtime_r(&epoch, &local_tm)) {
        return false;
    }
    snprintf(buf, buf_size, "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
    return true;
}

inline void format_relative_time_label(uint32_t offset_s, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return;
    }
    if (offset_s == 0) {
        snprintf(buf, buf_size, "now");
        return;
    }
    const uint32_t hours = offset_s / 3600U;
    const uint32_t minutes = (offset_s % 3600U) / 60U;
    if (hours > 0 && minutes == 0) {
        snprintf(buf, buf_size, "-%luh", static_cast<unsigned long>(hours));
        return;
    }
    if (hours > 0) {
        snprintf(buf, buf_size, "-%luh%02lum", static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
        return;
    }
    snprintf(buf, buf_size, "-%lum", static_cast<unsigned long>(minutes > 0 ? minutes : 1U));
}

inline constexpr uint8_t kGraphTimeTickCount = 7;
