// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "modules/ChartsHistory.h"

namespace WebChartsUtils {

struct ChartMetricSpec {
    const char *key;
    const char *unit;
    ChartsHistory::Metric metric;
};

uint16_t chartWindowPoints(const String &window_arg, const char *&window_name);
void chartGroupMetrics(const String &group_arg,
                       const char *&group_name,
                       const ChartMetricSpec *&metrics,
                       size_t &metric_count);

} // namespace WebChartsUtils
