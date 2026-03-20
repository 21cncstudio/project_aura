// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <stdio.h>

#include "core/AppVersion.h"
#include "ui/UiText.h"
#include "ui/ui.h"

void UiController::update_settings_texts() {
    if (objects.label_settings_title) safe_label_set_text(objects.label_settings_title, UiText::LabelSettingsTitle());
    if (objects.label_btn_back) safe_label_set_text(objects.label_btn_back, UiText::LabelSettingsBack());
    if (objects.label_temp_offset_title) safe_label_set_text(objects.label_temp_offset_title, UiText::LabelTempOffsetTitle());
    if (objects.label_hum_offset_title) safe_label_set_text(objects.label_hum_offset_title, UiText::LabelHumOffsetTitle());
    if (objects.label_btn_night_mode) safe_label_set_text(objects.label_btn_night_mode, UiText::LabelNightMode());
    if (objects.label_btn_units) safe_label_set_text(objects.label_btn_units, "UNITS");
    if (objects.label_btn_head_status) safe_label_set_text(objects.label_btn_head_status, UiText::LabelHeadStatus());
    if (objects.label_btn_wifi) safe_label_set_text(objects.label_btn_wifi, UiText::LabelWifi());
    if (objects.label_btn_time_date) safe_label_set_text(objects.label_btn_time_date, UiText::LabelTimeDate());
    if (objects.label_btn_theme_color) safe_label_set_text(objects.label_btn_theme_color, UiText::LabelThemeColor());
    if (objects.label_btn_mqtt) safe_label_set_text(objects.label_btn_mqtt, UiText::LabelMqtt());
    if (objects.label_btn_auto_dim) safe_label_set_text(objects.label_btn_auto_dim, UiText::LabelAutoNight());
    if (objects.label_btn_restart) safe_label_set_text(objects.label_btn_restart, UiText::LabelRestart());
    if (objects.label_btn_factory_reset) safe_label_set_text(objects.label_btn_factory_reset, UiText::LabelFactoryReset());
    if (objects.label_btn_co2_calib) safe_label_set_text(objects.label_btn_co2_calib, UiText::LabelCo2Calibration());
    if (objects.label_btn_about) safe_label_set_text(objects.label_btn_about, UiText::LabelAbout());
    if (objects.label_btn_web_page) safe_label_set_text(objects.label_btn_web_page, "WEB\nPAGE");
    if (objects.label_dac_settings) safe_label_set_text(objects.label_dac_settings, UiText::LabelDacSettings());
    if (objects.label_dac_settings_title) safe_label_set_text(objects.label_dac_settings_title, UiText::LabelDacSettingsTitle());
    if (objects.label_btn_about_back) {
        const char *back_label = UiText::LabelSettingsBack();
        if (ui_language == Config::Language::EN) {
            back_label = "BACK";
        }
        safe_label_set_text(objects.label_btn_about_back, back_label);
    }
    if (objects.label_btn_web_page_back) {
        const char *back_label = UiText::LabelSettingsBack();
        if (ui_language == Config::Language::EN) {
            back_label = "BACK";
        }
        safe_label_set_text(objects.label_btn_web_page_back, back_label);
    }
    if (objects.container_about_text) {
        char about_text[256];
        snprintf(about_text, sizeof(about_text),
                 "Project Aura\nVersion: v%s\n(c) Volodymyr Papush (21CNCStudio)\nOpen-source firmware (GPL-3.0-or-later)\n21cncstudio.com",
                 AppVersion::fullVersion());
        safe_label_set_text(objects.container_about_text, about_text);
    }
    update_web_page_panel();
    if (objects.label_btn_units_led_indicators) safe_label_set_text(objects.label_btn_units_led_indicators, UiText::LabelLedIndicators());
    if (objects.label_btn_alert_blink) safe_label_set_text(objects.label_btn_alert_blink, UiText::LabelAlertBlink());
    if (objects.label_voc_reset) safe_label_set_text(objects.label_voc_reset, UiText::LabelVocRelearn());
    if (objects.label_btn_head_status_1) safe_label_set_text(objects.label_btn_head_status_1, UiText::LabelBacklight());
}

