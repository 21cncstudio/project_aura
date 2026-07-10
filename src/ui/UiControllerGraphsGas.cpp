// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "config/AppConfig.h"
#include "modules/ChartsHistory.h"
#include "ui/UiOptionalGasProfile.h"
#include "ui/UiText.h"
#include "ui/ui.h"

#include "ui/UiControllerGraphsShared.h"

namespace {

constexpr float kGraphZoneLowerSentinel = -1000.0f;
constexpr float kGraphZoneUpperSentinel = 100000.0f;

} // namespace

void UiController::ensure_voc_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_voc_info,
        voc_graph_label_min_,
        voc_graph_label_now_,
        voc_graph_label_max_);
}

void UiController::update_voc_graph_overlays(bool has_values,
                                             float min_voc,
                                             float max_voc,
                                             float latest_voc) {
    update_graph_stat_overlay_labels(objects.chart_voc_info,
                                     voc_graph_label_min_,
                                     voc_graph_label_now_,
                                     voc_graph_label_max_,
                                     has_values,
                                     min_voc,
                                     max_voc,
                                     latest_voc,
                                     "MIN %.0f idx",
                                     "NOW %.0f idx",
                                     "MAX %.0f idx");
}

void UiController::ensure_voc_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.voc_info_graph,
        objects.chart_voc_info,
        voc_graph_zone_overlay_,
        voc_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_voc_zone_overlay(float y_min_display, float y_max_display) {
    ensure_voc_zone_overlay();

    const DisplayThresholds::High voc = displayThresholds.snapshot().voc;
    const float kVocZoneBounds[] = {
        kGraphZoneLowerSentinel,
        voc.green,
        voc.yellow,
        voc.orange,
        kGraphZoneUpperSentinel};
    static const GraphZoneTone kVocZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kVocZoneCount = 4;

    update_graph_zone_overlay(objects.chart_voc_info,
                              voc_graph_zone_overlay_,
                              voc_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kVocZoneBounds,
                              kVocZoneTones,
                              kVocZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_voc_time_labels() {
    ensure_graph_time_labels(
        objects.voc_info_graph,
        objects.chart_voc_info,
        voc_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_voc_time_labels() {
    update_graph_time_labels(
        objects.voc_info_graph,
        objects.chart_voc_info,
        voc_graph_time_labels_,
        kGraphTimeTickCount,
        voc_graph_points());
}

void UiController::update_voc_info_graph() {
    if (!objects.chart_voc_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(voc_graph_range_);
    apply_standard_info_chart_theme(objects.chart_voc_info, 5, vertical_divisions);

    const uint16_t points = voc_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_voc_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_voc_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_VOC),
                                                              1.0f,
                                                              false);
    const bool has_values = stats.has_values;
    float min_voc = stats.min_value;
    float max_voc = stats.max_value;
    float latest_voc = stats.latest_value;

    float scale_min = has_values ? min_voc : 100.0f;
    float scale_max = has_values ? max_voc : 100.0f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = 100.0f;
    axis_config.min_span = 80.0f;
    axis_config.fallback_half_span = 80.0f;
    axis_config.fallback_step = 25.0f;
    axis_config.point_scale = 1.0f;
    axis_config.clamp_min_zero = false;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_voc, axis_config);

    lv_chart_set_div_line_count(objects.chart_voc_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_voc_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_voc_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_voc)) {
            latest_voc = max_voc;
        }
        update_voc_graph_overlays(true, min_voc, max_voc, latest_voc);
    } else {
        update_voc_graph_overlays(false, 100.0f, 100.0f, 100.0f);
    }
    update_voc_time_labels();

    lv_chart_refresh(objects.chart_voc_info);
    mark_active_graph_refreshed(INFO_VOC, voc_graph_range_, points);
}

void UiController::ensure_nox_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_nox_info,
        nox_graph_label_min_,
        nox_graph_label_now_,
        nox_graph_label_max_);
}

