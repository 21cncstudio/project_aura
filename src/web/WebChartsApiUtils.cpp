// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebChartsApiUtils.h"

#include <cmath>

#include "config/AppConfig.h"
#include "web/WebChartsUtils.h"

namespace WebChartsApiUtils {

namespace {

constexpr uint32_t kChartStepS = Config::CHART_HISTORY_STEP_MS / 1000UL;

bool history_latest_metric(const HistoryView &history,
                           ChartsHistory::Metric metric,
                           float &out_value) {
    if (!history.latestMetric(metric, out_value) || !std::isfinite(out_value)) {
        return false;
    }
    return true;
}

} // namespace

void fillJson(ArduinoJson::JsonObject root,
              const HistoryView &history,
              const String &window_arg,
              const String &group_arg,
              const char *optional_gas_unit, bool include_statistics) {
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

    root["schema_version"] = ChartsHistory::kSchemaVersion;
    root["value_semantics"] = "mean_with_labelled_legacy_snapshots";
    root["success"] = true;
    root["group"] = group_name;
    root["window"] = window_name;
    root["step_s"] = kChartStepS;
    root["points"] = window_points;
    root["available"] = available;

    auto kinds = root["record_kind"].to<ArduinoJson::JsonArray>();
    for (uint16_t i = 0; i < window_points; ++i) {
        ChartsHistory::Entry record;
        if (i < missing_prefix || !history.entryFromOldest(start_offset + i - missing_prefix, record)) {
            kinds.add(nullptr);
        } else kinds.add(record.kind == ChartsHistory::Summary ? "summary" :
                         record.kind == ChartsHistory::LegacySnapshot ? "snapshot" : "gap");
    }
    ArduinoJson::JsonArray timestamps = root["timestamps"].to<ArduinoJson::JsonArray>();
    for (uint16_t i = 0; i < window_points; ++i) {
        ChartsHistory::Entry record;
        if (i >= missing_prefix && history.entryFromOldest(start_offset + i - missing_prefix, record)) {
            if (record.start_epoch) timestamps.add(record.start_epoch); else timestamps.add(nullptr);
            continue;
        }
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
        entry["unit"] = spec.metric == ChartsHistory::METRIC_OPTIONAL_GAS && optional_gas_unit
                            ? optional_gas_unit
                            : spec.unit;

        float latest_value = 0.0f;
        if (history_latest_metric(history, spec.metric, latest_value)) {
            entry["latest"] = latest_value;
        } else {
            entry["latest"] = nullptr;
        }

        ArduinoJson::JsonArray values = entry["values"].to<ArduinoJson::JsonArray>();
        ArduinoJson::JsonArray minima, maxima, counts, first, last;
        if (include_statistics) {
            minima = entry["min"].to<ArduinoJson::JsonArray>();
            maxima = entry["max"].to<ArduinoJson::JsonArray>();
            counts = entry["count"].to<ArduinoJson::JsonArray>();
            first = entry["first_second"].to<ArduinoJson::JsonArray>();
            last = entry["last_second"].to<ArduinoJson::JsonArray>();
        }
        for (uint16_t slot = 0; slot < window_points; ++slot) {
            ChartsHistory::Entry record;
            float chart_value = 0; bool chart_valid = false;
            const bool summary = include_statistics && slot >= missing_prefix &&
                history.metricValueFromOldest(start_offset + slot - missing_prefix, spec.metric, chart_value, chart_valid) && chart_valid &&
                history.entryFromOldest(start_offset + slot - missing_prefix, record) &&
                record.kind == ChartsHistory::Summary && (record.valid_mask & ChartsHistory::metricBit(spec.metric));
            if (summary) {
                minima.add(record.minimum[spec.metric]); maxima.add(record.maximum[spec.metric]);
                counts.add(record.samples[spec.metric]); first.add(record.first_second[spec.metric]);
                last.add(record.last_second[spec.metric]);
            } else if (include_statistics) {
                minima.add(nullptr); maxima.add(nullptr); counts.add(nullptr); first.add(nullptr); last.add(nullptr);
            }
            if (slot < missing_prefix) {
                values.add(nullptr);
                continue;
            }

            const uint16_t offset = static_cast<uint16_t>(start_offset + (slot - missing_prefix));
            float value = 0.0f;
            bool valid = false;
            if (!history.metricValueFromOldest(offset, spec.metric, value, valid) || !valid ||
                !std::isfinite(value)) {
                values.add(nullptr);
                continue;
            }
            values.add(value);
        }
    }
}

void fillHistoryJson(ArduinoJson::JsonObject root, const HistoryView &history,
                     uint32_t after_epoch, uint16_t limit) {
    root["schema"] = "aura.history.v3";
    root["step_s"] = kChartStepS;
    root["success"] = true;
    auto records = root["records"].to<ArduinoJson::JsonArray>();
    if (limit == 0 || limit > 8) limit = 8;
    uint32_t cursor = after_epoch;
    bool more = false;
    for (uint16_t n = 0; n < history.count(); ++n) {
        ChartsHistory::Entry record;
        if (!history.entryFromOldest(n, record) || !record.start_epoch || record.start_epoch <= after_epoch) continue;
        if (!record.persisted || records.size() >= limit) { more = true; break; }
        auto item = records.add<ArduinoJson::JsonObject>();
        item["start"] = record.start_epoch;
        if (record.kind != ChartsHistory::LegacySnapshot) item["end"] = record.start_epoch + kChartStepS;
        item["kind"] = record.kind == ChartsHistory::Summary ? "summary" :
                       record.kind == ChartsHistory::LegacySnapshot ? "snapshot" : "gap";
        item["optional_gas_type"] = record.optional_gas_type;
        auto metrics = item["metrics"].to<ArduinoJson::JsonObject>();
        // Numeric metric IDs are fixed by the versioned schema, never by display order.
        for (int m = 0; m < ChartsHistory::kMetricCount; ++m) {
            if (!(record.valid_mask & (1U << m))) continue;
            char key[4]; snprintf(key, sizeof(key), "%d", m);
            auto metric = metrics[key].to<ArduinoJson::JsonObject>();
            metric[record.kind == ChartsHistory::Summary ? "mean" : "value"] = record.values[m];
            if (record.kind == ChartsHistory::Summary) {
                metric["min"] = record.minimum[m]; metric["max"] = record.maximum[m];
                metric["count"] = record.samples[m]; metric["first_second"] = record.first_second[m];
                metric["last_second"] = record.last_second[m];
            }
        }
        cursor = record.start_epoch;
    }
    root["next_after"] = cursor;
    root["has_more"] = more;
}

} // namespace WebChartsApiUtils
