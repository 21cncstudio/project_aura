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
#include "core/MathUtils.h"
#include "modules/ChartsHistory.h"
#include "ui/UiOptionalGasProfile.h"
#include "ui/UiText.h"
#include "ui/ui.h"

namespace {
bool format_pm05_count(float value, char *buf, size_t buf_size) {
    if (!isfinite(value) || value < 0.0f || !buf || buf_size == 0) {
        return false;
    }
    if (value >= 1000.0f) {
        snprintf(buf, buf_size, "%.1fk", value / 1000.0f);
    } else {
        snprintf(buf, buf_size, "%.0f", value);
    }
    return true;
}

void set_checked_state(lv_obj_t *btn, bool checked) {
    if (!btn) {
        return;
    }
    if (checked) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
}
} // namespace

void UiController::update_sensor_info_ui() {
    if (info_sensor == INFO_NONE) {
        return;
    }
    switch (info_sensor) {
        case INFO_TEMP: {
            char buf[16];
            if (currentData.temp_valid) {
                float temp_display = currentData.temperature;
                if (!temp_units_c) {
                    temp_display = (temp_display * 9.0f / 5.0f) + 32.0f;
                }
                snprintf(buf, sizeof(buf), "%.1f", temp_display);
            } else {
                strcpy(buf, UiText::ValueMissing());
            }
            safe_label_set_text(objects.label_sensor_value, buf);
            lv_color_t temp_col = currentData.temp_valid ? getTempColor(currentData.temperature) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(temp_col));
            set_temperature_info_mode(temp_graph_mode_);
            if (temp_graph_mode_ &&
                should_refresh_active_graph(INFO_TEMP, temp_graph_range_, temperature_graph_points())) {
                update_temperature_info_graph();
            }
            break;
        }
        case INFO_VOC: {
            const bool gas_warmup = sensorManager.isWarmupActive();
            if (gas_warmup) {
                safe_label_set_text(objects.label_sensor_value, "---");
            } else if (currentData.voc_valid) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", currentData.voc_index);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            lv_color_t voc_col = gas_warmup ? color_blue()
                                            : (currentData.voc_valid ? getVOCColor(currentData.voc_index) : color_inactive());
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(voc_col));
            set_voc_info_mode(voc_graph_mode_);
            if (voc_graph_mode_ &&
                should_refresh_active_graph(INFO_VOC, voc_graph_range_, voc_graph_points())) {
                update_voc_info_graph();
            }
            break;
        }
        case INFO_NOX: {
            const bool gas_warmup = sensorManager.isWarmupActive();
            if (gas_warmup) {
                safe_label_set_text(objects.label_sensor_value, "---");
            } else if (currentData.nox_valid) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", currentData.nox_index);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            lv_color_t nox_col = gas_warmup ? color_blue()
                                            : (currentData.nox_valid ? getNOxColor(currentData.nox_index) : color_inactive());
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(nox_col));
            set_nox_info_mode(nox_graph_mode_);
            if (nox_graph_mode_ &&
                should_refresh_active_graph(INFO_NOX, nox_graph_range_, nox_graph_points())) {
                update_nox_info_graph();
            }
            break;
        }
        case INFO_OPTIONAL_GAS: {
            using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;
            const OptionalGasType type = static_cast<OptionalGasType>(currentData.optional_gas_type);
            const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
            const bool present = currentData.optional_gas_sensor_present &&
                                 UiOptionalGasProfile::isKnown(type);
            const bool warmup = present && currentData.optional_gas_warmup;
            const bool valid = present &&
                               currentData.optional_gas_valid &&
                               isfinite(currentData.optional_gas_ppm) &&
                               currentData.optional_gas_ppm >= 0.0f;

            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, profile.label);
            }
            safe_label_set_text(objects.label_sensor_info_unit, present ? profile.unit : "");
            if (objects.label_optional_gas_text) {
                safe_label_set_text(objects.label_optional_gas_text, profile.description);
            }

            if (valid) {
                char buf[16];
                UiOptionalGasProfile::formatValue(profile,
                                                  currentData.optional_gas_ppm,
                                                  currentData.optional_gas_ppm_decimals,
                                                  buf,
                                                  sizeof(buf));
                safe_label_set_text(objects.label_sensor_value, buf);
            } else if (warmup) {
                safe_label_set_text(objects.label_sensor_value, "---");
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }

            char band[160];
            UiOptionalGasProfile::formatBandLabel(profile, 0, band, sizeof(band));
            safe_label_set_text(objects.label_optional_gas_excellent, band);
            UiOptionalGasProfile::formatBandLabel(profile, 1, band, sizeof(band));
            safe_label_set_text(objects.label_optional_gas_acceptable, band);
            UiOptionalGasProfile::formatBandLabel(profile, 2, band, sizeof(band));
            safe_label_set_text(objects.label_optional_gas_uncomfortable, band);
            UiOptionalGasProfile::formatBandLabel(profile, 3, band, sizeof(band));
            safe_label_set_text(objects.label_optional_gas_poor, band);

            lv_color_t gas_col = warmup ? color_blue() : getOptionalGasColor(type, currentData.optional_gas_ppm, valid);
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(gas_col));
            set_optional_gas_info_mode(optional_gas_graph_mode_);
            if (optional_gas_graph_mode_ &&
                should_refresh_active_graph(INFO_OPTIONAL_GAS,
                                            optional_gas_graph_range_,
                                            optional_gas_graph_points())) {
                update_optional_gas_info_graph();
            }
            break;
        }
        case INFO_HCHO: {
            set_visible(objects.label_hcho_text, true);
            set_visible(objects.label_aqi_text, false);
            set_visible(objects.aqi_info_thresholds, false);
            const bool hcho_warmup = sensorManager.isSfaWarmupActive();
            if (currentData.hcho_valid) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", static_cast<int>(lroundf(currentData.hcho)));
                safe_label_set_text(objects.label_sensor_value, buf);
            } else if (hcho_warmup) {
                safe_label_set_text(objects.label_sensor_value, "---");
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = nullptr;
            if (objects.label_hcho_unit_1) {
                unit = lv_label_get_text(objects.label_hcho_unit_1);
            } else {
                unit = UiText::UnitPpb();
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t hcho_col = hcho_warmup ? color_blue()
                                              : getHCHOColor(currentData.hcho, currentData.hcho_valid);
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(hcho_col));
            set_hcho_info_mode(hcho_graph_mode_);
            if (hcho_graph_mode_ &&
                should_refresh_active_graph(INFO_HCHO, hcho_graph_range_, hcho_graph_points())) {
                update_hcho_info_graph();
            }
            break;
        }
        case INFO_AQI: {
            const AirQuality aq = getAirQuality(currentData);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", aq.score);
            safe_label_set_text(objects.label_sensor_value, buf);
            safe_label_set_text(objects.label_sensor_info_unit, UiText::UnitIndex());
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(aq.color));
            set_visible(objects.label_hcho_text, false);
            set_visible(objects.hcho_info_thresholds, false);
            set_visible(objects.hcho_info_graph, false);
            set_visible(objects.label_aqi_text, true);
            set_visible(objects.aqi_info_thresholds, true);
            break;
        }
        case INFO_CO2: {
            if (currentData.co2_valid) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", currentData.co2);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = nullptr;
            if (objects.label_co2_unit_1) {
                unit = lv_label_get_text(objects.label_co2_unit_1);
            } else {
                unit = "ppm";
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t co2_col = currentData.co2_valid ? getCO2Color(currentData.co2) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(co2_col));
            set_co2_info_mode(co2_graph_mode_);
            if (co2_graph_mode_ &&
                should_refresh_active_graph(INFO_CO2, co2_graph_range_, co2_graph_points())) {
                update_co2_info_graph();
            }
            break;
        }
        case INFO_RH: {
            if (currentData.hum_valid) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.0f", currentData.humidity);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissingShort());
            }
            const char *unit = objects.label_hum_unit_1
                ? lv_label_get_text(objects.label_hum_unit_1)
                : "%";
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t hum_col = currentData.hum_valid ? getHumidityColor(currentData.humidity) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(hum_col));
            set_rh_info_mode(rh_graph_mode_);
            if (rh_graph_mode_ &&
                should_refresh_active_graph(INFO_RH, rh_graph_range_, humidity_graph_points())) {
                update_humidity_info_graph();
            }
            break;
        }
        case INFO_AH: {
            float ah_gm3 = NAN;
            if (currentData.temp_valid && currentData.hum_valid) {
                ah_gm3 = MathUtils::compute_absolute_humidity_gm3(currentData.temperature, currentData.humidity);
            }
            if (isfinite(ah_gm3)) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.0f", ah_gm3);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissingShort());
            }
            const char *unit = objects.label_ah_unit_1
                ? lv_label_get_text(objects.label_ah_unit_1)
                : "g/m3";
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t ah_col = getAbsoluteHumidityColor(ah_gm3);
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(ah_col));
            set_rh_info_mode(false);
            break;
        }
        case INFO_MR: {
            const int mold_risk =
                (currentData.temp_valid && currentData.hum_valid)
                    ? MathUtils::compute_mold_risk_index(currentData.temperature, currentData.humidity)
                    : -1;
            if (mold_risk >= 0) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", mold_risk);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text_static(objects.label_sensor_value, UiText::ValueMissingShort());
            }

            const char *unit = UiText::UnitIndex();
            if (objects.label_mr_unit) {
                unit = lv_label_get_text(objects.label_mr_unit);
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);

            lv_color_t mr_col = color_inactive();
            if (mold_risk >= 0) {
                if (mold_risk <= 2) {
                    mr_col = color_green();
                } else if (mold_risk <= 4) {
                    mr_col = color_yellow();
                } else if (mold_risk <= 7) {
                    mr_col = color_orange();
                } else {
                    mr_col = color_red();
                }
            }
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(mr_col));
            break;
        }
        case INFO_DP: {
            float dew_c = NAN;
            if (currentData.temp_valid && currentData.hum_valid) {
                dew_c = MathUtils::compute_dew_point_c(currentData.temperature, currentData.humidity);
            }
            if (isfinite(dew_c)) {
                float dew_display = dew_c;
                if (!temp_units_c) {
                    dew_display = (dew_display * 9.0f / 5.0f) + 32.0f;
                }
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", dew_display);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissingShort());
            }
            safe_label_set_text(objects.label_sensor_info_unit, temp_units_c ? UiText::UnitC() : UiText::UnitF());
            lv_color_t dp_col = getDewPointColor(dew_c);
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(dp_col));
            break;
        }
        case INFO_PM05: {
            if (currentData.pm05_valid) {
                char buf[16];
                if (format_pm05_count(currentData.pm05, buf, sizeof(buf))) {
                    safe_label_set_text(objects.label_sensor_value, buf);
                } else {
                    safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
                }
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = objects.label_pm05_unit
                ? lv_label_get_text(objects.label_pm05_unit)
                : "#/cm3";
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t pm05_col = currentData.pm05_valid ? getPM05Color(currentData.pm05) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(pm05_col));
            set_pm05_info_mode(pm05_graph_mode_);
            if (pm05_graph_mode_ &&
                should_refresh_active_graph(INFO_PM05, pm05_graph_range_, pm05_graph_points())) {
                update_pm05_info_graph();
            }
            break;
        }
        case INFO_PM25:
        case INFO_PM4: {
            const bool pm4_selected = info_sensor == INFO_PM4;
            const float value = pm4_selected ? currentData.pm4 : currentData.pm25;
            const bool value_valid = pm4_selected
                ? (currentData.pm4_valid && isfinite(currentData.pm4) && currentData.pm4 >= 0.0f)
                : (currentData.pm25_valid && isfinite(currentData.pm25) && currentData.pm25 >= 0.0f);
            if (value_valid) {
                char buf[16];
                if (value < 10.0f) snprintf(buf, sizeof(buf), "%.1f", value);
                else snprintf(buf, sizeof(buf), "%.0f", value);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = nullptr;
            if (objects.label_pm25_unit_1) {
                unit = lv_label_get_text(objects.label_pm25_unit_1);
            } else {
                unit = "ug/m3";
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t pm_col = value_valid
                ? (pm4_selected ? getPM4Color(value) : getPM25Color(value))
                : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(pm_col));
            set_pm25_4_info_mode(pm25_4_graph_mode_);
            if (pm25_4_graph_mode_ &&
                should_refresh_active_graph(info_sensor, pm25_4_graph_range_, pm25_4_graph_points())) {
                update_pm25_4_info_graph();
            }
            break;
        }
        case INFO_PM10: {
            if (currentData.pm10_valid) {
                char buf[16];
                if (currentData.pm10 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm10);
                else snprintf(buf, sizeof(buf), "%.0f", currentData.pm10);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = nullptr;
            if (objects.label_pm10_unit) {
                unit = lv_label_get_text(objects.label_pm10_unit);
            } else {
                unit = "ug/m3";
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t pm10_col = currentData.pm10_valid ? getPM10Color(currentData.pm10) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(pm10_col));
            set_pm1_10_info_mode(pm1_10_graph_mode_);
            if (pm1_10_graph_mode_ &&
                should_refresh_active_graph(INFO_PM10, pm1_10_graph_range_, pm1_10_graph_points())) {
                update_pm1_10_info_graph();
            }
            break;
        }
        case INFO_PM1: {
            const bool pm1_available = currentData.pm1_valid && isfinite(currentData.pm1) && currentData.pm1 >= 0.0f;
            if (pm1_available) {
                char buf[16];
                if (currentData.pm1 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm1);
                else snprintf(buf, sizeof(buf), "%.0f", currentData.pm1);
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissing());
            }
            const char *unit = objects.label_pm10_unit
                ? lv_label_get_text(objects.label_pm10_unit)
                : "ug/m3";
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            lv_color_t pm1_col = pm1_available ? getPM1Color(currentData.pm1) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(pm1_col));
            set_pm1_10_info_mode(pm1_10_graph_mode_);
            if (pm1_10_graph_mode_ &&
                should_refresh_active_graph(INFO_PM1, pm1_10_graph_range_, pm1_10_graph_points())) {
                update_pm1_10_info_graph();
            }
            break;
        }
        case INFO_CO: {
            const bool co_valid = currentData.co_valid && isfinite(currentData.co_ppm) && currentData.co_ppm >= 0.0f;
            if (co_valid) {
                char buf[16];
                if (currentData.co_ppm > 0.0f && currentData.co_ppm < 10.0f) {
                    snprintf(buf, sizeof(buf), "%.1f", currentData.co_ppm);
                } else {
                    snprintf(buf, sizeof(buf), "%.0f", currentData.co_ppm);
                }
                safe_label_set_text(objects.label_sensor_value, buf);
            } else {
                safe_label_set_text(objects.label_sensor_value, UiText::ValueMissingShort());
            }
            safe_label_set_text(objects.label_sensor_info_unit, "ppm");
            lv_color_t co_col = co_valid ? getCOColor(currentData.co_ppm) : color_inactive();
            set_dot_color(objects.dot_sensor_info, alert_color_for_mode(co_col));
            set_co_info_mode(co_graph_mode_);
            if (co_graph_mode_ &&
                should_refresh_active_graph(INFO_CO, co_graph_range_, co_graph_points())) {
                update_co_info_graph();
            }
            break;
        }
        case INFO_PRESSURE_3H:
        case INFO_PRESSURE_24H: {
            char buf[16];
            const bool pressure_value_available =
                currentData.pressure_valid && pressure_altitude_is_set();
            const bool pressure_prompt_altitude =
                currentData.pressure_valid && !pressure_altitude_is_set();
            if (pressure_value_available) {
                const float pressure_display = pressure_to_display(currentData.pressure);
                if (pressure_display_uses_inhg()) {
                    snprintf(buf, sizeof(buf), "%.1f", pressure_display);
                } else {
                    snprintf(buf, sizeof(buf), "%.0f", pressure_display);
                }
            } else {
                strcpy(buf, UiText::ValueMissing());
            }
            safe_label_set_text(objects.label_sensor_value, buf);
            safe_label_set_text(objects.label_pressure_value_1, buf);
            set_visible(objects.label_pressure_value_1, !pressure_prompt_altitude);
            set_visible(objects.label_pressure_unit_1, !pressure_prompt_altitude);
            set_visible(objects.label_pressure_set_altitude, pressure_prompt_altitude);

            const char *unit = pressure_display_unit();
            if (objects.label_pressure_unit_1) {
                safe_label_set_text_static(objects.label_pressure_unit_1, unit);
            }
            safe_label_set_text(objects.label_sensor_info_unit, pressure_value_available ? unit : "");

            if (currentData.pressure_delta_3h_valid) {
                const float delta_display = pressure_delta_to_display(currentData.pressure_delta_3h);
                const float plus_threshold = pressure_display_uses_inhg() ? 0.005f : 0.05f;
                if (delta_display > plus_threshold) {
                    if (pressure_display_uses_inhg()) {
                        snprintf(buf, sizeof(buf), "+%.2f", delta_display);
                    } else {
                        snprintf(buf, sizeof(buf), "+%.1f", delta_display);
                    }
                } else {
                    if (pressure_display_uses_inhg()) {
                        snprintf(buf, sizeof(buf), "%.2f", delta_display);
                    } else {
                        snprintf(buf, sizeof(buf), "%.1f", delta_display);
                    }
                }
            } else {
                strcpy(buf, UiText::ValueMissingShort());
            }
            safe_label_set_text(objects.label_delta_5, buf);

            if (currentData.pressure_delta_24h_valid) {
                const float delta_display = pressure_delta_to_display(currentData.pressure_delta_24h);
                const float plus_threshold = pressure_display_uses_inhg() ? 0.005f : 0.05f;
                if (delta_display > plus_threshold) {
                    if (pressure_display_uses_inhg()) {
                        snprintf(buf, sizeof(buf), "+%.2f", delta_display);
                    } else {
                        snprintf(buf, sizeof(buf), "+%.1f", delta_display);
                    }
                } else {
                    if (pressure_display_uses_inhg()) {
                        snprintf(buf, sizeof(buf), "%.2f", delta_display);
                    } else {
                        snprintf(buf, sizeof(buf), "%.1f", delta_display);
                    }
                }
            } else {
                strcpy(buf, UiText::ValueMissingShort());
            }
            safe_label_set_text(objects.label_delta_26, buf);

            lv_color_t delta_3h_color = night_mode
                ? color_card_border()
                : getPressureDeltaColor(pressure_delta_to_msl_hpa(currentData.pressure_delta_3h),
                                        currentData.pressure_delta_3h_valid,
                                        false);
            lv_color_t delta_24h_color = night_mode
                ? color_card_border()
                : getPressureDeltaColor(pressure_delta_to_msl_hpa(currentData.pressure_delta_24h),
                                        currentData.pressure_delta_24h_valid,
                                        true);
            set_chip_color(objects.chip_delta_4, delta_3h_color);
            set_chip_color(objects.chip_delta_25, delta_24h_color);
            set_dot_color(objects.dot_sensor_info, delta_3h_color);
            const bool pressure_graph_available = pressure_altitude_is_set();
            set_pressure_info_mode(pressure_graph_available && pressure_graph_mode_);
            if (pressure_graph_available && pressure_graph_mode_ &&
                should_refresh_active_graph(info_sensor, pressure_graph_range_, pressure_graph_points())) {
                update_pressure_info_graph();
            }
            break;
        }
        case INFO_NONE:
        default:
            break;
    }

    sync_threshold_dots_visibility();
    sync_info_graph_button_state();
    sync_pressure_altitude_ui();
}