void UiController::update_nox_graph_overlays(bool has_values,
                                             float min_nox,
                                             float max_nox,
                                             float latest_nox) {
    update_graph_stat_overlay_labels(objects.chart_nox_info,
                                     nox_graph_label_min_,
                                     nox_graph_label_now_,
                                     nox_graph_label_max_,
                                     has_values,
                                     min_nox,
                                     max_nox,
                                     latest_nox,
                                     "MIN %.0f idx",
                                     "NOW %.0f idx",
                                     "MAX %.0f idx");
}

void UiController::ensure_nox_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.nox_info_graph,
        objects.chart_nox_info,
        nox_graph_zone_overlay_,
        nox_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_nox_zone_overlay(float y_min_display, float y_max_display) {
    ensure_nox_zone_overlay();

    const DisplayThresholds::High nox = displayThresholds.snapshot().nox;
    const float kNoxZoneBounds[] = {
        kGraphZoneLowerSentinel,
        nox.green,
        nox.yellow,
        nox.orange,
        kGraphZoneUpperSentinel};
    static const GraphZoneTone kNoxZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kNoxZoneCount = 4;

    update_graph_zone_overlay(objects.chart_nox_info,
                              nox_graph_zone_overlay_,
                              nox_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kNoxZoneBounds,
                              kNoxZoneTones,
                              kNoxZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_nox_time_labels() {
    ensure_graph_time_labels(
        objects.nox_info_graph,
        objects.chart_nox_info,
        nox_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_nox_time_labels() {
    update_graph_time_labels(
        objects.nox_info_graph,
        objects.chart_nox_info,
        nox_graph_time_labels_,
        kGraphTimeTickCount,
        nox_graph_points());
}

void UiController::update_nox_info_graph() {
    if (!objects.chart_nox_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(nox_graph_range_);
    apply_standard_info_chart_theme(objects.chart_nox_info, 5, vertical_divisions);

    const uint16_t points = nox_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_nox_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_nox_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_NOX),
                                                              1.0f,
                                                              false);
    const bool has_values = stats.has_values;
    float min_nox = stats.min_value;
    float max_nox = stats.max_value;
    float latest_nox = stats.latest_value;

    float scale_min = has_values ? min_nox : 50.0f;
    float scale_max = has_values ? max_nox : 50.0f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = 50.0f;
    axis_config.min_span = 60.0f;
    axis_config.fallback_half_span = 60.0f;
    axis_config.fallback_step = 20.0f;
    axis_config.point_scale = 1.0f;
    axis_config.clamp_min_zero = false;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_nox, axis_config);

    lv_chart_set_div_line_count(objects.chart_nox_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_nox_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_nox_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_nox)) {
            latest_nox = max_nox;
        }
        update_nox_graph_overlays(true, min_nox, max_nox, latest_nox);
    } else {
        update_nox_graph_overlays(false, 50.0f, 50.0f, 50.0f);
    }
    update_nox_time_labels();

    lv_chart_refresh(objects.chart_nox_info);
    mark_active_graph_refreshed(INFO_NOX, nox_graph_range_, points);
}

void UiController::ensure_optional_gas_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_optional_gas_info,
        optional_gas_graph_label_min_,
        optional_gas_graph_label_now_,
        optional_gas_graph_label_max_);
}

