// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"
#include "ui/UiOptionalGasProfile.h"
#include "ui/UiText.h"
#include "ui/ui.h"
#include "ui/fonts.h"
#include "core/MathUtils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

bool has_co_sensor_data(const SensorData &data) {
    return data.co_sensor_present;
}

bool has_valid_co_sensor_data(const SensorData &data) {
    return data.co_sensor_present &&
           data.co_valid &&
           isfinite(data.co_ppm) &&
           data.co_ppm >= 0.0f;
}

using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;

OptionalGasType get_optional_gas_type(const SensorData &data) {
    return static_cast<OptionalGasType>(data.optional_gas_type);
}

bool has_optional_gas_sensor_data(const SensorData &data) {
    return data.optional_gas_sensor_present &&
           get_optional_gas_type(data) != OptionalGasType::None;
}

bool has_valid_optional_gas_sensor_data(const SensorData &data) {
    return data.optional_gas_sensor_present &&
           data.optional_gas_valid &&
           get_optional_gas_type(data) != OptionalGasType::None &&
           isfinite(data.optional_gas_ppm) &&
           data.optional_gas_ppm >= 0.0f;
}

float get_optional_gas_ppm_value(const SensorData &data) {
    return data.optional_gas_ppm;
}

const char *get_optional_gas_label(const SensorData &data) {
    const OptionalGasType type = get_optional_gas_type(data);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    return UiOptionalGasProfile::isKnown(type)
        ? profile.label
        : DfrOptionalGasSensor::optionalGasLabel(type);
}

void format_optional_gas_value(const SensorData &data, char *buf, size_t buf_size) {
    const OptionalGasType type = get_optional_gas_type(data);
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    UiOptionalGasProfile::formatValue(profile,
                                      get_optional_gas_ppm_value(data),
                                      data.optional_gas_ppm_decimals,
                                      buf,
                                      buf_size);
}

float get_co_ppm_value(const SensorData &data) {
    return data.co_ppm;
}

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

} // namespace