void UiController::update_main_texts() {
    if (objects.label_status_title_1) safe_label_set_text(objects.label_status_title_1, UiText::LabelStatusTitle());
    if (objects.label_btn_settings_1) safe_label_set_text(objects.label_btn_settings_1, UiText::LabelSettingsButton());
    if (objects.label_temp_title_1) safe_label_set_text(objects.label_temp_title_1, UiText::LabelTemperatureTitle());
    if (objects.label_pressure_title_1) safe_label_set_text(objects.label_pressure_title_1, UiText::LabelPressureTitle());
    if (objects.label_time_title_2) safe_label_set_text(objects.label_time_title_2, UiText::LabelTimeCard());
    if (objects.label_voc_warmup_1) safe_label_set_text(objects.label_voc_warmup_1, UiText::LabelWarmup());
    if (objects.label_nox_warmup_1) safe_label_set_text(objects.label_nox_warmup_1, UiText::LabelWarmup());
    if (objects.label_co_warmup) safe_label_set_text(objects.label_co_warmup, UiText::LabelWarmup());
    if (objects.label_voc_unit_1) safe_label_set_text(objects.label_voc_unit_1, UiText::UnitIndex());
    if (objects.label_nox_unit_1) safe_label_set_text(objects.label_nox_unit_1, UiText::UnitIndex());
}