void UiController::update_optional_gas_graph_overlays(bool has_values,
                                                      float min_ppm,
                                                      float max_ppm,
                                                      float latest_ppm) {
    if (!objects.chart_optional_gas_info) {
        return;
    }

    ensure_optional_gas_graph_overlays();
    if (!optional_gas_graph_label_min_ ||
        !optional_gas_graph_label_now_ ||
        !optional_gas_graph_label_max_) {
        return;
    }

    style_graph_stat_overlays(objects.chart_optional_gas_info,
                              optional_gas_graph_label_min_,
                              optional_gas_graph_label_now_,
                              optional_gas_graph_label_max_);

    if (!has_values) {
        safe_label_set_text(optional_gas_graph_label_min_, "MIN --");
        safe_label_set_text(optional_gas_graph_label_now_, "NOW --");
        safe_label_set_text(optional_gas_graph_label_max_, "MAX --");
        return;
    }

    using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;
    const OptionalGasType type = static_cast<OptionalGasType>(currentData.optional_gas_type);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);

    char min_value[16];
    char now_value[16];
    char max_value[16];
    UiOptionalGasProfile::formatValue(profile,
                                      min_ppm,
                                      currentData.optional_gas_ppm_decimals,
                                      min_value,
                                      sizeof(min_value));
    UiOptionalGasProfile::formatValue(profile,
                                      latest_ppm,
                                      currentData.optional_gas_ppm_decimals,
                                      now_value,
                                      sizeof(now_value));
    UiOptionalGasProfile::formatValue(profile,
                                      max_ppm,
                                      currentData.optional_gas_ppm_decimals,
                                      max_value,
                                      sizeof(max_value));

    char min_buf[32];
    char now_buf[32];
    char max_buf[32];
    snprintf(min_buf, sizeof(min_buf), "MIN %s ppm", min_value);
    snprintf(now_buf, sizeof(now_buf), "NOW %s ppm", now_value);
    snprintf(max_buf, sizeof(max_buf), "MAX %s ppm", max_value);
    safe_label_set_text(optional_gas_graph_label_min_, min_buf);
    safe_label_set_text(optional_gas_graph_label_now_, now_buf);
    safe_label_set_text(optional_gas_graph_label_max_, max_buf);
}

void UiController::ensure_optional_gas_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.optional_gas_info_graph,
        objects.chart_optional_gas_info,
        optional_gas_graph_zone_overlay_,
        optional_gas_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_optional_gas_zone_overlay(float y_min_display, float y_max_display) {
    ensure_optional_gas_zone_overlay();

    using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;
    const OptionalGasType type = static_cast<OptionalGasType>(currentData.optional_gas_type);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    const float zone_bounds[] = {
        kGraphZoneLowerSentinel,
        profile.green_max_ppm,
        profile.yellow_max_ppm,
        profile.orange_max_ppm,
        kGraphZoneUpperSentinel,
    };
    const GraphZoneTone zone_tones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kZoneCount = 4;

    update_graph_zone_overlay(objects.chart_optional_gas_info,
                              optional_gas_graph_zone_overlay_,
                              optional_gas_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              zone_bounds,
                              zone_tones,
                              kZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_optional_gas_time_labels() {
    ensure_graph_time_labels(
        objects.optional_gas_info_graph,
        objects.chart_optional_gas_info,
        optional_gas_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_optional_gas_time_labels() {
    update_graph_time_labels(
        objects.optional_gas_info_graph,
        objects.chart_optional_gas_info,
        optional_gas_graph_time_labels_,
        kGraphTimeTickCount,
        optional_gas_graph_points());
}

void UiController::update_optional_gas_info_graph() {
    if (!objects.chart_optional_gas_info) {
        return;
    }

    using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;
    const OptionalGasType type = static_cast<OptionalGasType>(currentData.optional_gas_type);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    const float point_scale = (profile.graph_scale > 0.0f) ? profile.graph_scale : 100.0f;

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(optional_gas_graph_range_);
    apply_standard_info_chart_theme(objects.chart_optional_gas_info, 5, vertical_divisions);

    const uint16_t points = optional_gas_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_optional_gas_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_optional_gas_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_OPTIONAL_GAS),
                                                              point_scale,
                                                              true);
    const bool has_values = stats.has_values;
    float min_ppm = stats.min_value;
    float max_ppm = stats.max_value;
    float latest_ppm = stats.latest_value;

    float scale_min = has_values ? min_ppm : 0.0f;
    float scale_max = has_values ? max_ppm : profile.orange_max_ppm;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = profile.graph_fallback_ppm;
    axis_config.min_span = profile.graph_min_span_ppm;
    axis_config.fallback_half_span = profile.graph_min_span_ppm * 0.5f;
    axis_config.fallback_step = profile.graph_min_span_ppm / 4.0f;
    axis_config.last_resort_step = 0.1f;
    axis_config.point_scale = point_scale;
    axis_config.min_coord_span = 1;
    axis_config.clamp_min_zero = true;
    axis_config.ensure_display_span_after_clamp = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_ppm, axis_config);

    lv_chart_set_div_line_count(objects.chart_optional_gas_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_optional_gas_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_optional_gas_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_ppm)) {
            latest_ppm = max_ppm;
        }
        update_optional_gas_graph_overlays(true, min_ppm, max_ppm, latest_ppm);
    } else {
        const float fallback = profile.graph_fallback_ppm;
        update_optional_gas_graph_overlays(false, fallback, fallback, fallback);
    }
    update_optional_gas_time_labels();

    lv_chart_refresh(objects.chart_optional_gas_info);
    mark_active_graph_refreshed(INFO_OPTIONAL_GAS, optional_gas_graph_range_, points);
}

