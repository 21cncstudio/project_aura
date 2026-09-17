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

void UiController::apply_temperature_graph_theme(const SensorGraphProfile &profile) {
    if (!objects.chart_temp_info) {
        return;
    }

    const uint8_t initial_horizontal = (profile.horizontal_divisions_min > 0) ? profile.horizontal_divisions_min : 3;
    const uint8_t initial_vertical = (profile.vertical_divisions > 0) ? profile.vertical_divisions : 15;
    apply_standard_info_chart_theme(objects.chart_temp_info, initial_horizontal, initial_vertical);
}

void UiController::ensure_temperature_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_temp_info,
        temp_graph_label_min_,
        temp_graph_label_now_,
        temp_graph_label_max_);
}

void UiController::update_temperature_graph_overlays(const SensorGraphProfile &profile,
                                                     bool has_values,
                                                     float min_temp,
                                                     float max_temp,
                                                     float latest_temp) {
    if (!objects.chart_temp_info) {
        return;
    }

    ensure_temperature_graph_overlays();
    if (!temp_graph_label_min_ || !temp_graph_label_now_ || !temp_graph_label_max_) {
        return;
    }

    style_graph_stat_overlays(
        objects.chart_temp_info,
        temp_graph_label_min_,
        temp_graph_label_now_,
        temp_graph_label_max_);

    if (!has_values) {
        char min_empty[24];
        char now_empty[24];
        char max_empty[24];
        snprintf(min_empty, sizeof(min_empty), "%s --", profile.label_min ? profile.label_min : "MIN");
        snprintf(now_empty, sizeof(now_empty), "%s --", profile.label_now ? profile.label_now : "NOW");
        snprintf(max_empty, sizeof(max_empty), "%s --", profile.label_max ? profile.label_max : "MAX");
        safe_label_set_text(temp_graph_label_min_, min_empty);
        safe_label_set_text(temp_graph_label_now_, now_empty);
        safe_label_set_text(temp_graph_label_max_, max_empty);
        return;
    }

    const char *unit = profile.unit;
    char min_buf[32];
    char now_buf[32];
    char max_buf[32];
    snprintf(min_buf, sizeof(min_buf), "%s %.1f%s", profile.label_min ? profile.label_min : "MIN", min_temp, unit ? unit : "");
    snprintf(now_buf, sizeof(now_buf), "%s %.1f%s", profile.label_now ? profile.label_now : "NOW", latest_temp, unit ? unit : "");
    snprintf(max_buf, sizeof(max_buf), "%s %.1f%s", profile.label_max ? profile.label_max : "MAX", max_temp, unit ? unit : "");
    safe_label_set_text(temp_graph_label_min_, min_buf);
    safe_label_set_text(temp_graph_label_now_, now_buf);
    safe_label_set_text(temp_graph_label_max_, max_buf);
}