void UiController::restore_sensor_info_selection() {
    switch (info_sensor) {
        case INFO_TEMP: {
            hide_all_sensor_info_containers();
            set_visible(objects.temperature_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleTemperature());
            }
            const char *value = nullptr;
            if (objects.label_temp_value_1) {
                value = lv_label_get_text(objects.label_temp_value_1);
            } else {
                value = UiText::ValueMissing();
            }
            safe_label_set_text(objects.label_sensor_value, value);
            const char *unit = nullptr;
            if (objects.label_temp_unit_1) {
                unit = lv_label_get_text(objects.label_temp_unit_1);
            } else {
                unit = temp_units_c ? UiText::UnitC() : UiText::UnitF();
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            set_temperature_info_mode(temp_graph_mode_);
            update_sensor_info_ui();
            break;
        }
        case INFO_VOC: {
            hide_all_sensor_info_containers();
            set_visible(objects.voc_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, "VOC");
            }
            const char *unit = nullptr;
            if (objects.label_voc_unit_1) {
                unit = lv_label_get_text(objects.label_voc_unit_1);
            } else {
                unit = UiText::UnitIndex();
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            set_voc_info_mode(voc_graph_mode_);
            update_sensor_info_ui();
            break;
        }
        case INFO_NOX: {
            hide_all_sensor_info_containers();
            set_visible(objects.nox_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, "NOx");
            }
            safe_label_set_text(objects.label_sensor_info_unit, UiText::UnitIndex());
            set_nox_info_mode(nox_graph_mode_);
            update_sensor_info_ui();
            break;
        }
        case INFO_OPTIONAL_GAS:
            select_optional_gas_info();
            break;
        case INFO_HCHO: {
            hide_all_sensor_info_containers();
            set_visible(objects.hcho_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleFormaldehyde());
            }
            const char *unit = nullptr;
            if (objects.label_hcho_unit_1) {
                unit = lv_label_get_text(objects.label_hcho_unit_1);
            } else {
                unit = UiText::UnitPpb();
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            set_visible(objects.label_hcho_text, true);
            set_visible(objects.label_aqi_text, false);
            set_visible(objects.aqi_info_thresholds, false);
            set_hcho_info_mode(hcho_graph_mode_);
            update_sensor_info_ui();
            break;
        }
        case INFO_AQI: {
            hide_all_sensor_info_containers();
            set_visible(objects.hcho_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, UiText::LabelAqi());
            }
            safe_label_set_text(objects.label_sensor_info_unit, UiText::UnitIndex());
            set_visible(objects.label_hcho_text, false);
            set_visible(objects.hcho_info_thresholds, false);
            set_visible(objects.hcho_info_graph, false);
            set_visible(objects.label_aqi_text, true);
            set_visible(objects.aqi_info_thresholds, true);
            update_sensor_info_ui();
            break;
        }
        case INFO_CO2: {
            hide_all_sensor_info_containers();
            set_visible(objects.co2_info, true);
            if (objects.label_sensor_info_title) {
                safe_label_set_text(objects.label_sensor_info_title, "CO2");
            }
            const char *unit = nullptr;
            if (objects.label_co2_unit_1) {
                unit = lv_label_get_text(objects.label_co2_unit_1);
            } else {
                unit = "ppm";
            }
            safe_label_set_text(objects.label_sensor_info_unit, unit);
            set_co2_info_mode(co2_graph_mode_);
            update_sensor_info_ui();
            break;
        }
        case INFO_RH:
        case INFO_AH:
        case INFO_MR:
        case INFO_DP:
            select_humidity_info(info_sensor);
            break;
        case INFO_PM05:
        case INFO_PM25:
        case INFO_PM4:
        case INFO_PM10:
        case INFO_PM1:
        case INFO_CO:
            select_pm_info(info_sensor);
            break;
        case INFO_PRESSURE_3H:
        case INFO_PRESSURE_24H:
            select_pressure_info(info_sensor);
            break;
        case INFO_NONE:
        default:
            hide_all_sensor_info_containers();
            break;
    }
}

