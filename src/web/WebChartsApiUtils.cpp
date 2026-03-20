// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebChartsApiUtils.h"

#include <math.h>

#include "config/AppConfig.h"
#include "web/WebChartsUtils.h"

namespace WebChartsApiUtils {

namespace {

constexpr uint32_t kChartStepS = Config::CHART_HISTORY_STEP_MS / 1000UL;

bool history_latest_metric(const HistoryView &history,
                           ChartsHistory::Metric metric,
                           float &out_value) {
    if (!history.latestMetric(metric, out_value) || !isfinite(out_value)) {
        return false;
    }
    return true;
}

} // namespace

void fillJson(ArduinoJson::JsonObject root,
              const HistoryView &history,
              const String &window_arg,
              const String &group_arg) {
    const char *window_name = "3h";
    const uint16_t window_points = WebChartsUtils::chartWindowPoints(window_arg, window_name);

    const char *group_name = "core";
    const WebChartsUtils::ChartMetricSpec *metrics = nullptr;
    size_t metric_count = 0;
    WebChartsUtils::chartGroupMetrics(group_arg, group_name, metrics, metric_count);

    const uint16_t total_count = history.count();
    const uint16_t available = (total_count < window_points) ? total_count : window_points;
    const uint16_t missing_prefix = static_cast<uint16_t>(window_points - available);
    const uint16_t start_offset = static_cast<uint16_t>(total_count - available);

    const uint32_t latest_epoch = history.latestEpoch();
    const bool has_epoch = latest_epoch > Config::TIME_VALID_EPOCH;

    root["success"] = true;
    root["group"] = group_name;
    root["window"] = window_name;
    root["step_s"] = kChartStepS;
    root["points"] = window_points;
    root["available"] = available;

    ArduinoJson::JsonArray timestamps = root["timestamps"].to<ArduinoJson::JsonArray>();
    for (uint16_t i = 0; i < window_points; ++i) {
        if (!has_epoch) {
            timestamps.add(nullptr);
            continue;
        }
        const uint32_t back_steps = static_cast<uint32_t>(window_points - 1U - i);
        timestamps.add(latest_epoch - back_steps * kChartStepS);
    }

    ArduinoJson::JsonArray series = root["series"].to<ArduinoJson::JsonArray>();
    for (size_t i = 0; i < metric_count; ++i) {
        const WebChartsUtils::ChartMetricSpec &spec = metrics[i];
        ArduinoJson::JsonObject entry = series.add<ArduinoJson::JsonObject>();
        entry["key"] = spec.key;
        entry["unit"] = spec.unit;

        float latest_value = 0.0f;
        if (history_latest_metric(history, spec.metric, latest_value)) {
            entry["latest"] = latest_value;
        } else {
            entry["latest"] = nullptr;
        }

        ArduinoJson::JsonArray values = entry["values"].to<ArduinoJson::JsonArray>();
        for (uint16_t slot = 0; slot < window_points; ++slot) {
            if (slot < missing_prefix) {
                values.add(nullptr);
                continue;
            }

            const uint16_t offset = static_cast<uint16_t>(start_offset + (slot - missing_prefix));
            float value = 0.0f;
            bool valid = false;
            if (!history.metricValueFromOldest(offset, spec.metric, value, valid) || !valid ||
                !isfinite(value)) {
                values.add(nullptr);
                continue;
            }
            values.add(value);
        }
    }
}

} // namespace WebChartsApiUtils