void UiController::ensure_temperature_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.temperature_info_graph,
        objects.chart_temp_info,
        temp_graph_zone_overlay_,
        temp_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_temperature_zone_overlay(const SensorGraphProfile &profile,
                                                   float y_min_display,
                                                   float y_max_display) {
    ensure_temperature_zone_overlay();

    update_graph_zone_overlay(objects.chart_temp_info,
                              temp_graph_zone_overlay_,
                              temp_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              profile.zone_bounds,
                              profile.zone_tones,
                              profile.zone_count,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_temperature_time_labels() {
    ensure_graph_time_labels(
        objects.temperature_info_graph,
        objects.chart_temp_info,
        temp_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_temperature_time_labels() {
    update_graph_time_labels(
        objects.temperature_info_graph,
        objects.chart_temp_info,
        temp_graph_time_labels_,
        kGraphTimeTickCount,
        temperature_graph_points());
}

void UiController::ensure_humidity_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_rh_info,
        rh_graph_label_min_,
        rh_graph_label_now_,
        rh_graph_label_max_);
}

void UiController::update_humidity_graph_overlays(bool has_values,
                                                  float min_humidity,
                                                  float max_humidity,
                                                  float latest_humidity) {
    update_graph_stat_overlay_labels(objects.chart_rh_info,
                                     rh_graph_label_min_,
                                     rh_graph_label_now_,
                                     rh_graph_label_max_,
                                     has_values,
                                     min_humidity,
                                     max_humidity,
                                     latest_humidity,
                                     "MIN %.0f%%",
                                     "NOW %.0f%%",
                                     "MAX %.0f%%");
}

void UiController::ensure_humidity_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.rh_info_graph,
        objects.chart_rh_info,
        rh_graph_zone_overlay_,
        rh_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_humidity_zone_overlay(float y_min_display, float y_max_display) {
    ensure_humidity_zone_overlay();

    const DisplayThresholds::Range rh = displayThresholds.snapshot().rh;
    const float kRhZoneBounds[kMaxGraphZoneBounds] = {
        -1000.0f,
        rh.orange_min,
        rh.yellow_min,
        rh.good_min,
        rh.good_max,
        rh.yellow_max,
        rh.orange_max,
        1000.0f};
    static const GraphZoneTone kRhZoneTones[kMaxGraphZoneBands] = {
        GRAPH_ZONE_RED,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };

    update_graph_zone_overlay(objects.chart_rh_info,
                              rh_graph_zone_overlay_,
                              rh_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kRhZoneBounds,
                              kRhZoneTones,
                              kMaxGraphZoneBands,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_humidity_time_labels() {
    ensure_graph_time_labels(
        objects.rh_info_graph,
        objects.chart_rh_info,
        rh_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_humidity_time_labels() {
    update_graph_time_labels(
        objects.rh_info_graph,
        objects.chart_rh_info,
        rh_graph_time_labels_,
        kGraphTimeTickCount,
        humidity_graph_points());
}

void UiController::update_temperature_info_graph() {
    if (!objects.chart_temp_info) {
        return;
    }

    const SensorGraphProfile profile = build_temperature_graph_profile();
    apply_temperature_graph_theme(profile);

    const uint16_t points = temperature_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_temp_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_temp_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_TEMPERATURE),
                                                              10.0f,
                                                              false,
                                                              true);
    const bool has_values = stats.has_values;
    float min_temp = stats.min_value;
    float max_temp = stats.max_value;
    float latest_temp = stats.latest_value;

    const float fallback = profile.fallback_value;

    float scale_min = min_temp;
    float scale_max = max_temp;
    if (has_values) {
        scale_min = min_temp;
        scale_max = max_temp;
    } else {
        scale_min = fallback;
        scale_max = fallback;
        latest_temp = fallback;
    }

    const float min_span = profile.min_span;
    const float span = scale_max - scale_min;
    float scale_span = span;
    if (!std::isfinite(scale_span) || scale_span < min_span) {
        scale_span = min_span;
    }
    float step = graph_nice_step(scale_span / 4.0f);
    if (!std::isfinite(step) || step <= 0.0f) {
        step = temp_units_c ? 0.5f : 1.0f;
    }
    float y_min_f = floorf((scale_min - (step * 0.9f)) / step) * step;
    float y_max_f = ceilf((scale_max + (step * 0.9f)) / step) * step;
    if ((y_max_f - y_min_f) < (step * 2.0f)) {
        y_min_f -= step;
        y_max_f += step;
    }
    if (!std::isfinite(y_min_f) || !std::isfinite(y_max_f) || y_max_f <= y_min_f) {
        const float center = std::isfinite(latest_temp) ? latest_temp : fallback;
        y_min_f = center - min_span;
        y_max_f = center + min_span;
    }

    lv_coord_t y_min = static_cast<lv_coord_t>(floorf(y_min_f * 10.0f));
    lv_coord_t y_max = static_cast<lv_coord_t>(ceilf(y_max_f * 10.0f));
    if (y_max <= y_min) {
        y_max = static_cast<lv_coord_t>(y_min + 10);
    }
    if ((y_max - y_min) < 10) {
        const lv_coord_t center = static_cast<lv_coord_t>((y_min + y_max) / 2);
        y_min = static_cast<lv_coord_t>(center - 5);
        y_max = static_cast<lv_coord_t>(center + 5);
    }
    int32_t horizontal_divisions = static_cast<int32_t>(lroundf((y_max_f - y_min_f) / step));
    const int32_t horizontal_min = (profile.horizontal_divisions_min > 0)
        ? static_cast<int32_t>(profile.horizontal_divisions_min)
        : 3;
    const int32_t horizontal_max = (profile.horizontal_divisions_max >= profile.horizontal_divisions_min &&
                                    profile.horizontal_divisions_max > 0)
        ? static_cast<int32_t>(profile.horizontal_divisions_max)
        : 12;
    if (horizontal_divisions < horizontal_min) {
        horizontal_divisions = horizontal_min;
    }
    if (horizontal_divisions > horizontal_max) {
        horizontal_divisions = horizontal_max;
    }
    const uint8_t vertical_divisions = (profile.vertical_divisions > 0) ? profile.vertical_divisions : 15;
    lv_chart_set_div_line_count(objects.chart_temp_info,
                                static_cast<uint8_t>(horizontal_divisions),
                                vertical_divisions);
    lv_chart_set_range(objects.chart_temp_info, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    update_temperature_zone_overlay(profile, y_min_f, y_max_f);

    if (has_values) {
        if (!std::isfinite(latest_temp)) {
            latest_temp = max_temp;
        }
        update_temperature_graph_overlays(profile, true, min_temp, max_temp, latest_temp);
    } else {
        update_temperature_graph_overlays(profile, false, fallback, fallback, fallback);
    }
    update_temperature_time_labels();

    lv_chart_refresh(objects.chart_temp_info);
    mark_active_graph_refreshed(INFO_TEMP, temp_graph_range_, points);
}

void UiController::update_humidity_info_graph() {
    if (!objects.chart_rh_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(rh_graph_range_);
    apply_standard_info_chart_theme(objects.chart_rh_info, 5, vertical_divisions);

    const uint16_t points = humidity_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_rh_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_rh_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_HUMIDITY),
                                                              10.0f,
                                                              false);
    const bool has_values = stats.has_values;
    float min_h = stats.min_value;
    float max_h = stats.max_value;
    float latest_h = stats.latest_value;

    float scale_min = has_values ? min_h : 50.0f;
    float scale_max = has_values ? max_h : 50.0f;
    float scale_span = scale_max - scale_min;
    if (!std::isfinite(scale_span) || scale_span < 10.0f) {
        scale_span = 10.0f;
    }

    float step = graph_nice_step(scale_span / 4.0f);
    if (!std::isfinite(step) || step <= 0.0f) {
        step = 5.0f;
    }

    float y_min_f = floorf((scale_min - (step * 0.9f)) / step) * step;
    float y_max_f = ceilf((scale_max + (step * 0.9f)) / step) * step;
    if ((y_max_f - y_min_f) < (step * 2.0f)) {
        y_min_f -= step;
        y_max_f += step;
    }
    if (!std::isfinite(y_min_f) || !std::isfinite(y_max_f) || y_max_f <= y_min_f) {
        y_min_f = 0.0f;
        y_max_f = 100.0f;
    }

    if (y_min_f < 0.0f) {
        y_min_f = 0.0f;
    }
    if (y_max_f > 100.0f) {
        y_max_f = 100.0f;
    }
    if (y_max_f <= y_min_f) {
        y_min_f = 0.0f;
        y_max_f = 100.0f;
    }

    lv_coord_t y_min = static_cast<lv_coord_t>(floorf(y_min_f * 10.0f));
    lv_coord_t y_max = static_cast<lv_coord_t>(ceilf(y_max_f * 10.0f));
    if (y_max <= y_min) {
        y_max = static_cast<lv_coord_t>(y_min + 10);
    }

    int32_t horizontal_divisions = static_cast<int32_t>(lroundf((y_max_f - y_min_f) / step));
    if (horizontal_divisions < 3) {
        horizontal_divisions = 3;
    }
    if (horizontal_divisions > 12) {
        horizontal_divisions = 12;
    }

    lv_chart_set_div_line_count(objects.chart_rh_info,
                                static_cast<uint8_t>(horizontal_divisions),
                                vertical_divisions);
    lv_chart_set_range(objects.chart_rh_info, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    update_humidity_zone_overlay(y_min_f, y_max_f);

    if (has_values) {
        if (!std::isfinite(latest_h)) {
            latest_h = max_h;
        }
        update_humidity_graph_overlays(true, min_h, max_h, latest_h);
    } else {
        update_humidity_graph_overlays(false, 50.0f, 50.0f, 50.0f);
    }
    update_humidity_time_labels();

    lv_chart_refresh(objects.chart_rh_info);
    mark_active_graph_refreshed(INFO_RH, rh_graph_range_, points);
}