void UiController::select_humidity_info(InfoSensor sensor) {
    info_sensor = sensor;
    if (sensor != INFO_RH) {
        rh_graph_mode_ = false;
    }
    hide_all_sensor_info_containers();

    const bool show_rh_ah = (sensor == INFO_RH) || (sensor == INFO_AH);
    const bool show_mr_dp = (sensor == INFO_MR) || (sensor == INFO_DP);
    set_visible(objects.humidity_info_rh_ah, show_rh_ah);
    set_visible(objects.humidity_info_mr_dp, show_mr_dp);
    set_visible(objects.rh_info, sensor == INFO_RH);
    set_visible(objects.ah_info, sensor == INFO_AH);
    set_visible(objects.mr_info, sensor == INFO_MR);
    set_visible(objects.dp_info, sensor == INFO_DP);
    set_rh_info_mode((sensor == INFO_RH) && rh_graph_mode_);

    set_checked_state(objects.btn_rh_info, sensor == INFO_RH);
    set_checked_state(objects.btn_ah_info, sensor == INFO_AH);
    set_checked_state(objects.btn_mr_info, sensor == INFO_MR);
    set_checked_state(objects.btn_dp_info, sensor == INFO_DP);

    if (objects.label_sensor_info_title) {
        if (sensor == INFO_RH) {
            safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleRh());
        } else if (sensor == INFO_AH) {
            safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleAh());
        } else if (sensor == INFO_MR) {
            safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleMr());
        } else if (sensor == INFO_DP) {
            safe_label_set_text(objects.label_sensor_info_title, UiText::SensorInfoTitleDp());
        }
    }
    update_sensor_info_ui();
}