void UiController::ensure_hcho_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_hcho_info,
        hcho_graph_label_min_,
        hcho_graph_label_now_,
        hcho_graph_label_max_);
}

void UiController::update_hcho_graph_overlays(bool has_values,
                                              float min_hcho,
                                              float max_hcho,
                                              float latest_hcho) {
    update_graph_stat_overlay_labels(objects.chart_hcho_info,
                                     hcho_graph_label_min_,
                                     hcho_graph_label_now_,
                                     hcho_graph_label_max_,
                                     has_values,
                                     min_hcho,
                                     max_hcho,
                                     latest_hcho,
                                     "MIN %.0f ppb",
                                     "NOW %.0f ppb",
                                     "MAX %.0f ppb");
}

void UiController::ensure_hcho_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.hcho_info_graph,
        objects.chart_hcho_info,
        hcho_graph_zone_overlay_,
        hcho_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_hcho_zone_overlay(float y_min_display, float y_max_display) {
    ensure_hcho_zone_overlay();

    const DisplayThresholds::High hcho = displayThresholds.snapshot().hcho;
    const float kHchoZoneBounds[] = {
        kGraphZoneLowerSentinel,
        hcho.green,
        hcho.yellow,
        hcho.orange,
        kGraphZoneUpperSentinel};
    static const GraphZoneTone kHchoZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kHchoZoneCount = 4;

    update_graph_zone_overlay(objects.chart_hcho_info,
                              hcho_graph_zone_overlay_,
                              hcho_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kHchoZoneBounds,
                              kHchoZoneTones,
                              kHchoZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_hcho_time_labels() {
    ensure_graph_time_labels(
        objects.hcho_info_graph,
        objects.chart_hcho_info,
        hcho_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_hcho_time_labels() {
    update_graph_time_labels(
        objects.hcho_info_graph,
        objects.chart_hcho_info,
        hcho_graph_time_labels_,
        kGraphTimeTickCount,
        hcho_graph_points());
}

void UiController::update_hcho_info_graph() {
    if (!objects.chart_hcho_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(hcho_graph_range_);
    apply_standard_info_chart_theme(objects.chart_hcho_info, 5, vertical_divisions);

    const uint16_t points = hcho_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_hcho_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_hcho_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_HCHO),
                                                              1.0f,
                                                              false);
    const bool has_values = stats.has_values;
    float min_hcho = stats.min_value;
    float max_hcho = stats.max_value;
    float latest_hcho = stats.latest_value;

    float scale_min = has_values ? min_hcho : 20.0f;
    float scale_max = has_values ? max_hcho : 20.0f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = 20.0f;
    axis_config.min_span = 40.0f;
    axis_config.fallback_half_span = 40.0f;
    axis_config.fallback_step = 10.0f;
    axis_config.point_scale = 1.0f;
    axis_config.clamp_min_zero = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_hcho, axis_config);

    lv_chart_set_div_line_count(objects.chart_hcho_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_hcho_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_hcho_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_hcho)) {
            latest_hcho = max_hcho;
        }
        update_hcho_graph_overlays(true, min_hcho, max_hcho, latest_hcho);
    } else {
        update_hcho_graph_overlays(false, 20.0f, 20.0f, 20.0f);
    }
    update_hcho_time_labels();

    lv_chart_refresh(objects.chart_hcho_info);
    mark_active_graph_refreshed(INFO_HCHO, hcho_graph_range_, points);
}