void UiController::update_sensor_info_texts() {
    if (objects.label_btn_back_1) safe_label_set_text_static(objects.label_btn_back_1, UiText::LabelSettingsBack());
    if (objects.label_temperature_text) safe_label_set_text_static(objects.label_temperature_text, UiText::InfoTempText());
    if (objects.label_temperature_excellent) safe_label_set_text_static(objects.label_temperature_excellent, UiText::InfoTempExcellent());
    if (objects.label_temperature_acceptable) safe_label_set_text_static(objects.label_temperature_acceptable, UiText::InfoTempAcceptable());
    if (objects.label_temperature_uncomfortable) safe_label_set_text_static(objects.label_temperature_uncomfortable, UiText::InfoTempUncomfortable());
    if (objects.label_temperature_poor) safe_label_set_text_static(objects.label_temperature_poor, UiText::InfoTempPoor());
    if (objects.label_voc_text) safe_label_set_text_static(objects.label_voc_text, UiText::InfoVocText());
    if (objects.label_voc_excellent) safe_label_set_text_static(objects.label_voc_excellent, UiText::InfoVocExcellent());
    if (objects.label_voc_acceptable) safe_label_set_text_static(objects.label_voc_acceptable, UiText::InfoVocAcceptable());
    if (objects.label_voc_uncomfortable) safe_label_set_text_static(objects.label_voc_uncomfortable, UiText::InfoVocUncomfortable());
    if (objects.label_voc_poor) safe_label_set_text_static(objects.label_voc_poor, UiText::InfoVocPoor());
    if (objects.label_nox_text) safe_label_set_text_static(objects.label_nox_text, UiText::InfoNoxText());
    if (objects.label_nox_excellent) safe_label_set_text_static(objects.label_nox_excellent, UiText::InfoNoxExcellent());
    if (objects.label_nox_acceptable) safe_label_set_text_static(objects.label_nox_acceptable, UiText::InfoNoxAcceptable());
    if (objects.label_nox_uncomfortable) safe_label_set_text_static(objects.label_nox_uncomfortable, UiText::InfoNoxUncomfortable());
    if (objects.label_nox_poor) safe_label_set_text_static(objects.label_nox_poor, UiText::InfoNoxPoor());
    if (objects.label_hcho_text) safe_label_set_text_static(objects.label_hcho_text, UiText::InfoHchoText());
    if (objects.label_hcho_excellent) safe_label_set_text_static(objects.label_hcho_excellent, UiText::InfoHchoExcellent());
    if (objects.label_hcho_acceptable) safe_label_set_text_static(objects.label_hcho_acceptable, UiText::InfoHchoAcceptable());
    if (objects.label_hcho_uncomfortable) safe_label_set_text_static(objects.label_hcho_uncomfortable, UiText::InfoHchoUncomfortable());
    if (objects.label_hcho_poor) safe_label_set_text_static(objects.label_hcho_poor, UiText::InfoHchoPoor());
    if (objects.label_co2_text) safe_label_set_text_static(objects.label_co2_text, UiText::InfoCo2Text());
    if (objects.label_co2_excellent) safe_label_set_text_static(objects.label_co2_excellent, UiText::InfoCo2Excellent());
    if (objects.label_co2_acceptable) safe_label_set_text_static(objects.label_co2_acceptable, UiText::InfoCo2Acceptable());
    if (objects.label_co2_uncomfortable) safe_label_set_text_static(objects.label_co2_uncomfortable, UiText::InfoCo2Uncomfortable());
    if (objects.label_co2_poor) safe_label_set_text_static(objects.label_co2_poor, UiText::InfoCo2Poor());
    if (objects.label_pm25_text) safe_label_set_text_static(objects.label_pm25_text, UiText::InfoPm25Text());
    if (objects.label_pm25_excellent) safe_label_set_text_static(objects.label_pm25_excellent, UiText::InfoPm25Excellent());
    if (objects.label_pm25_acceptable) safe_label_set_text_static(objects.label_pm25_acceptable, UiText::InfoPm25Acceptable());
    if (objects.label_pm25_uncomfortable) safe_label_set_text_static(objects.label_pm25_uncomfortable, UiText::InfoPm25Uncomfortable());
    if (objects.label_pm25_poor) safe_label_set_text_static(objects.label_pm25_poor, UiText::InfoPm25Poor());
    if (objects.label_pm4_text) safe_label_set_text_static(objects.label_pm4_text, UiText::InfoPm4Text());
    if (objects.label_pm4_excellent) safe_label_set_text_static(objects.label_pm4_excellent, UiText::InfoPm4Excellent());
    if (objects.label_pm4_acceptable) safe_label_set_text_static(objects.label_pm4_acceptable, UiText::InfoPm4Acceptable());
    if (objects.label_pm4_uncomfortable) safe_label_set_text_static(objects.label_pm4_uncomfortable, UiText::InfoPm4Uncomfortable());
    if (objects.label_pm4_poor) safe_label_set_text_static(objects.label_pm4_poor, UiText::InfoPm4Poor());
    if (objects.label_pm10_text) safe_label_set_text_static(objects.label_pm10_text, UiText::InfoPm10Text());
    if (objects.label_pm10_excellent) safe_label_set_text_static(objects.label_pm10_excellent, UiText::InfoPm10Excellent());
    if (objects.label_pm10_acceptable) safe_label_set_text_static(objects.label_pm10_acceptable, UiText::InfoPm10Acceptable());
    if (objects.label_pm10_uncomfortable) safe_label_set_text_static(objects.label_pm10_uncomfortable, UiText::InfoPm10Uncomfortable());
    if (objects.label_pm10_poor) safe_label_set_text_static(objects.label_pm10_poor, UiText::InfoPm10Poor());
    if (objects.label_pm1_text) safe_label_set_text_static(objects.label_pm1_text, UiText::InfoPm1Text());
    if (objects.label_pm1_excellent) safe_label_set_text_static(objects.label_pm1_excellent, UiText::InfoPm1Excellent());
    if (objects.label_pm1_acceptable) safe_label_set_text_static(objects.label_pm1_acceptable, UiText::InfoPm1Acceptable());
    if (objects.label_pm1_uncomfortable) safe_label_set_text_static(objects.label_pm1_uncomfortable, UiText::InfoPm1Uncomfortable());
    if (objects.label_pm1_poor) safe_label_set_text_static(objects.label_pm1_poor, UiText::InfoPm1Poor());
    if (objects.label_co_text) safe_label_set_text_static(objects.label_co_text, UiText::InfoCoText());
    if (objects.label_co_excellent) safe_label_set_text_static(objects.label_co_excellent, UiText::InfoCoExcellent());
    if (objects.label_co_acceptable) safe_label_set_text_static(objects.label_co_acceptable, UiText::InfoCoAcceptable());
    if (objects.label_co_uncomfortable) safe_label_set_text_static(objects.label_co_uncomfortable, UiText::InfoCoUncomfortable());
    if (objects.label_co_poor) safe_label_set_text_static(objects.label_co_poor, UiText::InfoCoPoor());
    if (objects.label_pm05_text) safe_label_set_text_static(objects.label_pm05_text, UiText::InfoPm05Text());
    if (objects.label_pm05_excellent) safe_label_set_text_static(objects.label_pm05_excellent, UiText::InfoPm05Excellent());
    if (objects.label_pm05_acceptable) safe_label_set_text_static(objects.label_pm05_acceptable, UiText::InfoPm05Acceptable());
    if (objects.label_pm05_uncomfortable) safe_label_set_text_static(objects.label_pm05_uncomfortable, UiText::InfoPm05Uncomfortable());
    if (objects.label_pm05_poor) safe_label_set_text_static(objects.label_pm05_poor, UiText::InfoPm05Poor());
    if (objects.label_3h_pressure_text) safe_label_set_text_static(objects.label_3h_pressure_text, UiText::InfoPressure3hText());
    if (objects.label_3h_pressure_excellent) safe_label_set_text_static(objects.label_3h_pressure_excellent, UiText::InfoPressure3hExcellent());
    if (objects.label_3h_pressure_acceptable) safe_label_set_text_static(objects.label_3h_pressure_acceptable, UiText::InfoPressure3hAcceptable());
    if (objects.label_3h_pressure_uncomfortable) safe_label_set_text_static(objects.label_3h_pressure_uncomfortable, UiText::InfoPressure3hUncomfortable());
    if (objects.label_3h_pressure_poor) safe_label_set_text_static(objects.label_3h_pressure_poor, UiText::InfoPressure3hPoor());
    if (objects.label_24h_pressure_text) safe_label_set_text_static(objects.label_24h_pressure_text, UiText::InfoPressure24hText());
    if (objects.label_24h_pressure_excellent) safe_label_set_text_static(objects.label_24h_pressure_excellent, UiText::InfoPressure24hExcellent());
    if (objects.label_24h_pressure_acceptable) safe_label_set_text_static(objects.label_24h_pressure_acceptable, UiText::InfoPressure24hAcceptable());
    if (objects.label_24h_pressure_uncomfortable) safe_label_set_text_static(objects.label_24h_pressure_uncomfortable, UiText::InfoPressure24hUncomfortable());
    if (objects.label_24h_pressure_poor) safe_label_set_text_static(objects.label_24h_pressure_poor, UiText::InfoPressure24hPoor());
    if (objects.label_rh_text) safe_label_set_text_static(objects.label_rh_text, UiText::InfoRhText());
    if (objects.label_rh_excellent) safe_label_set_text_static(objects.label_rh_excellent, UiText::InfoRhExcellent());
    if (objects.label_rh_acceptable) safe_label_set_text_static(objects.label_rh_acceptable, UiText::InfoRhAcceptable());
    if (objects.label_rh_uncomfortable) safe_label_set_text_static(objects.label_rh_uncomfortable, UiText::InfoRhUncomfortable());
    if (objects.label_rh_poor) safe_label_set_text_static(objects.label_rh_poor, UiText::InfoRhPoor());
    if (objects.label_ah_text) safe_label_set_text_static(objects.label_ah_text, UiText::InfoAhText());
    if (objects.label_ah_excellent) safe_label_set_text_static(objects.label_ah_excellent, UiText::InfoAhExcellent());
    if (objects.label_ah_acceptable) safe_label_set_text_static(objects.label_ah_acceptable, UiText::InfoAhAcceptable());
    if (objects.label_ah_uncomfortable) safe_label_set_text_static(objects.label_ah_uncomfortable, UiText::InfoAhUncomfortable());
    if (objects.label_ah_poor) safe_label_set_text_static(objects.label_ah_poor, UiText::InfoAhPoor());
    if (objects.label_rh_text_1) safe_label_set_text_static(objects.label_rh_text_1, UiText::InfoMrText());
    if (objects.label_rh_excellent_1) safe_label_set_text_static(objects.label_rh_excellent_1, UiText::InfoMrExcellent());
    if (objects.label_rh_acceptable_1) safe_label_set_text_static(objects.label_rh_acceptable_1, UiText::InfoMrAcceptable());
    if (objects.label_rh_uncomfortable_1) safe_label_set_text_static(objects.label_rh_uncomfortable_1, UiText::InfoMrUncomfortable());
    if (objects.label_rh_poor_1) safe_label_set_text_static(objects.label_rh_poor_1, UiText::InfoMrPoor());
    if (objects.label_dp_text_1) safe_label_set_text_static(objects.label_dp_text_1, UiText::InfoDpText());
    if (objects.label_dp_excellent_1) safe_label_set_text_static(objects.label_dp_excellent_1, UiText::InfoDpExcellent());
    if (objects.label_dp_acceptable_1) safe_label_set_text_static(objects.label_dp_acceptable_1, UiText::InfoDpAcceptable());
    if (objects.label_dp_uncomfortable_1) safe_label_set_text_static(objects.label_dp_uncomfortable_1, UiText::InfoDpUncomfortable());
    if (objects.label_dp_poor_1) safe_label_set_text_static(objects.label_dp_poor_1, UiText::InfoDpPoor());
}