void UiController::select_pm_info(InfoSensor sensor) {
    info_sensor = sensor;
    hide_all_sensor_info_containers();

    if (sensor == INFO_CO) {
        set_visible(objects.co_info, true);
        set_co_info_mode(co_graph_mode_);
        if (objects.label_sensor_info_title) {
            safe_label_set_text(objects.label_sensor_info_title, "CO");
        }
        update_sensor_info_ui();
        return;
    }

    set_visible(objects.pm_info, true);
    const bool pm25_pm4_group = (sensor == INFO_PM25) || (sensor == INFO_PM4);
    const bool pm1_pm10_group = (sensor == INFO_PM1) || (sensor == INFO_PM10);
    set_visible(objects.pm1_pm10_info, pm1_pm10_group);
    set_visible(objects.pm05_info, sensor == INFO_PM05);
    set_visible(objects.pm25_info, pm25_pm4_group);
    set_visible(objects.pm10_info, sensor == INFO_PM10);
    set_visible(objects.pm1_info, sensor == INFO_PM1);
    set_visible(objects.pm05_info_graph, false);
    set_visible(objects.pm25_4_graph, false);
    set_visible(objects.pm1_10_info_graph, false);
    set_visible(objects.pm05_info_thresholds, sensor == INFO_PM05);
    set_visible(objects.pm25_info_thresholds, sensor == INFO_PM25);
    set_visible(objects.pm4_info_thresholds, sensor == INFO_PM4);
    set_visible(objects.label_pm25_text, sensor == INFO_PM25);
    set_visible(objects.label_pm4_text, sensor == INFO_PM4);
    set_visible(objects.pm1_info_thresholds, sensor == INFO_PM1);
    set_visible(objects.pm10_info_thresholds, sensor == INFO_PM10);
    if (sensor == INFO_PM05) {
        set_pm05_info_mode(pm05_graph_mode_);
    }
    if (pm25_pm4_group) {
        set_checked_state(objects.btn_pm25_info, sensor == INFO_PM25);
        set_checked_state(objects.btn_pm4_info, sensor == INFO_PM4);
        if (objects.btn_pm25_info) lv_obj_move_foreground(objects.btn_pm25_info);
        if (objects.btn_pm4_info) lv_obj_move_foreground(objects.btn_pm4_info);
        set_pm25_4_info_mode(pm25_4_graph_mode_);
    }
    if (pm1_pm10_group) {
        set_checked_state(objects.btn_pm10_info, sensor == INFO_PM10);
        set_checked_state(objects.btn_pm1_info, sensor == INFO_PM1);
        // Keep PM tab buttons above PM1/PM10 content layers so they remain clickable.
        if (objects.btn_pm10_info) lv_obj_move_foreground(objects.btn_pm10_info);
        if (objects.btn_pm1_info) lv_obj_move_foreground(objects.btn_pm1_info);
        set_pm1_10_info_mode(pm1_10_graph_mode_);
    }

    if (objects.label_sensor_info_title) {
        if (sensor == INFO_PM05) {
            safe_label_set_text(objects.label_sensor_info_title, "PM0.5");
        } else if (sensor == INFO_PM25) {
            safe_label_set_text(objects.label_sensor_info_title, "PM2.5");
        } else if (sensor == INFO_PM4) {
            safe_label_set_text(objects.label_sensor_info_title, "PM4");
        } else if (sensor == INFO_PM10) {
            safe_label_set_text(objects.label_sensor_info_title, "PM10");
        } else if (sensor == INFO_PM1) {
            safe_label_set_text(objects.label_sensor_info_title, "PM1");
        }
    }
    update_sensor_info_ui();
}

