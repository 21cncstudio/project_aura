// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <float.h>
#include <cmath>
#include <string.h>
#include <time.h>

#include "config/AppConfig.h"
#include "modules/ChartsHistory.h"
#include "ui/UiText.h"
#include "ui/ui.h"

#include "ui/UiControllerGraphsShared.h"

namespace {
constexpr float kPmGraphZoneLowerSentinel = -100000.0f;
constexpr float kPmGraphZoneUpperSentinel = 100000.0f;
} // namespace

void UiController::ensure_pm05_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_pm05_info,
        pm05_graph_label_min_,
        pm05_graph_label_now_,
        pm05_graph_label_max_);
}

void UiController::update_pm05_graph_overlays(bool has_values,
                                              float min_value,
                                              float max_value,
                                              float latest_value) {
    update_graph_stat_overlay_labels(objects.chart_pm05_info,
                                     pm05_graph_label_min_,
                                     pm05_graph_label_now_,
                                     pm05_graph_label_max_,
                                     has_values,
                                     min_value,
                                     max_value,
                                     latest_value,
                                     "MIN %.0f #/cm3",
                                     "NOW %.0f #/cm3",
                                     "MAX %.0f #/cm3");
}

void UiController::ensure_pm05_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.pm05_info_graph,
        objects.chart_pm05_info,
        pm05_graph_zone_overlay_,
        pm05_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_pm05_zone_overlay(float y_min_display, float y_max_display) {
    ensure_pm05_zone_overlay();

    constexpr float kZoneBounds[] = {
        kPmGraphZoneLowerSentinel,
        Config::AQ_PM05_GREEN_MAX_PPCM3,
        Config::AQ_PM05_YELLOW_MAX_PPCM3,
        Config::AQ_PM05_ORANGE_MAX_PPCM3,
        kPmGraphZoneUpperSentinel};
    constexpr GraphZoneTone kZoneTones[] = {GRAPH_ZONE_GREEN, GRAPH_ZONE_YELLOW, GRAPH_ZONE_ORANGE, GRAPH_ZONE_RED};
    constexpr uint8_t kZoneCount = 4;

    update_graph_zone_overlay(objects.chart_pm05_info,
                              pm05_graph_zone_overlay_,
                              pm05_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kZoneBounds,
                              kZoneTones,
                              kZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_pm05_time_labels() {
    ensure_graph_time_labels(
        objects.pm05_info_graph,
        objects.chart_pm05_info,
        pm05_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_pm05_time_labels() {
    update_graph_time_labels(
        objects.pm05_info_graph,
        objects.chart_pm05_info,
        pm05_graph_time_labels_,
        kGraphTimeTickCount,
        pm05_graph_points(),
        true,
        true,
        false);
}

void UiController::update_pm05_info_graph() {
    if (!objects.chart_pm05_info) {
        return;
    }

    constexpr float kFallbackValue = 250.0f;
    constexpr float kMinSpan = 200.0f;

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(pm05_graph_range_);
    apply_standard_info_chart_theme(objects.chart_pm05_info, 5, vertical_divisions);

    const uint16_t points = pm05_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_pm05_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_pm05_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_PM05),
                                                              1.0f,
                                                              true);
    const bool has_values = stats.has_values;
    float min_value = stats.min_value;
    float max_value = stats.max_value;
    float latest_value = stats.latest_value;

    float scale_min = has_values ? min_value : 0.0f;
    float scale_max = has_values ? max_value : kFallbackValue;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = kFallbackValue;
    axis_config.min_span = kMinSpan;
    axis_config.fallback_half_span = kMinSpan;
    axis_config.fallback_step = 100.0f;
    axis_config.point_scale = 1.0f;
    axis_config.clamp_min_zero = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_value, axis_config);

    lv_chart_set_div_line_count(objects.chart_pm05_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_pm05_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_pm05_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!std::isfinite(latest_value)) {
            latest_value = max_value;
        }
        update_pm05_graph_overlays(true, min_value, max_value, latest_value);
    } else {
        update_pm05_graph_overlays(false, kFallbackValue, kFallbackValue, kFallbackValue);
    }
    update_pm05_time_labels();

    lv_chart_refresh(objects.chart_pm05_info);
    mark_active_graph_refreshed(INFO_PM05, pm05_graph_range_, points);
}

void UiController::ensure_pm25_4_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_pm25_4_graph,
        pm25_4_graph_label_min_,
        pm25_4_graph_label_now_,
        pm25_4_graph_label_max_);
}