void UiController::update_confirm_texts() {
    if (objects.label_btn_confirm_voc) safe_label_set_text(objects.label_btn_confirm_voc, UiText::LabelConfirmVocButton());
    if (objects.label_btn_confirm_restart) safe_label_set_text(objects.label_btn_confirm_restart, UiText::LabelConfirmRestartButton());
    if (objects.label_btn_confirm_reset) safe_label_set_text(objects.label_btn_confirm_reset, UiText::LabelConfirmResetButton());
    if (objects.label_btn_confirm_cancel) safe_label_set_text(objects.label_btn_confirm_cancel, UiText::LabelConfirmCancelButton());
    if (objects.label_confirm_title_voc) safe_label_set_text(objects.label_confirm_title_voc, UiText::LabelConfirmTitleVoc());
    if (objects.container_confirm_voc_text) safe_label_set_text(objects.container_confirm_voc_text, UiText::LabelConfirmTextVoc());
    if (objects.label_confirm_title_restart) safe_label_set_text(objects.label_confirm_title_restart, UiText::LabelConfirmTitleRestart());
    if (objects.container_confirm_restart_text) safe_label_set_text(objects.container_confirm_restart_text, UiText::LabelConfirmTextRestart());
    if (objects.label_confirm_title_reset) safe_label_set_text(objects.label_confirm_title_reset, UiText::LabelConfirmTitleReset());
    if (objects.container_confirm_reset_text) safe_label_set_text(objects.container_confirm_reset_text, UiText::LabelConfirmTextReset());
}