void UiController::ensure_co2_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_co2_info,
        co2_graph_label_min_,
        co2_graph_label_now_,
        co2_graph_label_max_);
}

void UiController::update_co2_graph_overlays(bool has_values,
                                             float min_co2,
                                             float max_co2,
                                             float latest_co2) {
    update_graph_stat_overlay_labels(objects.chart_co2_info,
                                     co2_graph_label_min_,
                                     co2_graph_label_now_,
                                     co2_graph_label_max_,
                                     has_values,
                                     min_co2,
                                     max_co2,
                                     latest_co2,
                                     "MIN %.0f ppm",
                                     "NOW %.0f ppm",
                                     "MAX %.0f ppm");
}

void UiController::ensure_co2_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.co2_info_graph,
        objects.chart_co2_info,
        co2_graph_zone_overlay_,
        co2_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_co2_zone_overlay(float y_min_display, float y_max_display) {
    ensure_co2_zone_overlay();

    const DisplayThresholds::High co2 = displayThresholds.snapshot().co2;
    const float kCo2ZoneBounds[] = {
        kGraphZoneLowerSentinel,
        co2.green,
        co2.yellow,
        co2.orange,
        kGraphZoneUpperSentinel};
    static const GraphZoneTone kCo2ZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kCo2ZoneCount = 4;

    update_graph_zone_overlay(objects.chart_co2_info,
                              co2_graph_zone_overlay_,
                              co2_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kCo2ZoneBounds,
                              kCo2ZoneTones,
                              kCo2ZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_co2_time_labels() {
    ensure_graph_time_labels(
        objects.co2_info_graph,
        objects.chart_co2_info,
        co2_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_co2_time_labels() {
    update_graph_time_labels(
        objects.co2_info_graph,
        objects.chart_co2_info,
        co2_graph_time_labels_,
        kGraphTimeTickCount,
        co2_graph_points());
}

void UiController::update_co2_info_graph() {
    if (!objects.chart_co2_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(co2_graph_range_);
    apply_standard_info_chart_theme(objects.chart_co2_info, 5, vertical_divisions);

    const uint16_t points = co2_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_co2_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_co2_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_CO2),
                                                              1.0f,
                                                              false);
    const bool has_values = stats.has_values;
    float min_co2 = stats.min_value;
    float max_co2 = stats.max_value;
    float latest_co2 = stats.latest_value;

    float scale_min = has_values ? min_co2 : 700.0f;
    float scale_max = has_values ? max_co2 : 700.0f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = 700.0f;
    axis_config.min_span = 150.0f;
    axis_config.fallback_half_span = 150.0f;
    axis_config.fallback_step = 50.0f;
    axis_config.point_scale = 1.0f;
    axis_config.clamp_min_zero = false;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_co2, axis_config);

    lv_chart_set_div_line_count(objects.chart_co2_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_co2_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_co2_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_co2)) {
            latest_co2 = max_co2;
        }
        update_co2_graph_overlays(true, min_co2, max_co2, latest_co2);
    } else {
        update_co2_graph_overlays(false, 700.0f, 700.0f, 700.0f);
    }
    update_co2_time_labels();

    lv_chart_refresh(objects.chart_co2_info);
    mark_active_graph_refreshed(INFO_CO2, co2_graph_range_, points);
}

void UiController::ensure_co_graph_overlays() {
    ensure_graph_stat_overlays(
        objects.chart_co_info,
        co_graph_label_min_,
        co_graph_label_now_,
        co_graph_label_max_);
}

void UiController::update_co_graph_overlays(bool has_values,
                                            float min_co,
                                            float max_co,
                                            float latest_co) {
    update_graph_stat_overlay_labels(objects.chart_co_info,
                                     co_graph_label_min_,
                                     co_graph_label_now_,
                                     co_graph_label_max_,
                                     has_values,
                                     min_co,
                                     max_co,
                                     latest_co,
                                     "MIN %.1f ppm",
                                     "NOW %.1f ppm",
                                     "MAX %.1f ppm");
}

