// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebChartsUtils.h"

#include <ctype.h>

#include "config/AppConfig.h"

namespace WebChartsUtils {

namespace {

constexpr ChartMetricSpec kChartCoreMetrics[] = {
    {"co2", "ppm", ChartsHistory::METRIC_CO2},
    {"temperature", "C", ChartsHistory::METRIC_TEMPERATURE},
    {"humidity", "%", ChartsHistory::METRIC_HUMIDITY},
    {"pressure", "hPa", ChartsHistory::METRIC_PRESSURE},
};

constexpr ChartMetricSpec kChartGasMetrics[] = {
    {"co", "ppm", ChartsHistory::METRIC_CO},
    {"voc", "idx", ChartsHistory::METRIC_VOC},
    {"nox", "idx", ChartsHistory::METRIC_NOX},
    {"hcho", "ppb", ChartsHistory::METRIC_HCHO},
};

constexpr ChartMetricSpec kChartPmMetrics[] = {
    {"pm05", "#/cm3", ChartsHistory::METRIC_PM05},
    {"pm1", "ug/m3", ChartsHistory::METRIC_PM1},
    {"pm25", "ug/m3", ChartsHistory::METRIC_PM25},
    {"pm4", "ug/m3", ChartsHistory::METRIC_PM4},
    {"pm10", "ug/m3", ChartsHistory::METRIC_PM10},
};

String normalize_token(const String &value) {
    String out;
    const char *begin = value.c_str();
    if (!begin) {
        return out;
    }

    const char *end = begin;
    while (*end != '\0') {
        ++end;
    }
    while (begin < end && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    out.reserve(static_cast<size_t>(end - begin));
    for (const char *it = begin; it < end; ++it) {
        out += static_cast<char>(tolower(static_cast<unsigned char>(*it)));
    }
    return out;
}

} // namespace

uint16_t chartWindowPoints(const String &window_arg, const char *&window_name) {
    const String window = normalize_token(window_arg);

    if (window == "1h") {
        window_name = "1h";
        return Config::CHART_HISTORY_1H_STEPS;
    }
    if (window == "24h") {
        window_name = "24h";
        return Config::CHART_HISTORY_24H_SAMPLES;
    }
    window_name = "3h";
    return Config::CHART_HISTORY_3H_STEPS;
}

void chartGroupMetrics(const String &group_arg,
                       const char *&group_name,
                       const ChartMetricSpec *&metrics,
                       size_t &metric_count) {
    const String group = normalize_token(group_arg);

    if (group == "gases" || group == "gas") {
        group_name = "gases";
        metrics = kChartGasMetrics;
        metric_count = sizeof(kChartGasMetrics) / sizeof(kChartGasMetrics[0]);
        return;
    }
    if (group == "pm") {
        group_name = "pm";
        metrics = kChartPmMetrics;
        metric_count = sizeof(kChartPmMetrics) / sizeof(kChartPmMetrics[0]);
        return;
    }
    group_name = "core";
    metrics = kChartCoreMetrics;
    metric_count = sizeof(kChartCoreMetrics) / sizeof(kChartCoreMetrics[0]);
}

} // namespace WebChartsUtils