void UiController::update_theme_texts() {
    refreshConnectivitySnapshot();

    if (objects.label_theme_title) safe_label_set_text(objects.label_theme_title, UiText::LabelThemeTitle());
    if (objects.label_btn_theme_back) safe_label_set_text(objects.label_btn_theme_back, UiText::LabelSettingsBack());
    if (objects.label_btn_theme_custom) safe_label_set_text(objects.label_btn_theme_custom, UiText::LabelThemeCustom());
    if (objects.label_btn_theme_presets) safe_label_set_text(objects.label_btn_theme_presets, UiText::LabelThemePresets());
    if (objects.label_theme_preview_title) safe_label_set_text(objects.label_theme_preview_title, UiText::LabelThemeExample());
    if (objects.label_theme_custom_text) {
        String custom_info = UiText::LabelThemeCustomInfo();
        const String theme_url = connectivity_.theme_local_url;
        String ip_url = "http://<device-ip>/theme";
        if (connectivity_.wifi_connected && !connectivity_.sta_ip.isEmpty()) {
            ip_url = connectivity_.theme_sta_url;
        }
        custom_info.replace("{{LOCAL_URL}}", theme_url);
        custom_info.replace(UiText::ThemePortalUrl(), theme_url);
        custom_info.replace("http://aura.local/theme", theme_url);
        custom_info.replace("{{IP_URL}}", ip_url);
        custom_info.replace("http://<device-ip>/theme", ip_url);
        custom_info.replace("http://<device ip>/theme", ip_url);
        safe_label_set_text(objects.label_theme_custom_text, custom_info.c_str());
    }
    if (objects.label_theme_preview_hum_title) safe_label_set_text(objects.label_theme_preview_hum_title, UiText::LabelHumidityTitle());
}