void UiController::ensure_co_zone_overlay() {
    ensure_graph_zone_overlay(
        objects.co_info_graph,
        objects.chart_co_info,
        co_graph_zone_overlay_,
        co_graph_zone_bands_,
        kMaxGraphZoneBands);
}

void UiController::update_co_zone_overlay(float y_min_display, float y_max_display) {
    ensure_co_zone_overlay();

    const DisplayThresholds::High co = displayThresholds.snapshot().co;
    const float kCoZoneBounds[] = {
        kGraphZoneLowerSentinel,
        co.green,
        co.yellow,
        co.orange,
        kGraphZoneUpperSentinel,
    };
    static const GraphZoneTone kCoZoneTones[] = {
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_RED,
    };
    constexpr uint8_t kCoZoneCount = 4;

    update_graph_zone_overlay(objects.chart_co_info,
                              co_graph_zone_overlay_,
                              co_graph_zone_bands_,
                              kMaxGraphZoneBands,
                              kCoZoneBounds,
                              kCoZoneTones,
                              kCoZoneCount,
                              y_min_display,
                              y_max_display);
}

void UiController::ensure_co_time_labels() {
    ensure_graph_time_labels(
        objects.co_info_graph,
        objects.chart_co_info,
        co_graph_time_labels_,
        kGraphTimeTickCount);
}

void UiController::update_co_time_labels() {
    update_graph_time_labels(
        objects.co_info_graph,
        objects.chart_co_info,
        co_graph_time_labels_,
        kGraphTimeTickCount,
        co_graph_points());
}

void UiController::update_co_info_graph() {
    if (!objects.chart_co_info) {
        return;
    }

    const uint8_t vertical_divisions = graph_vertical_divisions_for_range(co_graph_range_);
    apply_standard_info_chart_theme(objects.chart_co_info, 5, vertical_divisions);

    const uint16_t points = co_graph_points();
    lv_chart_series_t *series = ensure_info_chart_series(objects.chart_co_info, points);
    if (!series) {
        return;
    }

    const GraphSeriesStats stats = populate_info_chart_series(objects.chart_co_info,
                                                              series,
                                                              points,
                                                              static_cast<int>(ChartsHistory::METRIC_CO),
                                                              10.0f,
                                                              true);
    const bool has_values = stats.has_values;
    float min_co = stats.min_value;
    float max_co = stats.max_value;
    float latest_co = stats.latest_value;

    float scale_min = has_values ? min_co : 0.0f;
    float scale_max = has_values ? max_co : Config::AQ_CO_GREEN_MAX_PPM;
    const float fallback = Config::AQ_CO_GREEN_MAX_PPM * 0.5f;
    GraphAxisConfig axis_config{};
    axis_config.fallback_center = fallback;
    axis_config.min_span = 5.0f;
    axis_config.fallback_half_span = 5.0f;
    axis_config.fallback_step = 1.0f;
    axis_config.point_scale = 10.0f;
    axis_config.clamp_min_zero = true;
    const GraphAxisRange axis = compute_standard_graph_axis(scale_min, scale_max, latest_co, axis_config);

    lv_chart_set_div_line_count(objects.chart_co_info,
                                axis.horizontal_divisions,
                                vertical_divisions);
    lv_chart_set_range(objects.chart_co_info, LV_CHART_AXIS_PRIMARY_Y, axis.y_min, axis.y_max);
    update_co_zone_overlay(axis.y_min_display, axis.y_max_display);

    if (has_values) {
        if (!isfinite(latest_co)) {
            latest_co = max_co;
        }
        update_co_graph_overlays(true, min_co, max_co, latest_co);
    } else {
        update_co_graph_overlays(false, fallback, fallback, fallback);
    }
    update_co_time_labels();

    lv_chart_refresh(objects.chart_co_info);
    mark_active_graph_refreshed(INFO_CO, co_graph_range_, points);
}