void UiController::select_optional_gas_info() {
    info_sensor = INFO_OPTIONAL_GAS;
    hide_all_sensor_info_containers();
    set_visible(objects.optional_gas_info, true);
    set_optional_gas_info_mode(optional_gas_graph_mode_);

    using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;
    const OptionalGasType type = static_cast<OptionalGasType>(currentData.optional_gas_type);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    if (objects.label_sensor_info_title) {
        safe_label_set_text(objects.label_sensor_info_title, profile.label);
    }
    safe_label_set_text(objects.label_sensor_info_unit, "ppm");

    update_sensor_info_ui();
}

void UiController::select_pressure_info(InfoSensor sensor) {
    info_sensor = sensor;
    hide_all_sensor_info_containers();
    set_visible(objects.pressure_info, true);
    set_visible(objects.pressure_3h_info, sensor == INFO_PRESSURE_3H);
    set_visible(objects.pressure_24h_info, sensor == INFO_PRESSURE_24H);

    set_checked_state(objects.btn_3h_pressure_info, sensor == INFO_PRESSURE_3H);
    set_checked_state(objects.btn_24h_pressure_info, sensor == INFO_PRESSURE_24H);
    set_pressure_info_mode(pressure_graph_mode_);

    safe_label_set_text(objects.label_sensor_info_title, UiText::LabelPressureTitle());

    update_sensor_info_ui();
}

