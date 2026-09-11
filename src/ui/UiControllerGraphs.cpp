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
void release_lvgl_obj(lv_obj_t *&obj) {
    if (obj && lv_obj_is_valid(obj)) {
        lv_obj_del(obj);
    }
    obj = nullptr;
}

void release_lvgl_obj_array(lv_obj_t **objs, uint8_t count) {
    if (!objs) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        if (objs[i] && lv_obj_is_valid(objs[i])) {
            lv_obj_del(objs[i]);
        }
        objs[i] = nullptr;
    }
}

void set_checked(lv_obj_t *btn, bool checked) {
    if (!btn) {
        return;
    }
    if (checked) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
}

uint32_t graph_color_token(lv_color_t color) {
    return static_cast<uint32_t>(color.full);
}

void hide_graph_zone_bands(lv_obj_t **bands, uint8_t band_count) {
    if (!bands) {
        return;
    }
    for (uint8_t i = 0; i < band_count; ++i) {
        if (bands[i]) {
            lv_obj_add_flag(bands[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
} // namespace

void UiController::release_all_sensor_graph_runtime_objects() {
    release_lvgl_obj(temp_graph_label_min_);
    release_lvgl_obj(temp_graph_label_now_);
    release_lvgl_obj(temp_graph_label_max_);
    release_lvgl_obj(rh_graph_label_min_);
    release_lvgl_obj(rh_graph_label_now_);
    release_lvgl_obj(rh_graph_label_max_);
    release_lvgl_obj(voc_graph_label_min_);
    release_lvgl_obj(voc_graph_label_now_);
    release_lvgl_obj(voc_graph_label_max_);
    release_lvgl_obj(nox_graph_label_min_);
    release_lvgl_obj(nox_graph_label_now_);
    release_lvgl_obj(nox_graph_label_max_);
    release_lvgl_obj(optional_gas_graph_label_min_);
    release_lvgl_obj(optional_gas_graph_label_now_);
    release_lvgl_obj(optional_gas_graph_label_max_);
    release_lvgl_obj(hcho_graph_label_min_);
    release_lvgl_obj(hcho_graph_label_now_);
    release_lvgl_obj(hcho_graph_label_max_);
    release_lvgl_obj(co2_graph_label_min_);
    release_lvgl_obj(co2_graph_label_now_);
    release_lvgl_obj(co2_graph_label_max_);
    release_lvgl_obj(co_graph_label_min_);
    release_lvgl_obj(co_graph_label_now_);
    release_lvgl_obj(co_graph_label_max_);
    release_lvgl_obj(pm05_graph_label_min_);
    release_lvgl_obj(pm05_graph_label_now_);
    release_lvgl_obj(pm05_graph_label_max_);
    release_lvgl_obj(pm25_4_graph_label_min_);
    release_lvgl_obj(pm25_4_graph_label_now_);
    release_lvgl_obj(pm25_4_graph_label_max_);
    release_lvgl_obj(pm1_10_graph_label_min_);
    release_lvgl_obj(pm1_10_graph_label_now_);
    release_lvgl_obj(pm1_10_graph_label_max_);
    release_lvgl_obj(pressure_graph_label_min_);
    release_lvgl_obj(pressure_graph_label_now_);
    release_lvgl_obj(pressure_graph_label_max_);

    release_lvgl_obj_array(temp_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(rh_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(voc_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(nox_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(optional_gas_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(hcho_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(co2_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(co_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(pm05_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(pm25_4_graph_zone_bands_, kMaxGraphZoneBands);
    release_lvgl_obj_array(pm1_10_graph_zone_bands_, kMaxGraphZoneBands);

    release_lvgl_obj(temp_graph_zone_overlay_);
    release_lvgl_obj(rh_graph_zone_overlay_);
    release_lvgl_obj(voc_graph_zone_overlay_);
    release_lvgl_obj(nox_graph_zone_overlay_);
    release_lvgl_obj(optional_gas_graph_zone_overlay_);
    release_lvgl_obj(hcho_graph_zone_overlay_);
    release_lvgl_obj(co2_graph_zone_overlay_);
    release_lvgl_obj(co_graph_zone_overlay_);
    release_lvgl_obj(pm05_graph_zone_overlay_);
    release_lvgl_obj(pm25_4_graph_zone_overlay_);
    release_lvgl_obj(pm1_10_graph_zone_overlay_);

    release_lvgl_obj_array(temp_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(rh_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(voc_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(nox_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(optional_gas_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(hcho_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(co2_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(co_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(pm05_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(pm25_4_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(pm1_10_graph_time_labels_, kGraphTimeTickCount);
    release_lvgl_obj_array(pressure_graph_time_labels_, kGraphTimeTickCount);

    invalidate_active_graph_refresh_cache();
}

bool UiController::should_refresh_active_graph(InfoSensor sensor, TempGraphRange range, uint16_t points) {
    constexpr uint32_t kGraphRefreshHeartbeatMs = 5000UL;
    const uint16_t history_count = chartsHistory.count();
    const uint32_t history_epoch = chartsHistory.latestEpoch();
    const uint32_t theme_sig = active_graph_theme_signature();
    const uint32_t now_ms = millis();

    const bool key_changed =
        (sensor != graph_refresh_sensor_) ||
        (range != graph_refresh_range_) ||
        (points != graph_refresh_points_) ||
        (temp_units_c != graph_refresh_units_c_) ||
        (night_mode != graph_refresh_night_mode_) ||
        (theme_sig != graph_refresh_theme_sig_);
    const bool history_changed =
        (history_count != graph_refresh_history_count_) ||
        (history_epoch != graph_refresh_epoch_);
    const bool heartbeat_due =
        (graph_refresh_last_ms_ == 0U) ||
        ((now_ms - graph_refresh_last_ms_) >= kGraphRefreshHeartbeatMs);

    return key_changed || history_changed || heartbeat_due;
}

void UiController::mark_active_graph_refreshed(InfoSensor sensor, TempGraphRange range, uint16_t points) {
    graph_refresh_sensor_ = sensor;
    graph_refresh_range_ = range;
    graph_refresh_points_ = points;
    graph_refresh_history_count_ = chartsHistory.count();
    graph_refresh_epoch_ = chartsHistory.latestEpoch();
    graph_refresh_units_c_ = temp_units_c;
    graph_refresh_night_mode_ = night_mode;
    graph_refresh_theme_sig_ = active_graph_theme_signature();
    graph_refresh_last_ms_ = millis();
}

void UiController::invalidate_active_graph_refresh_cache() {
    graph_refresh_sensor_ = INFO_NONE;
    graph_refresh_range_ = TEMP_GRAPH_RANGE_3H;
    graph_refresh_points_ = 0;
    graph_refresh_history_count_ = 0;
    graph_refresh_epoch_ = 0;
    graph_refresh_units_c_ = temp_units_c;
    graph_refresh_night_mode_ = night_mode;
    graph_refresh_theme_sig_ = active_graph_theme_signature();
    graph_refresh_last_ms_ = 0;
}

uint32_t UiController::active_graph_theme_signature() {
    lv_color_t card_bg = lv_color_hex(0xff160c09);
    lv_color_t card_border = color_card_border();
    if (objects.card_co2_pro) {
        card_bg = lv_obj_get_style_bg_color(objects.card_co2_pro, LV_PART_MAIN);
        card_border = lv_obj_get_style_border_color(objects.card_co2_pro, LV_PART_MAIN);
    }

    uint32_t signature = graph_color_token(active_text_color());
    signature = (signature * 16777619UL) ^ graph_color_token(card_border);
    signature = (signature * 16777619UL) ^ graph_color_token(card_bg);
    return signature;
}

uint16_t UiController::graph_points_for_range(TempGraphRange range) const {
    switch (range) {
        case TEMP_GRAPH_RANGE_1H:
            return Config::CHART_HISTORY_1H_STEPS;
        case TEMP_GRAPH_RANGE_24H:
            return Config::CHART_HISTORY_24H_SAMPLES;
        case TEMP_GRAPH_RANGE_3H:
        default:
            return Config::CHART_HISTORY_3H_STEPS;
    }
}

uint16_t UiController::temperature_graph_points() const {
    return graph_points_for_range(temp_graph_range_);
}

uint16_t UiController::humidity_graph_points() const {
    return graph_points_for_range(rh_graph_range_);
}

uint16_t UiController::voc_graph_points() const {
    return graph_points_for_range(voc_graph_range_);
}

uint16_t UiController::nox_graph_points() const {
    return graph_points_for_range(nox_graph_range_);
}

uint16_t UiController::optional_gas_graph_points() const {
    return graph_points_for_range(optional_gas_graph_range_);
}

uint16_t UiController::hcho_graph_points() const {
    return graph_points_for_range(hcho_graph_range_);
}

uint16_t UiController::co2_graph_points() const {
    return graph_points_for_range(co2_graph_range_);
}

uint16_t UiController::co_graph_points() const {
    return graph_points_for_range(co_graph_range_);
}

uint16_t UiController::pm05_graph_points() const {
    return graph_points_for_range(pm05_graph_range_);
}

uint16_t UiController::pm25_4_graph_points() const {
    return graph_points_for_range(pm25_4_graph_range_);
}

uint16_t UiController::pm1_10_graph_points() const {
    return graph_points_for_range(pm1_10_graph_range_);
}

uint16_t UiController::pressure_graph_points() const {
    return graph_points_for_range(pressure_graph_range_);
}

uint8_t UiController::graph_vertical_divisions_for_range(TempGraphRange range) const {
    return (range == TEMP_GRAPH_RANGE_24H) ? 25U : 13U;
}

void UiController::apply_standard_info_chart_theme(lv_obj_t *chart, uint8_t horizontal_divisions, uint8_t vertical_divisions) {
    if (!chart) {
        return;
    }

    lv_color_t card_bg = lv_color_hex(0xff160c09);
    lv_color_t border_color = color_card_border();
    if (objects.card_co2_pro) {
        card_bg = lv_obj_get_style_bg_color(objects.card_co2_pro, LV_PART_MAIN);
        border_color = lv_obj_get_style_border_color(objects.card_co2_pro, LV_PART_MAIN);
    }

    const lv_color_t text_color = active_text_color();
    const lv_color_t grid_color = lv_color_mix(border_color, card_bg, LV_OPA_50);
    const lv_color_t line_color = lv_color_mix(border_color, text_color, LV_OPA_40);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart, horizontal_divisions, vertical_divisions);

    lv_obj_set_style_bg_color(chart, card_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(chart, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(chart, border_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(chart, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(chart, grid_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(chart, LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_line_color(chart, line_color, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(chart, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

lv_chart_series_t *UiController::ensure_info_chart_series(lv_obj_t *chart, uint16_t points) {
    if (!chart) {
        return nullptr;
    }

    const uint16_t desired_points = (points > 0U) ? points : 1U;
    if (lv_chart_get_point_count(chart) != desired_points) {
        lv_chart_set_point_count(chart, desired_points);
    }

    lv_chart_series_t *series = lv_chart_get_series_next(chart, nullptr);
    if (!series) {
        series = lv_chart_add_series(chart,
                                     lv_obj_get_style_line_color(chart, LV_PART_ITEMS),
                                     LV_CHART_AXIS_PRIMARY_Y);
    }
    if (!series) {
        return nullptr;
    }

    series->color = lv_obj_get_style_line_color(chart, LV_PART_ITEMS);
    lv_chart_set_all_value(chart, series, LV_CHART_POINT_NONE);
    return series;
}

UiController::GraphSeriesStats UiController::populate_info_chart_series(lv_obj_t *chart,
                                                                        lv_chart_series_t *series,
                                                                        uint16_t points,
                                                                        int metric_id,
                                                                        float point_scale,
                                                                        bool require_non_negative,
                                                                        bool convert_temperature_to_display) {
    GraphSeriesStats stats{};
    stats.has_values = false;
    stats.min_value = FLT_MAX;
    stats.max_value = -FLT_MAX;
    stats.latest_value = NAN;

    if (!chart || !series || points == 0) {
        return stats;
    }

    const uint16_t total_count = chartsHistory.count();
    const uint16_t available = (total_count < points) ? total_count : points;
    const uint16_t missing_prefix = points - available;
    const uint16_t start_offset = total_count - available;
    const ChartsHistory::Metric metric = static_cast<ChartsHistory::Metric>(metric_id);

    for (uint16_t i = 0; i < points; ++i) {
        lv_coord_t point_value = LV_CHART_POINT_NONE;
        if (i >= missing_prefix) {
            const uint16_t offset = start_offset + (i - missing_prefix);
            float raw_value = 0.0f;
            bool valid = false;
            if (chartsHistory.metricValueFromOldest(offset, metric, raw_value, valid) &&
                valid && std::isfinite(raw_value)) {
                float display_value = raw_value;
                if (convert_temperature_to_display) {
                    display_value = temperature_to_display(raw_value, temp_units_c);
                }
                if (std::isfinite(display_value) && (!require_non_negative || display_value >= 0.0f)) {
                    if (!stats.has_values) {
                        stats.min_value = display_value;
                        stats.max_value = display_value;
                        stats.has_values = true;
                    } else {
                        if (display_value < stats.min_value) {
                            stats.min_value = display_value;
                        }
                        if (display_value > stats.max_value) {
                            stats.max_value = display_value;
                        }
                    }
                    stats.latest_value = display_value;
                    point_value = static_cast<lv_coord_t>(lroundf(display_value * point_scale));
                }
            }
        }
        lv_chart_set_value_by_id(chart, series, i, point_value);
    }

    return stats;
}

UiController::GraphAxisRange UiController::compute_standard_graph_axis(float scale_min,
                                                                       float scale_max,
                                                                       float latest_value,
                                                                       const GraphAxisConfig &config) const {
    GraphAxisRange range{};
    const float min_span = (std::isfinite(config.min_span) && config.min_span > 0.0f) ? config.min_span : 1.0f;
    const float fallback_half_span = (std::isfinite(config.fallback_half_span) && config.fallback_half_span > 0.0f)
        ? config.fallback_half_span
        : min_span;

    float scale_span = scale_max - scale_min;
    if (!std::isfinite(scale_span) || scale_span < min_span) {
        scale_span = min_span;
    }

    float step = graph_nice_step(scale_span / 4.0f);
    if (!std::isfinite(step) || step <= 0.0f) {
        step = config.fallback_step;
    }
    if (!std::isfinite(step) || step <= 0.0f) {
        step = config.last_resort_step;
    }
    if (!std::isfinite(step) || step <= 0.0f) {
        step = 1.0f;
    }

    float y_min_f = floorf((scale_min - (step * 0.9f)) / step) * step;
    float y_max_f = ceilf((scale_max + (step * 0.9f)) / step) * step;
    if ((y_max_f - y_min_f) < (step * 2.0f)) {
        y_min_f -= step;
        y_max_f += step;
    }
    if (!std::isfinite(y_min_f) || !std::isfinite(y_max_f) || y_max_f <= y_min_f) {
        const float center = std::isfinite(latest_value) ? latest_value : config.fallback_center;
        y_min_f = center - fallback_half_span;
        y_max_f = center + fallback_half_span;
    }
    if (config.clamp_min_zero && y_min_f < 0.0f) {
        y_min_f = 0.0f;
    }
    if (config.ensure_display_span_after_clamp && y_max_f <= y_min_f) {
        y_max_f = y_min_f + step;
    }

    const float coord_scale = (std::isfinite(config.point_scale) && config.point_scale > 0.0f) ? config.point_scale : 1.0f;
    lv_coord_t y_min = static_cast<lv_coord_t>(floorf(y_min_f * coord_scale));
    lv_coord_t y_max = static_cast<lv_coord_t>(ceilf(y_max_f * coord_scale));
    if (y_max <= y_min) {
        const lv_coord_t span = (config.min_coord_span > 0) ? config.min_coord_span : 1;
        y_max = static_cast<lv_coord_t>(y_min + span);
    }

    int32_t horizontal_divisions = static_cast<int32_t>(lroundf((y_max_f - y_min_f) / step));
    if (horizontal_divisions < 3) {
        horizontal_divisions = 3;
    }
    if (horizontal_divisions > 12) {
        horizontal_divisions = 12;
    }

    range.y_min_display = y_min_f;
    range.y_max_display = y_max_f;
    range.step = step;
    range.y_min = y_min;
    range.y_max = y_max;
    range.horizontal_divisions = static_cast<uint8_t>(horizontal_divisions);
    return range;
}

UiController::SensorGraphProfile UiController::build_temperature_graph_profile() const {
    SensorGraphProfile profile{};
    profile.min_span = temp_units_c ? 2.0f : 3.5f;
    profile.fallback_value = temp_units_c ? 22.0f : 71.6f;
    switch (temp_graph_range_) {
        case TEMP_GRAPH_RANGE_1H:
            // 0..60 min with 5 min step => 13 vertical marks
            profile.vertical_divisions = 13;
            break;
        case TEMP_GRAPH_RANGE_24H:
            // 0..24 h with 1 h step => 25 vertical marks
            profile.vertical_divisions = 25;
            break;
        case TEMP_GRAPH_RANGE_3H:
        default:
            // 0..180 min with 15 min step => 13 vertical marks
            profile.vertical_divisions = 13;
            break;
    }
    profile.horizontal_divisions_min = 3;
    profile.horizontal_divisions_max = 12;
    profile.unit = temp_units_c ? UiText::UnitC() : UiText::UnitF();
    profile.label_min = "MIN";
    profile.label_now = "NOW";
    profile.label_max = "MAX";
    profile.zone_count = 7;

    const DisplayThresholds::Range temp = displayThresholds.snapshot().temp;
    const float bounds_c[kMaxGraphZoneBounds] = {
        -1000.0f,
        temp.orange_min,
        temp.yellow_min,
        temp.good_min,
        temp.good_max,
        temp.yellow_max,
        temp.orange_max,
        1000.0f};
    for (uint8_t i = 0; i < kMaxGraphZoneBounds; ++i) {
        const bool edge = (i == 0) || (i == (kMaxGraphZoneBounds - 1));
        profile.zone_bounds[i] = edge ? bounds_c[i] : temperature_to_display(bounds_c[i], temp_units_c);
    }

    profile.zone_tones[0] = GRAPH_ZONE_RED;
    profile.zone_tones[1] = GRAPH_ZONE_ORANGE;
    profile.zone_tones[2] = GRAPH_ZONE_YELLOW;
    profile.zone_tones[3] = GRAPH_ZONE_GREEN;
    profile.zone_tones[4] = GRAPH_ZONE_YELLOW;
    profile.zone_tones[5] = GRAPH_ZONE_ORANGE;
    profile.zone_tones[6] = GRAPH_ZONE_RED;

    return profile;
}

lv_color_t UiController::resolve_graph_zone_color(GraphZoneTone tone, lv_color_t chart_bg) {
    switch (tone) {
        case GRAPH_ZONE_RED:
            return lv_color_mix(color_red(), chart_bg, LV_OPA_40);
        case GRAPH_ZONE_ORANGE:
            return lv_color_mix(color_orange(), chart_bg, LV_OPA_40);
        case GRAPH_ZONE_YELLOW:
            return lv_color_mix(color_yellow(), chart_bg, LV_OPA_40);
        case GRAPH_ZONE_GREEN:
            return lv_color_mix(color_green(), chart_bg, LV_OPA_40);
        case GRAPH_ZONE_BLUE:
            return lv_color_mix(color_blue(), chart_bg, LV_OPA_40);
        case GRAPH_ZONE_NONE:
        default:
            return lv_color_mix(color_card_border(), chart_bg, LV_OPA_40);
    }
}

void UiController::ensure_graph_time_labels(lv_obj_t *graph_container,
                                            lv_obj_t *chart,
                                            lv_obj_t **labels,
                                            uint8_t label_count) {
    if (!graph_container || !chart || !labels || label_count == 0) {
        return;
    }

    auto ensure_label = [graph_container](lv_obj_t *&label) {
        if (!label || !lv_obj_is_valid(label) || lv_obj_get_parent(label) != graph_container) {
            label = lv_label_create(graph_container);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_font(label, &ui_font_jet_reg_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    };

    for (uint8_t i = 0; i < label_count; ++i) {
        ensure_label(labels[i]);
    }

    const lv_color_t border = lv_obj_get_style_border_color(chart, LV_PART_MAIN);
    const lv_color_t text = lv_color_mix(active_text_color(), border, LV_OPA_30);
    for (uint8_t i = 0; i < label_count; ++i) {
        lv_obj_t *label = labels[i];
        if (!label) {
            continue;
        }
        lv_obj_set_style_text_color(label, text, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(label);
    }
}

void UiController::update_graph_time_labels(lv_obj_t *graph_container,
                                            lv_obj_t *chart,
                                            lv_obj_t **labels,
                                            uint8_t label_count,
                                            uint16_t points,
                                            bool clear_when_points_lt_two,
                                            bool chart_layout_before_position,
                                            bool move_foreground_after_position) {
    if (!graph_container || !chart || !labels || label_count == 0) {
        return;
    }

    ensure_graph_time_labels(graph_container, chart, labels, label_count);
    if (!labels[0]) {
        return;
    }

    if (clear_when_points_lt_two && points < 2U) {
        for (uint8_t i = 0; i < label_count; ++i) {
            if (labels[i]) {
                safe_label_set_text(labels[i], "");
            }
        }
        return;
    }

    const uint32_t step_s = Config::CHART_HISTORY_STEP_MS / 1000UL;
    const uint32_t span_points = (points > 1U) ? static_cast<uint32_t>(points - 1U) : 1U;
    uint32_t duration_s = step_s * span_points;
    if (duration_s == 0U) {
        duration_s = 3600U;
    }

    bool absolute_time = timeManager.isSystemTimeValid();
    time_t end_epoch = static_cast<time_t>(chartsHistory.latestEpoch());
    if (!absolute_time || end_epoch <= Config::TIME_VALID_EPOCH) {
        end_epoch = time(nullptr);
        if (end_epoch <= Config::TIME_VALID_EPOCH) {
            absolute_time = false;
        }
    }

    const uint8_t last_tick = (label_count > 0) ? static_cast<uint8_t>(label_count - 1U) : 0U;
    for (uint8_t i = 0; i < label_count; ++i) {
        lv_obj_t *label = labels[i];
        if (!label) {
            continue;
        }

        const uint32_t ratio_num = static_cast<uint32_t>(last_tick - i);
        const uint32_t offset_s = (last_tick > 0)
            ? static_cast<uint32_t>((static_cast<uint64_t>(duration_s) * static_cast<uint64_t>(ratio_num)) /
                                    static_cast<uint64_t>(last_tick))
            : 0U;

        char buf[24];
        bool formatted = false;
        if (absolute_time) {
            const time_t tick_epoch = end_epoch - static_cast<time_t>(offset_s);
            formatted = format_epoch_hhmm(tick_epoch, buf, sizeof(buf));
        }
        if (!formatted) {
            format_relative_time_label(offset_s, buf, sizeof(buf));
        }
        safe_label_set_text(label, buf);
    }

    if (chart_layout_before_position) {
        lv_obj_update_layout(chart);
    }

    const lv_coord_t chart_x = lv_obj_get_x(chart);
    const lv_coord_t chart_y = lv_obj_get_y(chart);
    const lv_coord_t chart_w = lv_obj_get_width(chart);
    const lv_coord_t chart_h = lv_obj_get_height(chart);
    const lv_coord_t label_y = chart_y + chart_h + 4;

    for (uint8_t i = 0; i < label_count; ++i) {
        lv_obj_t *label = labels[i];
        if (!label) {
            continue;
        }

        lv_obj_update_layout(label);
        const lv_coord_t label_w = lv_obj_get_width(label);
        lv_coord_t tick_x = chart_x;
        if (chart_w > 1 && last_tick > 0) {
            tick_x = chart_x + static_cast<lv_coord_t>(
                (static_cast<int32_t>(chart_w - 1) * static_cast<int32_t>(i)) / static_cast<int32_t>(last_tick));
        }

        lv_coord_t label_x = tick_x - (label_w / 2);
        const lv_coord_t min_x = chart_x;
        const lv_coord_t max_x = chart_x + chart_w - label_w;
        if (label_x < min_x) {
            label_x = min_x;
        }
        if (label_x > max_x) {
            label_x = max_x;
        }

        lv_obj_set_pos(label, label_x, label_y);
        if (move_foreground_after_position) {
            lv_obj_move_foreground(label);
        }
    }
}

void UiController::ensure_graph_stat_overlays(lv_obj_t *chart,
                                              lv_obj_t *&label_min,
                                              lv_obj_t *&label_now,
                                              lv_obj_t *&label_max) {
    if (!chart) {
        return;
    }

    auto ensure_label = [chart](lv_obj_t *&label, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
        if (!label || !lv_obj_is_valid(label) || lv_obj_get_parent(label) != chart) {
            label = lv_label_create(chart);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_font(label, &ui_font_jet_reg_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(label, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(label, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(label, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(label, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(label, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(label, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(label, LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_align(label, align, x_ofs, y_ofs);
        lv_obj_move_foreground(label);
    };

    ensure_label(label_min, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    ensure_label(label_now, LV_ALIGN_TOP_LEFT, 8, 6);
    ensure_label(label_max, LV_ALIGN_TOP_RIGHT, -8, 6);
}

void UiController::style_graph_stat_overlays(lv_obj_t *chart,
                                             lv_obj_t *label_min,
                                             lv_obj_t *label_now,
                                             lv_obj_t *label_max) {
    if (!chart) {
        return;
    }

    const lv_color_t chart_bg = lv_obj_get_style_bg_color(chart, LV_PART_MAIN);
    const lv_color_t border = lv_obj_get_style_border_color(chart, LV_PART_MAIN);
    const lv_color_t text = active_text_color();
    const lv_color_t badge_bg = lv_color_mix(border, chart_bg, LV_OPA_60);

    lv_obj_t *labels[] = {label_min, label_now, label_max};
    for (lv_obj_t *label : labels) {
        if (!label) {
            continue;
        }
        lv_obj_set_style_text_color(label, text, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(label, badge_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(label, border, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::update_graph_stat_overlay_labels(lv_obj_t *chart,
                                                    lv_obj_t *&label_min,
                                                    lv_obj_t *&label_now,
                                                    lv_obj_t *&label_max,
                                                    bool has_values,
                                                    float min_value,
                                                    float max_value,
                                                    float latest_value,
                                                    const char *min_format,
                                                    const char *now_format,
                                                    const char *max_format) {
    if (!chart) {
        return;
    }

    ensure_graph_stat_overlays(chart, label_min, label_now, label_max);
    if (!label_min || !label_now || !label_max) {
        return;
    }

    style_graph_stat_overlays(chart, label_min, label_now, label_max);

    if (!has_values) {
        safe_label_set_text(label_min, "MIN --");
        safe_label_set_text(label_now, "NOW --");
        safe_label_set_text(label_max, "MAX --");
        return;
    }

    char min_buf[48];
    char now_buf[48];
    char max_buf[48];
    snprintf(min_buf, sizeof(min_buf), min_format ? min_format : "MIN %.1f", min_value);
    snprintf(now_buf, sizeof(now_buf), now_format ? now_format : "NOW %.1f", latest_value);
    snprintf(max_buf, sizeof(max_buf), max_format ? max_format : "MAX %.1f", max_value);
    safe_label_set_text(label_min, min_buf);
    safe_label_set_text(label_now, now_buf);
    safe_label_set_text(label_max, max_buf);
}

void UiController::ensure_graph_zone_overlay(lv_obj_t *graph_container,
                                             lv_obj_t *chart,
                                             lv_obj_t *&overlay,
                                             lv_obj_t **bands,
                                             uint8_t band_count) {
    if (!graph_container || !chart || !bands || band_count == 0) {
        return;
    }

    lv_obj_update_layout(graph_container);
    lv_obj_update_layout(chart);

    if (!overlay || !lv_obj_is_valid(overlay) || lv_obj_get_parent(overlay) != graph_container) {
        overlay = lv_obj_create(graph_container);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    const lv_coord_t chart_x = lv_obj_get_x(chart);
    const lv_coord_t chart_y = lv_obj_get_y(chart);
    const lv_coord_t chart_w = lv_obj_get_width(chart);
    const lv_coord_t chart_h = lv_obj_get_height(chart);

    lv_obj_set_pos(overlay, chart_x, chart_y);
    lv_obj_set_size(overlay, chart_w, chart_h);
    lv_obj_update_layout(overlay);
    lv_obj_set_style_radius(overlay,
                            lv_obj_get_style_radius(chart, LV_PART_MAIN),
                            LV_PART_MAIN | LV_STATE_DEFAULT);

    for (uint8_t i = 0; i < band_count; ++i) {
        lv_obj_t *&band = bands[i];
        if (!band || !lv_obj_is_valid(band) || lv_obj_get_parent(band) != overlay) {
            band = lv_obj_create(overlay);
            lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_move_background(band);
    }

    lv_obj_move_background(overlay);
    lv_obj_move_foreground(chart);
}

void UiController::update_graph_zone_overlay(lv_obj_t *chart,
                                             lv_obj_t *overlay,
                                             lv_obj_t **bands,
                                             uint8_t band_count,
                                             const float *zone_bounds,
                                             const GraphZoneTone *zone_tones,
                                             uint8_t zone_count,
                                             float y_min_display,
                                             float y_max_display) {
    if (!chart || !overlay || !lv_obj_is_valid(overlay) || !bands || band_count == 0) {
        return;
    }

    const lv_coord_t width = lv_obj_get_width(overlay);
    const lv_coord_t height = lv_obj_get_height(overlay);
    if (width <= 0 || height <= 0 || !std::isfinite(y_min_display) || !std::isfinite(y_max_display) || y_max_display <= y_min_display ||
        !zone_bounds || !zone_tones) {
        hide_graph_zone_bands(bands, band_count);
        return;
    }

    if (zone_count > band_count) {
        zone_count = band_count;
    }
    if (zone_count == 0) {
        hide_graph_zone_bands(bands, band_count);
        return;
    }

    const lv_color_t chart_bg = lv_obj_get_style_bg_color(chart, LV_PART_MAIN);
    const float denom = y_max_display - y_min_display;

    for (uint8_t i = 0; i < band_count; ++i) {
        lv_obj_t *band = bands[i];
        if (!band) {
            continue;
        }
        if (i >= zone_count) {
            lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const float zone_low = zone_bounds[i];
        const float zone_high = zone_bounds[i + 1];
        if (!std::isfinite(zone_low) || !std::isfinite(zone_high) || zone_high <= zone_low) {
            lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const float visible_low = fmaxf(zone_low, y_min_display);
        const float visible_high = fminf(zone_high, y_max_display);
        if (!(visible_high > visible_low)) {
            lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        float top_ratio = (y_max_display - visible_high) / denom;
        float bottom_ratio = (y_max_display - visible_low) / denom;
        if (top_ratio < 0.0f) top_ratio = 0.0f;
        if (top_ratio > 1.0f) top_ratio = 1.0f;
        if (bottom_ratio < 0.0f) bottom_ratio = 0.0f;
        if (bottom_ratio > 1.0f) bottom_ratio = 1.0f;

        lv_coord_t top = static_cast<lv_coord_t>(lroundf(top_ratio * static_cast<float>(height)));
        lv_coord_t bottom = static_cast<lv_coord_t>(lroundf(bottom_ratio * static_cast<float>(height)));
        if (bottom <= top) {
            bottom = static_cast<lv_coord_t>(top + 1);
        }
        if (top < 0) top = 0;
        if (bottom > height) bottom = height;
        if (bottom <= top) {
            lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_set_pos(band, 0, top);
        lv_obj_set_size(band, width, static_cast<lv_coord_t>(bottom - top));
        const lv_color_t zone_color = resolve_graph_zone_color(zone_tones[i], chart_bg);
        lv_obj_set_style_bg_color(band, zone_color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(band, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(band);
    }
}

void UiController::sync_info_graph_button_state() {
    if (!objects.btn_info_graph) {
        return;
    }

    const bool voc_selected = (info_sensor == INFO_VOC);
    const bool nox_selected = (info_sensor == INFO_NOX);
    const bool optional_gas_selected = (info_sensor == INFO_OPTIONAL_GAS);
    const bool hcho_selected = (info_sensor == INFO_HCHO);
    const bool co2_selected = (info_sensor == INFO_CO2);
    const bool co_selected = (info_sensor == INFO_CO);
    const bool pm05_selected = (info_sensor == INFO_PM05);
    const bool pm25_4_selected = (info_sensor == INFO_PM25) || (info_sensor == INFO_PM4);
    const bool pm1_10_selected = (info_sensor == INFO_PM1) || (info_sensor == INFO_PM10);
    const bool pressure_selected = (info_sensor == INFO_PRESSURE_3H) || (info_sensor == INFO_PRESSURE_24H);
    const bool pressure_graph_available = !pressure_selected || pressure_altitude_is_set();
    const bool graph_supported = (info_sensor == INFO_TEMP) || (info_sensor == INFO_RH) ||
                                 voc_selected || nox_selected || hcho_selected || co2_selected ||
                                 optional_gas_selected ||
                                 pm05_selected ||
                                 pm25_4_selected ||
                                 pm1_10_selected ||
                                 co_selected || (pressure_selected && pressure_graph_available);
    const bool graph_checked =
        ((info_sensor == INFO_TEMP) && temp_graph_mode_) ||
        ((info_sensor == INFO_RH) && rh_graph_mode_) ||
        (voc_selected && voc_graph_mode_) ||
        (nox_selected && nox_graph_mode_) ||
        (optional_gas_selected && optional_gas_graph_mode_) ||
        (hcho_selected && hcho_graph_mode_) ||
        (co2_selected && co2_graph_mode_) ||
        (pm05_selected && pm05_graph_mode_) ||
        (pm25_4_selected && pm25_4_graph_mode_) ||
        (pm1_10_selected && pm1_10_graph_mode_) ||
        (co_selected && co_graph_mode_) ||
        (pressure_selected && pressure_graph_mode_);

    if (graph_checked) {
        lv_obj_add_state(objects.btn_info_graph, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_info_graph, LV_STATE_CHECKED);
    }

    if (graph_supported) {
        set_visible(objects.btn_info_graph, true);
        lv_obj_clear_state(objects.btn_info_graph, LV_STATE_DISABLED);
        lv_obj_add_flag(objects.btn_info_graph, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_move_foreground(objects.btn_info_graph);
    } else {
        lv_obj_clear_state(objects.btn_info_graph, LV_STATE_CHECKED);
        lv_obj_add_state(objects.btn_info_graph, LV_STATE_DISABLED);
        set_visible(objects.btn_info_graph, false);
    }

    lv_obj_set_ext_click_area(objects.btn_info_graph, 18);
}

bool UiController::should_show_threshold_dots() const {
    if (info_sensor == INFO_NONE) {
        return false;
    }
    if (info_sensor == INFO_TEMP) {
        return !temp_graph_mode_;
    }
    if (info_sensor == INFO_RH) {
        return !rh_graph_mode_;
    }
    if (info_sensor == INFO_VOC) {
        return !voc_graph_mode_;
    }
    if (info_sensor == INFO_NOX) {
        return !nox_graph_mode_;
    }
    if (info_sensor == INFO_OPTIONAL_GAS) {
        return !optional_gas_graph_mode_;
    }
    if (info_sensor == INFO_HCHO) {
        return !hcho_graph_mode_;
    }
    if (info_sensor == INFO_CO2) {
        return !co2_graph_mode_;
    }
    if (info_sensor == INFO_PM05) {
        return !pm05_graph_mode_;
    }
    if (info_sensor == INFO_PM25 || info_sensor == INFO_PM4) {
        return !pm25_4_graph_mode_;
    }
    if (info_sensor == INFO_PM1 || info_sensor == INFO_PM10) {
        return !pm1_10_graph_mode_;
    }
    if (info_sensor == INFO_CO) {
        return !co_graph_mode_;
    }
    if (info_sensor == INFO_PRESSURE_3H || info_sensor == INFO_PRESSURE_24H) {
        return !pressure_graph_mode_;
    }
    return true;
}

void UiController::sync_threshold_dots_visibility() {
    set_visible(objects.container_thresholds_dots, should_show_threshold_dots());
}

void UiController::set_temperature_info_mode(bool graph_mode) {
    const bool mode_changed = (temp_graph_mode_ != graph_mode);
    temp_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_info_graph) {
        lv_obj_add_flag(objects.btn_info_graph, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_info_graph, 18);
    }
    if (objects.btn_temp_range_1h) {
        lv_obj_add_flag(objects.btn_temp_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_1h, 12);
    }
    if (objects.btn_temp_range_3h) {
        lv_obj_add_flag(objects.btn_temp_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_3h, 12);
    }
    if (objects.btn_temp_range_24h) {
        lv_obj_add_flag(objects.btn_temp_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.temperature_info_thresholds, !graph_mode);
    set_visible(objects.temperature_info_graph, graph_mode);
    sync_threshold_dots_visibility();

    sync_info_graph_button_state();
    set_checked(objects.btn_temp_range_1h, temp_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_temp_range_3h, temp_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_temp_range_24h, temp_graph_range_ == TEMP_GRAPH_RANGE_24H);
}

void UiController::set_rh_info_mode(bool graph_mode) {
    const bool mode_changed = (rh_graph_mode_ != graph_mode);
    rh_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_rh_range_1h) {
        lv_obj_add_flag(objects.btn_rh_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_rh_range_1h, 12);
    }
    if (objects.btn_rh_range_3h) {
        lv_obj_add_flag(objects.btn_rh_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_rh_range_3h, 12);
    }
    if (objects.btn_rh_range_24h) {
        lv_obj_add_flag(objects.btn_rh_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_rh_range_24h, 12);
    }
    set_visible(objects.rh_info_thresholds, !graph_mode);
    set_visible(objects.rh_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_rh_range_1h, rh_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_rh_range_3h, rh_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_rh_range_24h, rh_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_voc_info_mode(bool graph_mode) {
    const bool mode_changed = (voc_graph_mode_ != graph_mode);
    voc_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_voc_range_1h) {
        lv_obj_add_flag(objects.btn_voc_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_1h, 12);
    }
    if (objects.btn_voc_range_3h) {
        lv_obj_add_flag(objects.btn_voc_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_3h, 12);
    }
    if (objects.btn_voc_range_24h) {
        lv_obj_add_flag(objects.btn_voc_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.voc_info_thresholds, !graph_mode);
    set_visible(objects.voc_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_voc_range_1h, voc_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_voc_range_3h, voc_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_voc_range_24h, voc_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_nox_info_mode(bool graph_mode) {
    const bool mode_changed = (nox_graph_mode_ != graph_mode);
    nox_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_nox_range_1h) {
        lv_obj_add_flag(objects.btn_nox_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_1h, 12);
    }
    if (objects.btn_nox_range_3h) {
        lv_obj_add_flag(objects.btn_nox_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_3h, 12);
    }
    if (objects.btn_nox_range_24h) {
        lv_obj_add_flag(objects.btn_nox_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.nox_info_thresholds, !graph_mode);
    set_visible(objects.nox_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_nox_range_1h, nox_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_nox_range_3h, nox_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_nox_range_24h, nox_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_optional_gas_info_mode(bool graph_mode) {
    const bool mode_changed = (optional_gas_graph_mode_ != graph_mode);
    optional_gas_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_optional_gas_range_1h) {
        lv_obj_add_flag(objects.btn_optional_gas_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_optional_gas_range_1h, 12);
    }
    if (objects.btn_optional_gas_range_3h) {
        lv_obj_add_flag(objects.btn_optional_gas_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_optional_gas_range_3h, 12);
    }
    if (objects.btn_optional_gas_range_24h) {
        lv_obj_add_flag(objects.btn_optional_gas_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_optional_gas_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.optional_gas_info_thresholds, !graph_mode);
    set_visible(objects.optional_gas_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_optional_gas_range_1h, optional_gas_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_optional_gas_range_3h, optional_gas_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_optional_gas_range_24h, optional_gas_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_hcho_info_mode(bool graph_mode) {
    const bool mode_changed = (hcho_graph_mode_ != graph_mode);
    hcho_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_hcho_range_1h) {
        lv_obj_add_flag(objects.btn_hcho_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_1h, 12);
    }
    if (objects.btn_hcho_range_3h) {
        lv_obj_add_flag(objects.btn_hcho_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_3h, 12);
    }
    if (objects.btn_hcho_range_24h) {
        lv_obj_add_flag(objects.btn_hcho_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.hcho_info_thresholds, !graph_mode);
    set_visible(objects.hcho_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_hcho_range_1h, hcho_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_hcho_range_3h, hcho_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_hcho_range_24h, hcho_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_co2_info_mode(bool graph_mode) {
    const bool mode_changed = (co2_graph_mode_ != graph_mode);
    co2_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_co2_range_1h) {
        lv_obj_add_flag(objects.btn_co2_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co2_range_1h, 12);
    }
    if (objects.btn_co2_range_3h) {
        lv_obj_add_flag(objects.btn_co2_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co2_range_3h, 12);
    }
    if (objects.btn_co2_range_24h) {
        lv_obj_add_flag(objects.btn_co2_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co2_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.co2_info_thresholds, !graph_mode);
    set_visible(objects.co2_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_co2_range_1h, co2_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_co2_range_3h, co2_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_co2_range_24h, co2_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_pm05_info_mode(bool graph_mode) {
    const bool mode_changed = (pm05_graph_mode_ != graph_mode);
    pm05_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_pm05_range_1h) {
        lv_obj_add_flag(objects.btn_pm05_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_1h, 12);
    }
    if (objects.btn_pm05_range_3h) {
        lv_obj_add_flag(objects.btn_pm05_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_3h, 12);
    }
    if (objects.btn_pm05_range_24h) {
        lv_obj_add_flag(objects.btn_pm05_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }

    set_visible(objects.pm05_info_thresholds, !graph_mode);
    set_visible(objects.pm05_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_pm05_range_1h, pm05_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm05_range_3h, pm05_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm05_range_24h, pm05_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_pm25_4_info_mode(bool graph_mode) {
    const bool mode_changed = (pm25_4_graph_mode_ != graph_mode);
    pm25_4_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_pm25_4_range_1h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_1h, 12);
    }
    if (objects.btn_pm25_4_range_3h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_3h, 12);
    }
    if (objects.btn_pm25_4_range_24h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    if (objects.btn_pm25_info) {
        lv_obj_move_foreground(objects.btn_pm25_info);
    }
    if (objects.btn_pm4_info) {
        lv_obj_move_foreground(objects.btn_pm4_info);
    }
    if (objects.pm25_info) {
        lv_obj_clear_flag(objects.pm25_info, LV_OBJ_FLAG_CLICKABLE);
    }

    const bool pm25_selected = info_sensor == INFO_PM25;
    const bool pm4_selected = info_sensor == INFO_PM4;
    const bool pm25_4_selected = pm25_selected || pm4_selected;
    set_visible(objects.pm25_4_graph, graph_mode && pm25_4_selected);
    set_visible(objects.label_pm25_text, pm25_selected);
    set_visible(objects.label_pm4_text, pm4_selected);
    set_visible(objects.pm25_info_thresholds, !graph_mode && pm25_selected);
    set_visible(objects.pm4_info_thresholds, !graph_mode && pm4_selected);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_pm25_4_range_1h, pm25_4_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm25_4_range_3h, pm25_4_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm25_4_range_24h, pm25_4_graph_range_ == TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pm25_info, pm25_selected);
    set_checked(objects.btn_pm4_info, pm4_selected);

    sync_info_graph_button_state();
}

void UiController::set_pm1_10_info_mode(bool graph_mode) {
    const bool mode_changed = (pm1_10_graph_mode_ != graph_mode);
    pm1_10_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_pm1_10_range_1h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_1h, 12);
    }
    if (objects.btn_pm1_10_range_3h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_3h, 12);
    }
    if (objects.btn_pm1_10_range_24h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    if (objects.btn_pm1_info) {
        lv_obj_move_foreground(objects.btn_pm1_info);
    }
    if (objects.btn_pm10_info) {
        lv_obj_move_foreground(objects.btn_pm10_info);
    }
    // PM1/PM10 info containers are decorative only; keep them non-clickable so range buttons remain tappable.
    if (objects.pm1_info) {
        lv_obj_clear_flag(objects.pm1_info, LV_OBJ_FLAG_CLICKABLE);
    }
    if (objects.pm10_info) {
        lv_obj_clear_flag(objects.pm10_info, LV_OBJ_FLAG_CLICKABLE);
    }

    const bool pm1_selected = info_sensor == INFO_PM1;
    const bool pm10_selected = info_sensor == INFO_PM10;
    const bool pm1_10_selected = pm1_selected || pm10_selected;
    set_visible(objects.pm1_10_info_graph, graph_mode && pm1_10_selected);
    set_visible(objects.pm1_info, pm1_selected);
    set_visible(objects.pm10_info, pm10_selected);
    set_visible(objects.pm1_info_thresholds, !graph_mode && pm1_selected);
    set_visible(objects.pm10_info_thresholds, !graph_mode && pm10_selected);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_pm1_10_range_1h, pm1_10_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm1_10_range_3h, pm1_10_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm1_10_range_24h, pm1_10_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_co_info_mode(bool graph_mode) {
    const bool mode_changed = (co_graph_mode_ != graph_mode);
    co_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }
    if (objects.btn_co_range_1h) {
        lv_obj_add_flag(objects.btn_co_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_1h, 12);
    }
    if (objects.btn_co_range_3h) {
        lv_obj_add_flag(objects.btn_co_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_3h, 12);
    }
    if (objects.btn_co_range_24h) {
        lv_obj_add_flag(objects.btn_co_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_24h, 12);
    }
    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }
    set_visible(objects.co_info_thresholds, !graph_mode);
    set_visible(objects.co_info_graph, graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_co_range_1h, co_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_co_range_3h, co_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_co_range_24h, co_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

void UiController::set_pressure_info_mode(bool graph_mode) {
    const bool mode_changed = (pressure_graph_mode_ != graph_mode);
    pressure_graph_mode_ = graph_mode;
    if (mode_changed) {
        invalidate_active_graph_refresh_cache();
    }

    if (objects.btn_pressure_range_1h) {
        lv_obj_add_flag(objects.btn_pressure_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pressure_range_1h, 12);
    }
    if (objects.btn_pressure_range_3h) {
        lv_obj_add_flag(objects.btn_pressure_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pressure_range_3h, 12);
    }
    if (objects.btn_pressure_range_24h) {
        lv_obj_add_flag(objects.btn_pressure_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pressure_range_24h, 12);
    }

    if (objects.btn_info_graph) {
        lv_obj_move_foreground(objects.btn_info_graph);
    }
    if (objects.btn_back_1) {
        lv_obj_move_foreground(objects.btn_back_1);
    }

    const bool show_3h = info_sensor == INFO_PRESSURE_3H;
    const bool show_24h = info_sensor == INFO_PRESSURE_24H;
    set_visible(objects.pressure_3h_pressure_thresholds, !graph_mode && show_3h);
    set_visible(objects.pressure_24h_pressure_thresholds, !graph_mode && show_24h);
    set_visible(objects.pressure_info_graph, graph_mode && (show_3h || show_24h));
    set_visible(objects.btn_3h_pressure_info, !graph_mode);
    set_visible(objects.btn_24h_pressure_info, !graph_mode);
    sync_threshold_dots_visibility();
    set_checked(objects.btn_pressure_range_1h, pressure_graph_range_ == TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pressure_range_3h, pressure_graph_range_ == TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pressure_range_24h, pressure_graph_range_ == TEMP_GRAPH_RANGE_24H);

    sync_info_graph_button_state();
}