void UiController::update_pm25_4_graph_overlays(bool has_values,
                                                float min_value,
                                                float max_value,
                                                float latest_value) {
    update_graph_stat_overlay_labels(objects.chart_pm25_4_graph,
                                     pm25_4_graph_label_min_,
                                     pm25_4_graph_label_now_,
                                     pm25_4_graph_label_max_,
                                     has_values,
                                     min_value,
                                     max_value,
                                     latest_value,
                                     "MIN %.1f ug/m3",
                                     "NOW %.1f ug/m3",
                                     "MAX %.1f ug/m3");
}

void UiController::ensure_pm25_4_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.pm25_4_graph,
        objects.chart_pm25_4_graph,
        pm25_4_graph_zone_overlay_,
        pm25_4_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_pm25_4_zone_overlay(float y_min_display, float y_max_display) {
    ensure_pm25_4_zone_overlay();

    static const float kPm25ZoneBounds[] = {
        -1000.0f,
        Config::AQ_PM25_GREEN_MAX_UGM3,
        Config::AQ_PM25_YELLOW_MAX_UGM3,
        Config::AQ_PM25_ORANGE_MAX_UGM3,
        kPmGraphZoneUpperSentinel,
    };
    static const float kPm4ZoneBounds[] = {
        -1000.0f,
        Config::AQ_PM4_GREEN_MAX_UGM3,
        Config::AQ_PM4_YELLOW_MAX_UGM3,
        Config::AQ_PM4_ORANGE_MAX_UGM3,
        kPmGraphZoneUpperSentinel,
    };
    static const GraphZoneTone kPmZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kPmZoneCount = 4;
    const bool pm4_selected = info_sensor == INFO_PM4;
    const float *zone_bounds = pm4_selected ? kPm4ZoneBounds : kPm25ZoneBounds;

    update_graph_zone_overlay(objects.chart_pm25_4_graph,
                              pm25_4_graph_zone_overlay_,
                              pm25_4_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              zone_bounds,
                              kPmZoneTones,
                              kPmZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_pm25_4_time_labels() {
    ensure_graph_time_labels(
        objects.pm25_4_graph,
        objects.chart_pm25_4_graph,
        pm25_4_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_pm25_4_time_labels() {
    update_graph_time_labels(
        objects.pm25_4_graph,
        objects.chart_pm25_4_graph,
        pm25_4_graph_time_labels_,
        kGraphTimeTickCount,
        pm25_4_graph_points());
}

void UiController::update_pm25_4_info_graph() {
    if (!objects.chart_pm25_4_graph) {
        return;
    }

    const bool pm4_selected = info_sensor == INFO_PM4;
    const ChartsHistory::Metric metric = pm4_selected ? ChartsHistory::METRIC_PM4 : ChartsHistory::METRIC_PM25;
    const float fallback_value = pm4_selected ? Config::AQ_PM4_GREEN_MAX_UGM3 : Config::AQ_PM25_GREEN_MAX_UGM3;
    const float min_span = pm4_selected ? 15.0f : 10.0f;

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(pm25_4_graph_range_);
    apply_standard_info_chart_theme(objects.chart_pm25_4_graph, 5, vertical_divisions);

    const uint16_t points = pm25_4_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_pm25_4_graph, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_pm25_4_graph,
                                                              series,
                                                              points,
                                                              static_cast<int>(metric),
                                                              10.0f,
                                                              true);
    const bool has_values = stats.has_values;
    float min_value = stats.min_value;
    float max_value = stats.max_value;
    float latest_value = stats.latest_value;

    float scale_min = has_values ? min_value : 0.0f;
    float scale_max = has_values ? max_value : fallback_value;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = fallback_value;
    axis_config.min_span = min_span;
    axis_config.fallback_half_span = min_span;
    axis_config.fallback_step = 5.0f;
    axis_config.point_scale = 10.0f;
    axis_config.clamp_min_zero = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_value, axis_config);

    lv_chart_set_div_line_count(objects.chart_pm25_4_graph,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_pm25_4_graph, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_pm25_4_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!std::isfinite(latest_value)) {
            latest_value = max_value;
        }
        update_pm25_4_graph_overlays(true, min_value, max_value, latest_value);
    } else {
        update_pm25_4_graph_overlays(false, fallback_value, fallback_value, fallback_value);
    }
    update_pm25_4_time_labels();

    lv_chart_refresh(objects.chart_pm25_4_graph);
    mark_active_graph_refreshed(info_sensor, pm25_4_graph_range_, points);
}

void UiController::ensure_pm1_10_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_pm1_10_info,
        pm1_10_graph_label_min_,
        pm1_10_graph_label_now_,
        pm1_10_graph_label_max_);
}