void UiController::set_visible(lv_obj_t *obj, bool visible) {
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiController::hide_all_sensor_info_containers() {
    pressure_altitude_overlay_open_ = false;
    release_all_sensor_graph_runtime_objects();
    set_visible(objects.container_set_altitude, false);
    set_visible(objects.btn_set_altitude, false);
    set_visible(objects.label_sensor_info_title, true);
    set_visible(objects.label_sensor_value, true);
    set_visible(objects.dot_sensor_info, true);
    set_visible(objects.label_sensor_info_unit, true);
    set_visible(objects.temperature_info, false);
    set_visible(objects.temperature_info_thresholds, false);
    set_visible(objects.temperature_info_graph, false);
    set_visible(objects.container_thresholds_dots, false);
    set_visible(objects.co2_info, false);
    set_visible(objects.co2_info_thresholds, false);
    set_visible(objects.co2_info_graph, false);
    set_visible(objects.voc_info, false);
    set_visible(objects.voc_info_thresholds, false);
    set_visible(objects.voc_info_graph, false);
    set_visible(objects.nox_info, false);
    set_visible(objects.nox_info_thresholds, false);
    set_visible(objects.nox_info_graph, false);
    set_visible(objects.optional_gas_info, false);
    set_visible(objects.optional_gas_info_thresholds, false);
    set_visible(objects.optional_gas_info_graph, false);
    set_visible(objects.hcho_info, false);
    set_visible(objects.label_hcho_text, false);
    set_visible(objects.hcho_info_thresholds, false);
    set_visible(objects.hcho_info_graph, false);
    set_visible(objects.label_aqi_text, false);
    set_visible(objects.aqi_info_thresholds, false);
    set_visible(objects.co_info, false);
    set_visible(objects.co_info_thresholds, false);
    set_visible(objects.co_info_graph, false);
    set_visible(objects.humidity_info_rh_ah, false);
    set_visible(objects.humidity_info_mr_dp, false);
    set_visible(objects.rh_info, false);
    set_visible(objects.rh_info_thresholds, false);
    set_visible(objects.rh_info_graph, false);
    set_visible(objects.ah_info, false);
    set_visible(objects.mr_info, false);
    set_visible(objects.dp_info, false);
    set_visible(objects.pressure_info, false);
    set_visible(objects.pressure_3h_info, false);
    set_visible(objects.pressure_24h_info, false);
    set_visible(objects.pressure_info_graph, false);
    set_visible(objects.pm_info, false);
    set_visible(objects.pm1_pm10_info, false);
    set_visible(objects.pm1_10_info_graph, false);
    set_visible(objects.pm05_info, false);
    set_visible(objects.pm05_info_thresholds, false);
    set_visible(objects.pm05_info_graph, false);
    set_visible(objects.pm10_info, false);
    set_visible(objects.pm10_info_thresholds, false);
    set_visible(objects.pm25_info, false);
    set_visible(objects.pm25_info_thresholds, false);
    set_visible(objects.pm4_info_thresholds, false);
    set_visible(objects.pm25_4_graph, false);
    set_visible(objects.pm1_info, false);
    set_visible(objects.pm1_info_thresholds, false);
}