void UiController::update_auto_night_texts() {
    if (objects.label_auto_night_title) safe_label_set_text(objects.label_auto_night_title, UiText::LabelAutoNightTitle());
    if (objects.label_btn_auto_night_back) safe_label_set_text(objects.label_btn_auto_night_back, UiText::LabelSettingsBack());
    if (objects.label_auto_night_hint) safe_label_set_text(objects.label_auto_night_hint, UiText::LabelAutoNightHint());
    if (objects.label_auto_night_start_title) safe_label_set_text(objects.label_auto_night_start_title, UiText::LabelAutoNightStartTitle());
    if (objects.label_auto_night_end_title) safe_label_set_text(objects.label_auto_night_end_title, UiText::LabelAutoNightEndTitle());
    if (objects.label_auto_night_start_hours) safe_label_set_text(objects.label_auto_night_start_hours, UiText::LabelSetTimeHours());
    if (objects.label_auto_night_start_minutes) safe_label_set_text(objects.label_auto_night_start_minutes, UiText::LabelSetTimeMinutes());
    if (objects.label_auto_night_end_hours) safe_label_set_text(objects.label_auto_night_end_hours, UiText::LabelSetTimeHours());
    if (objects.label_auto_night_end_minutes) safe_label_set_text(objects.label_auto_night_end_minutes, UiText::LabelSetTimeMinutes());
    if (objects.label_btn_auto_night_toggle) safe_label_set_text(objects.label_btn_auto_night_toggle, UiText::MqttToggleLabel());
}

void UiController::update_backlight_texts() {
    if (objects.label_backlight_title) safe_label_set_text(objects.label_backlight_title, UiText::LabelBacklightTitle());
    if (objects.label_backlight_hint) safe_label_set_text(objects.label_backlight_hint, UiText::LabelBacklightHint());
    if (objects.label_backlight_schedule_title) safe_label_set_text(objects.label_backlight_schedule_title, UiText::LabelBacklightScheduleTitle());
    if (objects.label_backlight_alarm_wake_title) safe_label_set_text(objects.label_backlight_alarm_wake_title, UiText::LabelBacklightAlarmWakeTitle());
    if (objects.label_backlight_presets_title) safe_label_set_text(objects.label_backlight_presets_title, UiText::LabelBacklightPresetsTitle());
    if (objects.label_backlight_sleep_title) safe_label_set_text(objects.label_backlight_sleep_title, UiText::LabelBacklightSleepTitle());
    if (objects.label_backlight_wake_title) safe_label_set_text(objects.label_backlight_wake_title, UiText::LabelBacklightWakeTitle());
    if (objects.label_backlight_sleep_hours) safe_label_set_text(objects.label_backlight_sleep_hours, UiText::LabelSetTimeHours());
    if (objects.label_backlight_sleep_minutes) safe_label_set_text(objects.label_backlight_sleep_minutes, UiText::LabelSetTimeMinutes());
    if (objects.label_backlight_wake_hours) safe_label_set_text(objects.label_backlight_wake_hours, UiText::LabelSetTimeHours());
    if (objects.label_backlight_wake_minutes) safe_label_set_text(objects.label_backlight_wake_minutes, UiText::LabelSetTimeMinutes());
    if (objects.label_btn_backlight_back) safe_label_set_text(objects.label_btn_backlight_back, UiText::LabelSettingsBack());
    if (objects.label_btn_backlight_schedule_toggle) safe_label_set_text(objects.label_btn_backlight_schedule_toggle, UiText::MqttToggleLabel());
    if (objects.label_btn_backlight_alarm_wake_toggle) safe_label_set_text(objects.label_btn_backlight_alarm_wake_toggle, UiText::LabelBacklightAlarmWakeToggle());
    if (objects.label_btn_backlight_always_on) safe_label_set_text(objects.label_btn_backlight_always_on, UiText::LabelBacklightAlwaysOn());
    if (objects.label_btn_backlight_30s) safe_label_set_text(objects.label_btn_backlight_30s, UiText::LabelBacklight30s());
    if (objects.label_btn_backlight_1m) safe_label_set_text(objects.label_btn_backlight_1m, UiText::LabelBacklight1m());
}