void UiController::update_pm1_10_graph_overlays(bool has_values,
                                                float min_value,
                                                float max_value,
                                                float latest_value) {
    update_graph_stat_overlay_labels(objects.chart_pm1_10_info,
                                     pm1_10_graph_label_min_,
                                     pm1_10_graph_label_now_,
                                     pm1_10_graph_label_max_,
                                     has_values,
                                     min_value,
                                     max_value,
                                     latest_value,
                                     "MIN %.1f ug/m3",
                                     "NOW %.1f ug/m3",
                                     "MAX %.1f ug/m3");
}

void UiController::ensure_pm1_10_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.pm1_10_info_graph,
        objects.chart_pm1_10_info,
        pm1_10_graph_zone_overlay_,
        pm1_10_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_pm1_10_zone_overlay(float y_min_display, float y_max_display) {
    ensure_pm1_10_zone_overlay();

    static const float kPm1ZoneBounds[] = {
        -1000.0f,
        Config::AQ_PM1_GREEN_MAX_UGM3,
        Config::AQ_PM1_YELLOW_MAX_UGM3,
        Config::AQ_PM1_ORANGE_MAX_UGM3,
        kPmGraphZoneUpperSentinel};
    static const float kPm10ZoneBounds[] = {
        -1000.0f,
        Config::AQ_PM10_GREEN_MAX_UGM3,
        Config::AQ_PM10_YELLOW_MAX_UGM3,
        Config::AQ_PM10_ORANGE_MAX_UGM3,
        kPmGraphZoneUpperSentinel};
    static const GraphZoneTone kPmZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kPmZoneCount = 4;
    const bool pm10_selected = info_sensor == INFO_PM10;
    const float *zone_bounds = pm10_selected ? kPm10ZoneBounds : kPm1ZoneBounds;

    update_graph_zone_overlay(objects.chart_pm1_10_info,
                              pm1_10_graph_zone_overlay_,
                              pm1_10_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              zone_bounds,
                              kPmZoneTones,
                              kPmZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_pm1_10_time_labels() {
    ensure_graph_time_labels(
        objects.pm1_10_info_graph,
        objects.chart_pm1_10_info,
        pm1_10_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_pm1_10_time_labels() {
    update_graph_time_labels(
        objects.pm1_10_info_graph,
        objects.chart_pm1_10_info,
        pm1_10_graph_time_labels_,
        kGraphTimeTickCount,
        pm1_10_graph_points());
}

void UiController::update_pm1_10_info_graph() {
    if (!objects.chart_pm1_10_info) {
        return;
    }

    const bool pm10_selected = info_sensor == INFO_PM10;
    const ChartsHistory::Metric metric = pm10_selected ? ChartsHistory::METRIC_PM10 : ChartsHistory::METRIC_PM1;
    const float fallback_value = pm10_selected ? 54.0f : 10.0f;
    const float min_span = pm10_selected ? 20.0f : 8.0f;

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(pm1_10_graph_range_);
    apply_standard_info_chart_theme(objects.chart_pm1_10_info, 5, vertical_divisions);

    const uint16_t points = pm1_10_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_pm1_10_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_pm1_10_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(metric),
                                                              10.0f,
                                                              true);
    const bool has_values = stats.has_values;
    float min_value = stats.min_value;
    float max_value = stats.max_value;
    float latest_value = stats.latest_value;

    float scale_min = has_values ? min_value : 0.0f;
    float scale_max = has_values ? max_value : fallback_value;
    const float fallback_step = pm10_selected ? 10.0f : 2.0f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = fallback_value;
    axis_config.min_span = min_span;
    axis_config.fallback_half_span = min_span;
    axis_config.fallback_step = fallback_step;
    axis_config.point_scale = 10.0f;
    axis_config.clamp_min_zero = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_value, axis_config);

    lv_chart_set_div_line_count(objects.chart_pm1_10_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_pm1_10_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_pm1_10_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!std::isfinite(latest_value)) {
            latest_value = max_value;
        }
        update_pm1_10_graph_overlays(true, min_value, max_value, latest_value);
    } else {
        update_pm1_10_graph_overlays(false, fallback_value, fallback_value, fallback_value);
    }
    update_pm1_10_time_labels();

    lv_chart_refresh(objects.chart_pm1_10_info);
    mark_active_graph_refreshed(info_sensor, pm1_10_graph_range_, points);
}

