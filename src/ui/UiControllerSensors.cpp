// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"
#include "ui/UiText.h"
#include "ui/ui.h"
#include "core/MathUtils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void UiController::update_sensor_cards(const AirQuality &aq, bool gas_warmup, bool show_co2_bar) {
    char buf[16];

    if (currentData.co2_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.co2);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    safe_label_set_text(objects.label_co2_value, buf);
    if (objects.co2_bar_wrap) {
        show_co2_bar ? lv_obj_clear_flag(objects.co2_bar_wrap, LV_OBJ_FLAG_HIDDEN)
                     : lv_obj_add_flag(objects.co2_bar_wrap, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t co2_col = currentData.co2_valid ? getCO2Color(currentData.co2) : color_inactive();
    set_dot_color(objects.dot_co2, alert_color_for_mode(co2_col));
    if (show_co2_bar) {
        set_dot_color(objects.co2_marker, co2_col);
        update_co2_bar(currentData.co2, currentData.co2_valid);
    }

    if (currentData.temp_valid) {
        float temp_display = currentData.temperature;
        if (!temp_units_c) {
            temp_display = (temp_display * 9.0f / 5.0f) + 32.0f;
        }
        snprintf(buf, sizeof(buf), "%.1f", temp_display);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    if (objects.label_temp_value) {
        safe_label_set_text(objects.label_temp_value, buf);
    }
    if (objects.label_temp_unit) {
        safe_label_set_text(objects.label_temp_unit, temp_units_c ? UiText::kUnitC : UiText::kUnitF);
    }
    lv_color_t temp_col = currentData.temp_valid ? getTempColor(currentData.temperature) : color_inactive();
    set_dot_color(objects.dot_temp, alert_color_for_mode(temp_col));

    if (currentData.hum_valid) {
        snprintf(buf, sizeof(buf), "%.0f", currentData.humidity);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    safe_label_set_text(objects.label_hum_value, buf);
    lv_color_t hum_col = currentData.hum_valid ? getHumidityColor(currentData.humidity) : color_inactive();
    set_dot_color(objects.dot_hum, alert_color_for_mode(hum_col));

    float dew_c = NAN;
    if (currentData.temp_valid && currentData.hum_valid) {
        dew_c = MathUtils::compute_dew_point_c(currentData.temperature, currentData.humidity);
    }
    if (objects.label_dew_value) {
        if (isfinite(dew_c)) {
            float dew_display = dew_c;
            if (!temp_units_c) {
                dew_display = (dew_display * 9.0f / 5.0f) + 32.0f;
            }
            snprintf(buf, sizeof(buf), "%.1f", dew_display);
        } else {
            strcpy(buf, UiText::kValueMissing);
        }
        safe_label_set_text(objects.label_dew_value, buf);
    }
    if (objects.label_dew_unit) {
        safe_label_set_text(objects.label_dew_unit, temp_units_c ? UiText::kUnitC : UiText::kUnitF);
    }
    if (objects.dot_dp) {
        lv_color_t dp_col = getDewPointColor(dew_c);
        set_dot_color(objects.dot_dp, alert_color_for_mode(dp_col));
    }

    if (currentData.pm25_valid) {
        if (currentData.pm25 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm25);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm25);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    safe_label_set_text(objects.label_pm25_value, buf);
    lv_color_t pm25_col = currentData.pm25_valid ? getPM25Color(currentData.pm25) : color_inactive();
    set_dot_color(objects.dot_pm25, alert_color_for_mode(pm25_col));

    if (currentData.pm10_valid) {
        if (currentData.pm10 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm10);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm10);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    safe_label_set_text(objects.label_pm10_value, buf);
    lv_color_t pm10_col = currentData.pm10_valid ? getPM10Color(currentData.pm10) : color_inactive();
    set_dot_color(objects.dot_pm10, alert_color_for_mode(pm10_col));

    if (currentData.voc_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.voc_index);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    if (objects.label_voc_value) {
        safe_label_set_text(objects.label_voc_value, buf);
    }
    if (objects.label_voc_warmup) {
        gas_warmup ? lv_obj_clear_flag(objects.label_voc_warmup, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_add_flag(objects.label_voc_warmup, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_voc_value) {
        gas_warmup ? lv_obj_add_flag(objects.label_voc_value, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_voc_value, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_voc_unit) {
        gas_warmup ? lv_obj_add_flag(objects.label_voc_unit, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_voc_unit, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t voc_col = gas_warmup ? color_blue()
                                    : (currentData.voc_valid ? getVOCColor(currentData.voc_index) : color_inactive());
    set_dot_color(objects.dot_voc, gas_warmup ? voc_col : alert_color_for_mode(voc_col));

    if (currentData.nox_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.nox_index);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    if (objects.label_nox_value) {
        safe_label_set_text(objects.label_nox_value, buf);
    }
    if (objects.label_nox_warmup) {
        gas_warmup ? lv_obj_clear_flag(objects.label_nox_warmup, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_add_flag(objects.label_nox_warmup, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_nox_value) {
        gas_warmup ? lv_obj_add_flag(objects.label_nox_value, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_nox_value, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.label_nox_unit) {
        gas_warmup ? lv_obj_add_flag(objects.label_nox_unit, LV_OBJ_FLAG_HIDDEN)
                   : lv_obj_clear_flag(objects.label_nox_unit, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t nox_col = gas_warmup ? color_blue()
                                    : (currentData.nox_valid ? getNOxColor(currentData.nox_index) : color_inactive());
    set_dot_color(objects.dot_nox, gas_warmup ? nox_col : alert_color_for_mode(nox_col));

    const bool hcho_available = currentData.hcho_valid;
    if (objects.label_hcho_title) {
        safe_label_set_text(objects.label_hcho_title, hcho_available ? UiText::kLabelHcho : UiText::kLabelAqi);
    }
    if (objects.label_hcho_unit) {
        safe_label_set_text(objects.label_hcho_unit, hcho_available ? UiText::kUnitPpb : UiText::kUnitIndex);
    }
    if (objects.label_hcho_value) {
        if (hcho_available) {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(lroundf(currentData.hcho)));
        } else {
            snprintf(buf, sizeof(buf), "%d", aq.score);
        }
        safe_label_set_text(objects.label_hcho_value, buf);
    }
    if (objects.dot_hcho) {
        lv_color_t hcho_col = hcho_available ? getHCHOColor(currentData.hcho, true) : aq.color;
        set_dot_color(objects.dot_hcho, alert_color_for_mode(hcho_col));
    }

    if (currentData.pressure_valid) {
        snprintf(buf, sizeof(buf), "%.0f", currentData.pressure);
    } else {
        strcpy(buf, UiText::kValueMissing);
    }
    safe_label_set_text(objects.label_pressure_value, buf);

    if (currentData.pressure_delta_3h_valid) {
        if (currentData.pressure_delta_3h > 0.05f) {
            snprintf(buf, sizeof(buf), "+%.1f", currentData.pressure_delta_3h);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", currentData.pressure_delta_3h);
        }
    } else {
        strcpy(buf, UiText::kValueMissingShort);
    }
    safe_label_set_text(objects.label_delta_3h_value, buf);

    if (currentData.pressure_delta_24h_valid) {
        if (currentData.pressure_delta_24h > 0.05f) {
            snprintf(buf, sizeof(buf), "+%.1f", currentData.pressure_delta_24h);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", currentData.pressure_delta_24h);
        }
    } else {
        strcpy(buf, UiText::kValueMissingShort);
    }
    safe_label_set_text(objects.label_delta_24h_value, buf);

    lv_color_t delta_3h_color = night_mode
        ? color_card_border()
        : getPressureDeltaColor(currentData.pressure_delta_3h, currentData.pressure_delta_3h_valid, false);
    lv_color_t delta_24h_color = night_mode
        ? color_card_border()
        : getPressureDeltaColor(currentData.pressure_delta_24h, currentData.pressure_delta_24h_valid, true);
    set_chip_color(objects.chip_delta_3h, delta_3h_color);
    set_chip_color(objects.chip_delta_24h, delta_24h_color);
}