void UiController::update_co2_calib_texts() {
    if (objects.label_co2_calib_title) safe_label_set_text(objects.label_co2_calib_title, UiText::LabelCo2CalibTitle());
    if (objects.label_btn_co2_calib_back) safe_label_set_text(objects.label_btn_co2_calib_back, UiText::LabelSettingsBack());
    if (objects.label_btn_co2_calib_start) safe_label_set_text(objects.label_btn_co2_calib_start, UiText::LabelCo2CalibStart());
    if (objects.label_co2_calib_asc_text) safe_label_set_text(objects.label_co2_calib_asc_text, UiText::LabelCo2CalibAscInfo());
    if (objects.label_co2_calib_fresh_text) safe_label_set_text(objects.label_co2_calib_fresh_text, UiText::LabelCo2CalibFreshInfo());
}

void UiController::update_fw_update_texts() {
    if (objects.label_fw_update) safe_label_set_text(objects.label_fw_update, UiText::LabelFwUpdateScreen());
}

void UiController::update_boot_diag_texts() {
    if (objects.label_btn_diag_continue) safe_label_set_text(objects.label_btn_diag_continue, UiText::LabelBootTapToContinue());
    if (objects.lbl_diag_title) safe_label_set_text(objects.lbl_diag_title, UiText::LabelBootDiagTitle());
    if (objects.lbl_diag_system_title) safe_label_set_text(objects.lbl_diag_system_title, UiText::LabelBootDiagSystemTitle());
    if (objects.lbl_diag_sensors_title) safe_label_set_text(objects.lbl_diag_sensors_title, UiText::LabelBootDiagSensorsTitle());
    if (objects.lbl_diag_app_label) safe_label_set_text(objects.lbl_diag_app_label, UiText::LabelBootDiagAppLabel());
    if (objects.lbl_diag_mac_label) safe_label_set_text(objects.lbl_diag_mac_label, UiText::LabelBootDiagMacLabel());
    if (objects.lbl_diag_reason_label) safe_label_set_text(objects.lbl_diag_reason_label, UiText::LabelBootDiagResetLabel());
    if (objects.lbl_diag_heap_label) safe_label_set_text(objects.lbl_diag_heap_label, UiText::LabelBootDiagHeapLabel());
    if (objects.lbl_diag_storage_label) safe_label_set_text(objects.lbl_diag_storage_label, UiText::LabelBootDiagStorageLabel());
    if (objects.lbl_diag_i2c_label) safe_label_set_text(objects.lbl_diag_i2c_label, UiText::LabelBootDiagI2cLabel());
    if (objects.lbl_diag_touch_label) safe_label_set_text(objects.lbl_diag_touch_label, UiText::LabelBootDiagTouchLabel());
    if (objects.lbl_diag_sen_label) safe_label_set_text(objects.lbl_diag_sen_label, UiText::LabelBootDiagSenLabel());
    if (objects.lbl_diag_sfa_label) safe_label_set_text(objects.lbl_diag_sfa_label, UiText::LabelBootDiagSfaLabel());
    if (objects.lbl_diag_rtc_label) {
        char rtc_label[16];
        snprintf(rtc_label, sizeof(rtc_label), "%s:", timeManager.rtcLabel());
        safe_label_set_text(objects.lbl_diag_rtc_label, rtc_label);
    }
    if (objects.lbl_diag_error) safe_label_set_text(objects.lbl_diag_error, UiText::LabelBootDiagErrorsDetected());
    if (objects.label_btn_diag_errors) safe_label_set_text(objects.label_btn_diag_errors, UiText::LabelBootDiagShowErrors());
}