void UiController::update_sensor_cards(const AirQuality &aq, bool gas_warmup, bool show_co2_bar) {
    char buf[16];

    if (currentData.co2_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.co2);
        safe_label_set_text(objects.label_co2_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_co2_value_1, UiText::ValueMissing());
    }
    if (objects.co2_bar_wrap_1) {
        show_co2_bar ? lv_obj_clear_flag(objects.co2_bar_wrap_1, LV_OBJ_FLAG_HIDDEN)
                     : lv_obj_add_flag(objects.co2_bar_wrap_1, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t co2_col = currentData.co2_valid ? getCO2Color(currentData.co2) : color_inactive();
    set_dot_color(objects.dot_co2_1, alert_color_for_mode(co2_col));
    if (show_co2_bar) {
        sync_co2_marker_border();
        set_dot_color(objects.co2_marker_1, co2_col);
        update_co2_bar(currentData.co2, currentData.co2_valid);

        if (objects.co2_bar_fill_1 && objects.co2_marker_1) {
            if (!currentData.co2_valid) {
                if (objects.co2_bar_mask_1) {
                    lv_obj_set_width(objects.co2_bar_mask_1, 0);
                } else {
                    lv_obj_set_width(objects.co2_bar_fill_1, 0);
                }
                lv_obj_set_x(objects.co2_marker_1, 2);
            } else {
                int bar_max = 330;
                int fill_w = lv_obj_get_width(objects.co2_bar_fill_1);
                if (fill_w > 0) {
                    bar_max = fill_w;
                }
                int clamped = constrain(currentData.co2, 400, 2000);
                int w = map(clamped, 400, 2000, 0, bar_max);
                w = constrain(w, 0, bar_max);
                if (objects.co2_bar_mask_1) {
                    lv_obj_set_width(objects.co2_bar_mask_1, w);
                } else {
                    lv_obj_set_width(objects.co2_bar_fill_1, w);
                }

                const int marker_w = 14;
                int center = 4 + w;
                int x = center - (marker_w / 2);
                int track_w = objects.co2_bar_track_1 ? lv_obj_get_width(objects.co2_bar_track_1) : 0;
                int max_x = (track_w > 0) ? (track_w - marker_w - 2) : (340 - marker_w - 2);
                x = constrain(x, 2, max_x);
                lv_obj_set_x(objects.co2_marker_1, x);
            }
            set_dot_color(objects.co2_marker_1, co2_col);
        }
    }

    if (currentData.temp_valid) {
        float temp_display = currentData.temperature;
        if (!temp_units_c) {
            temp_display = (temp_display * 9.0f / 5.0f) + 32.0f;
        }
        snprintf(buf, sizeof(buf), "%.1f", temp_display);
        safe_label_set_text(objects.label_temp_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_temp_value_1, UiText::ValueMissing());
    }
    if (objects.label_temp_unit_1) {
        safe_label_set_text_static(objects.label_temp_unit_1, temp_units_c ? UiText::UnitC() : UiText::UnitF());
    }
    lv_color_t temp_col = currentData.temp_valid ? getTempColor(currentData.temperature) : color_inactive();
    set_dot_color(objects.dot_temp_1, alert_color_for_mode(temp_col));

    if (currentData.hum_valid) {
        snprintf(buf, sizeof(buf), "%.0f", currentData.humidity);
        safe_label_set_text(objects.label_hum_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_hum_value_1, UiText::ValueMissingShort());
    }
    lv_color_t hum_col = currentData.hum_valid ? getHumidityColor(currentData.humidity) : color_inactive();
    set_dot_color(objects.dot_hum_1, alert_color_for_mode(hum_col));

    float dew_c = NAN;
    float dew_c_rounded = NAN;
    float ah_gm3 = NAN;
    if (currentData.temp_valid && currentData.hum_valid) {
        dew_c = MathUtils::compute_dew_point_c(currentData.temperature, currentData.humidity);
        if (isfinite(dew_c)) {
            dew_c_rounded = roundf(dew_c);
        }
        ah_gm3 = MathUtils::compute_absolute_humidity_gm3(currentData.temperature, currentData.humidity);
    }
    if (isfinite(dew_c)) {
        float dew_display = dew_c;
        if (!temp_units_c) {
            dew_display = (dew_display * 9.0f / 5.0f) + 32.0f;
        } else if (isfinite(dew_c_rounded)) {
            dew_display = dew_c_rounded;
        }
        snprintf(buf, sizeof(buf), "%.0f", dew_display);
        safe_label_set_text(objects.label_dew_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_dew_value_1, UiText::ValueMissingShort());
    }
    if (objects.label_dew_unit_1) {
        safe_label_set_text_static(objects.label_dew_unit_1, temp_units_c ? UiText::UnitC() : UiText::UnitF());
    }
    float dp_color_c = isfinite(dew_c_rounded) ? dew_c_rounded : dew_c;
    lv_color_t dp_col = getDewPointColor(dp_color_c);
    if (objects.dot_dp_1) {
        set_dot_color(objects.dot_dp_1, alert_color_for_mode(dp_col));
    }
    if (isfinite(ah_gm3)) {
        snprintf(buf, sizeof(buf), "%.0f", ah_gm3);
        safe_label_set_text(objects.label_ah_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_ah_value_1, UiText::ValueMissingShort());
    }
    lv_color_t ah_col = getAbsoluteHumidityColor(ah_gm3);
    if (objects.dot_ah_1) {
        set_dot_color(objects.dot_ah_1, alert_color_for_mode(ah_col));
    }

    const int mold_risk =
        (currentData.temp_valid && currentData.hum_valid)
            ? MathUtils::compute_mold_risk_index(currentData.temperature, currentData.humidity)
            : -1;
    if (objects.label_mr_value) {
        if (mold_risk >= 0) {
            snprintf(buf, sizeof(buf), "%d", mold_risk);
            safe_label_set_text(objects.label_mr_value, buf);
        } else {
            safe_label_set_text_static(objects.label_mr_value, UiText::ValueMissingShort());
        }
    }
    if (objects.dot_mr) {
        lv_color_t mr_col = color_inactive();
        if (mold_risk >= 0) {
            if (mold_risk <= 2) mr_col = color_green();
            else if (mold_risk <= 4) mr_col = color_yellow();
            else if (mold_risk <= 7) mr_col = color_orange();
            else mr_col = color_red();
        }
        set_dot_color(objects.dot_mr, alert_color_for_mode(mr_col));
    }

    if (currentData.pm25_valid) {
        if (currentData.pm25 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm25);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm25);
        safe_label_set_text(objects.label_pm25_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_pm25_value_1, UiText::ValueMissing());
    }
    lv_color_t pm25_col = currentData.pm25_valid ? getPM25Color(currentData.pm25) : color_inactive();
    set_dot_color(objects.dot_pm25_1, alert_color_for_mode(pm25_col));

    if (currentData.pm10_valid) {
        if (currentData.pm10 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm10);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm10);
        safe_label_set_text(objects.label_pm10_value_pro, buf);
    } else {
        safe_label_set_text_static(objects.label_pm10_value_pro, UiText::ValueMissing());
    }
    lv_color_t pm10_col = currentData.pm10_valid ? getPM10Color(currentData.pm10) : color_inactive();
    set_dot_color(objects.dot_pm10_pro, alert_color_for_mode(pm10_col));

    if (currentData.pm05_valid) {
        if (!format_pm05_count(currentData.pm05, buf, sizeof(buf))) {
            strcpy(buf, UiText::ValueMissing());
        }
        safe_label_set_text(objects.label_pm05_value, buf);
    } else {
        safe_label_set_text_static(objects.label_pm05_value, UiText::ValueMissing());
    }
    lv_color_t pm05_col = currentData.pm05_valid ? getPM05Color(currentData.pm05) : color_inactive();
    set_dot_color(objects.dot_pm05, alert_color_for_mode(pm05_col));

    const bool pm1_available = currentData.pm1_valid && isfinite(currentData.pm1) && currentData.pm1 >= 0.0f;
    const bool co_sensor_present = has_co_sensor_data(currentData);
    const bool show_pm1_in_pm10_card = co_sensor_present;
    set_visible(objects.label_pm1_title, show_pm1_in_pm10_card);
    set_visible(objects.label_pm1_value, show_pm1_in_pm10_card);
    set_visible(objects.dot_pm1, show_pm1_in_pm10_card && led_indicators_enabled);
    set_visible(objects.label_pm10_unit, !show_pm1_in_pm10_card);

    if (show_pm1_in_pm10_card && objects.label_pm1_value) {
        if (pm1_available) {
            if (currentData.pm1 < 10.0f) {
                snprintf(buf, sizeof(buf), "%.1f", currentData.pm1);
            } else {
                snprintf(buf, sizeof(buf), "%.0f", currentData.pm1);
            }
            safe_label_set_text(objects.label_pm1_value, buf);
        } else {
            safe_label_set_text_static(objects.label_pm1_value, UiText::ValueMissing());
        }
    }
    if (show_pm1_in_pm10_card && objects.dot_pm1) {
        lv_color_t pm1_col = pm1_available ? getPM1Color(currentData.pm1) : color_inactive();
        set_dot_color(objects.dot_pm1, alert_color_for_mode(pm1_col));
    }

    const OptionalGasType optional_gas_type = get_optional_gas_type(currentData);
    const bool optional_gas_present = has_optional_gas_sensor_data(currentData);
    const bool optional_gas_available = has_valid_optional_gas_sensor_data(currentData);
    const bool optional_gas_warmup = optional_gas_present && currentData.optional_gas_warmup;
    // With optional gas connected, keep VOC and NOx visible together and reuse the standalone NOx slot.
    const bool show_voc_nox_combo = optional_gas_present;
    set_visible(objects.card_voc_pro, !show_voc_nox_combo);
    set_visible(objects.card_voc_nox, show_voc_nox_combo);
    if (objects.card_nox_pro) {
        lv_obj_add_flag(objects.card_nox_pro, LV_OBJ_FLAG_CLICKABLE);
    }

    if (currentData.voc_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.voc_index);
        safe_label_set_text(objects.label_voc_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_voc_value_1, UiText::ValueMissing());
    }
    if (objects.label_voc_warmup_1) {
        gas_warmup ? lv_obj_clear_flag(objects.label_voc_warmup_1, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_add_flag(objects.label_voc_warmup_1, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_voc_value_1) {
        gas_warmup ? lv_obj_add_flag(objects.label_voc_value_1, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_voc_value_1, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_voc_unit_1) {
        gas_warmup ? lv_obj_add_flag(objects.label_voc_unit_1, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_voc_unit_1, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t voc_col = gas_warmup ? color_blue()
                                    : (currentData.voc_valid ? getVOCColor(currentData.voc_index) : color_inactive());
    set_dot_color(objects.dot_voc_1, alert_color_for_mode(voc_col));

    lv_color_t nox_col = gas_warmup ? color_blue()
                                    : (currentData.nox_valid ? getNOxColor(currentData.nox_index) : color_inactive());

    if (objects.label_voc_value_2) {
        if (gas_warmup) {
            safe_label_set_text(objects.label_voc_value_2, "---");
        } else if (currentData.voc_valid) {
            snprintf(buf, sizeof(buf), "%d", currentData.voc_index);
            safe_label_set_text(objects.label_voc_value_2, buf);
        } else {
            safe_label_set_text_static(objects.label_voc_value_2, UiText::ValueMissingShort());
        }
    }
    if (objects.label_nox_value_2) {
        if (gas_warmup) {
            safe_label_set_text(objects.label_nox_value_2, "---");
        } else if (currentData.nox_valid) {
            snprintf(buf, sizeof(buf), "%d", currentData.nox_index);
            safe_label_set_text(objects.label_nox_value_2, buf);
        } else {
            safe_label_set_text_static(objects.label_nox_value_2, UiText::ValueMissingShort());
        }
    }
    if (objects.dot_voc_2) {
        set_dot_color(objects.dot_voc_2, alert_color_for_mode(voc_col));
    }
    if (objects.dot_nox_2) {
        set_dot_color(objects.dot_nox_2, alert_color_for_mode(nox_col));
    }

    const bool nox_card_is_optional_gas = optional_gas_present;
    const bool nox_card_warmup = nox_card_is_optional_gas ? optional_gas_warmup : gas_warmup;
    const lv_color_t nox_card_col = nox_card_is_optional_gas
        ? (optional_gas_warmup
               ? color_blue()
               : getOptionalGasColor(optional_gas_type,
                                     currentData.optional_gas_ppm,
                                     optional_gas_available))
        : nox_col;
    if (objects.label_nox_title_1) {
        safe_label_set_text_static(objects.label_nox_title_1,
                                   nox_card_is_optional_gas ? get_optional_gas_label(currentData) : "NOx");
    }
    if (objects.label_nox_unit_1) {
        safe_label_set_text_static(objects.label_nox_unit_1,
                                   nox_card_is_optional_gas ? "ppm" : UiText::UnitIndex());
        nox_card_warmup ? lv_obj_add_flag(objects.label_nox_unit_1, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_clear_flag(objects.label_nox_unit_1, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_nox_warmup_1) {
        nox_card_warmup ? lv_obj_clear_flag(objects.label_nox_warmup_1, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_add_flag(objects.label_nox_warmup_1, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_nox_value_1) {
        nox_card_warmup ? lv_obj_add_flag(objects.label_nox_value_1, LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_clear_flag(objects.label_nox_value_1, LV_OBJ_FLAG_HIDDEN);
        if (nox_card_is_optional_gas) {
            if (optional_gas_available) {
                format_optional_gas_value(currentData, buf, sizeof(buf));
                safe_label_set_text(objects.label_nox_value_1, buf);
            } else {
                safe_label_set_text_static(objects.label_nox_value_1, UiText::ValueMissing());
            }
        } else if (currentData.nox_valid) {
            snprintf(buf, sizeof(buf), "%d", currentData.nox_index);
            safe_label_set_text(objects.label_nox_value_1, buf);
        } else {
            safe_label_set_text_static(objects.label_nox_value_1, UiText::ValueMissing());
        }
    }
    if (objects.dot_nox_1) {
        set_dot_color(objects.dot_nox_1, alert_color_for_mode(nox_card_col));
    }

    const bool hcho_warmup = sensorManager.isSfaWarmupActive();
    const bool hcho_available = currentData.hcho_valid;
    if (objects.label_hcho_title_1) {
        safe_label_set_text_static(objects.label_hcho_title_1,
                                   (hcho_available || hcho_warmup)
                                       ? UiText::LabelHcho()
                                       : UiText::LabelAqi());
    }
    if (objects.label_hcho_unit_1) {
        safe_label_set_text_static(objects.label_hcho_unit_1,
                                   (hcho_available || hcho_warmup)
                                       ? UiText::UnitPpb()
                                       : UiText::UnitIndex());
    }
    if (objects.label_hcho_value_1) {
        if (hcho_available) {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(lroundf(currentData.hcho)));
        } else if (hcho_warmup) {
            snprintf(buf, sizeof(buf), "...");
        } else {
            snprintf(buf, sizeof(buf), "%d", aq.score);
        }
        safe_label_set_text(objects.label_hcho_value_1, buf);
    }
    if (objects.dot_hcho_1) {
        lv_color_t hcho_col = hcho_available ? getHCHOColor(currentData.hcho, true)
                                             : (hcho_warmup ? color_blue() : aq.color);
        set_dot_color(objects.dot_hcho_1, alert_color_for_mode(hcho_col));
    }

    const bool co_warmup = co_sensor_present && currentData.co_warmup;
    const bool co_available = has_valid_co_sensor_data(currentData);
    if (objects.label_co_title) {
        safe_label_set_text_static(objects.label_co_title, co_sensor_present ? "CO" : "PM1");
    }
    if (objects.label_co_unit) {
        const lv_font_t *unit_font = &ui_font_jet_reg_14;
        if (ui_language == Config::Language::ZH) {
            unit_font = &ui_font_noto_sans_sc_reg_14;
        } else if (ui_language == Config::Language::JA) {
            unit_font = &ui_font_noto_sans_jp_reg_14;
        }
        const lv_font_t *current_font =
            lv_obj_get_style_text_font(objects.label_co_unit, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (current_font != unit_font) {
            lv_obj_set_style_text_font(objects.label_co_unit, unit_font, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        safe_label_set_text_static(objects.label_co_unit, co_sensor_present ? "ppm" : "ug/m\xC2\xB3");
    }
    if (objects.label_co_warmup) {
        co_warmup ? lv_obj_clear_flag(objects.label_co_warmup, LV_OBJ_FLAG_HIDDEN)
                  : lv_obj_add_flag(objects.label_co_warmup, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_co_value) {
        co_warmup ? lv_obj_add_flag(objects.label_co_value, LV_OBJ_FLAG_HIDDEN)
                  : lv_obj_clear_flag(objects.label_co_value, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_co_unit) {
        co_warmup ? lv_obj_add_flag(objects.label_co_unit, LV_OBJ_FLAG_HIDDEN)
                  : lv_obj_clear_flag(objects.label_co_unit, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_co_value) {
        if (co_sensor_present) {
            if (co_available) {
                float co_ppm = get_co_ppm_value(currentData);
                snprintf(buf, sizeof(buf), "%.0f", co_ppm);
            } else {
                strcpy(buf, UiText::ValueMissingShort());
            }
        } else if (pm1_available) {
            if (currentData.pm1 < 10.0f) {
                snprintf(buf, sizeof(buf), "%.1f", currentData.pm1);
            } else {
                snprintf(buf, sizeof(buf), "%.0f", currentData.pm1);
            }
        } else {
            strcpy(buf, UiText::ValueMissing());
        }
        safe_label_set_text(objects.label_co_value, buf);
    }
    if (objects.dot_co) {
        lv_color_t co_card_col = color_inactive();
        if (co_sensor_present) {
            if (co_warmup) {
                co_card_col = color_blue();
            } else if (co_available) {
                co_card_col = getCOColor(get_co_ppm_value(currentData));
            }
        } else if (pm1_available) {
            co_card_col = getPM1Color(currentData.pm1);
        }
        set_dot_color(objects.dot_co, alert_color_for_mode(co_card_col));
    }

    // PRO divider lines follow active theme border color, no shadow.
    const lv_color_t divider_col = color_card_border();
    if (objects.line_1) {
        lv_obj_set_style_line_color(objects.line_1, divider_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(objects.line_1, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.line_2) {
        lv_obj_set_style_line_color(objects.line_2, divider_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(objects.line_2, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    const bool pressure_prompt_altitude =
        currentData.pressure_valid && !pressure_altitude_is_set();
    if (currentData.pressure_valid && !pressure_prompt_altitude) {
        const float pressure_display = pressure_to_display(currentData.pressure);
        if (pressure_display_uses_inhg()) {
            snprintf(buf, sizeof(buf), "%.1f", pressure_display);
        } else {
            snprintf(buf, sizeof(buf), "%.0f", pressure_display);
        }
        safe_label_set_text(objects.label_pressure_value_1, buf);
    } else {
        safe_label_set_text_static(objects.label_pressure_value_1, UiText::ValueMissing());
    }
    if (objects.label_pressure_unit_1) {
        safe_label_set_text_static(objects.label_pressure_unit_1, pressure_display_unit());
    }
    set_visible(objects.label_pressure_value_1, !pressure_prompt_altitude);
    set_visible(objects.label_pressure_unit_1, !pressure_prompt_altitude);
    set_visible(objects.label_pressure_set_altitude, pressure_prompt_altitude);

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
        safe_label_set_text(objects.label_delta_5, buf);
    } else {
        safe_label_set_text_static(objects.label_delta_5, UiText::ValueMissingShort());
    }

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
        safe_label_set_text(objects.label_delta_26, buf);
    } else {
        safe_label_set_text_static(objects.label_delta_26, UiText::ValueMissingShort());
    }

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
}
