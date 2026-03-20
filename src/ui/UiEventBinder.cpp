// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiEventBinder.h"

#include <stddef.h>

#include "ui/UiController.h"
#include "ui/BacklightManager.h"
#include "ui/ThemeManager.h"
#include "modules/NetworkManager.h"
#include "ui/ui.h"

lv_obj_t *UiEventBinder::screenRootById(int screen_id) {
    switch (screen_id) {
        case SCREEN_ID_PAGE_BOOT_LOGO:
            return objects.page_boot_logo;
        case SCREEN_ID_PAGE_BOOT_DIAG:
            return objects.page_boot_diag;
        case SCREEN_ID_PAGE_MAIN_PRO:
            return objects.page_main_pro;
        case SCREEN_ID_PAGE_SETTINGS:
            return objects.page_settings;
        case SCREEN_ID_PAGE_WIFI:
            return objects.page_wifi;
        case SCREEN_ID_PAGE_THEME:
            return objects.page_theme;
        case SCREEN_ID_PAGE_CLOCK:
            return objects.page_clock;
        case SCREEN_ID_PAGE_CO2_CALIB:
            return objects.page_co2_calib;
        case SCREEN_ID_PAGE_AUTO_NIGHT_MODE:
            return objects.page_auto_night_mode;
        case SCREEN_ID_PAGE_BACKLIGHT:
            return objects.page_backlight;
        case SCREEN_ID_PAGE_MQTT:
            return objects.page_mqtt;
        case SCREEN_ID_PAGE_SENSORS_INFO:
            return objects.page_sensors_info;
        case SCREEN_ID_PAGE_DAC_SETTINGS:
            return objects.page_dac_settings;
        case SCREEN_ID_PAGE_FW_UPDATE:
            return objects.page_fw_update;
        case SCREEN_ID_PAGE_DIAG:
            return objects.page_diag;
        default:
            return nullptr;
    }
}

bool UiEventBinder::objectBelongsToScreen(lv_obj_t *obj, lv_obj_t *screen_root) {
    if (!obj || !screen_root) {
        return false;
    }
    if (obj == screen_root) {
        return true;
    }
    return lv_obj_get_screen(obj) == screen_root;
}

void UiEventBinder::bindAvailableEvents(UiController &owner, int screen_id) {
    lv_obj_t *screen_root = screenRootById(screen_id);
    if (!screen_root) {
        return;
    }

    struct EventBinding {
        lv_obj_t *obj;
        lv_event_cb_t cb;
        lv_event_code_t code;
    };

    const EventBinding click_bindings[] = {
        {objects.btn_settings_1, UiController::on_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_back, UiController::on_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_about, UiController::on_about_event_cb, LV_EVENT_CLICKED},
        {objects.btn_about_back, UiController::on_about_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_web_page, UiController::on_web_page_event_cb, LV_EVENT_CLICKED},
        {objects.btn_web_page_back, UiController::on_web_page_back_event_cb, LV_EVENT_CLICKED},
        {objects.card_temp_pro, UiController::on_card_temp_event_cb, LV_EVENT_CLICKED},
        {objects.card_voc_pro, UiController::on_card_voc_event_cb, LV_EVENT_CLICKED},
        {objects.card_nox_pro, UiController::on_card_nox_event_cb, LV_EVENT_CLICKED},
        {objects.card_hcho_pro, UiController::on_card_hcho_event_cb, LV_EVENT_CLICKED},
        {objects.card_co2_pro, UiController::on_card_co2_event_cb, LV_EVENT_CLICKED},
        {objects.card_hum_pro, UiController::on_card_hum_event_cb, LV_EVENT_CLICKED},
        {objects.card_hum_2, UiController::on_dp_info_event_cb, LV_EVENT_CLICKED},
        {objects.card_pm05_pro, UiController::on_card_pm05_event_cb, LV_EVENT_CLICKED},
        {objects.card_pm25_pro, UiController::on_card_pm25_event_cb, LV_EVENT_CLICKED},
        {objects.card_pm10_pro, UiController::on_card_pm10_event_cb, LV_EVENT_CLICKED},
        {objects.card_co_pro, UiController::on_card_co_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm25_info, UiController::on_pm25_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm4_info, UiController::on_pm4_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm10_info, UiController::on_pm10_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm1_info, UiController::on_pm1_info_event_cb, LV_EVENT_CLICKED},
        {objects.card_pressure_pro, UiController::on_card_pressure_event_cb, LV_EVENT_CLICKED},
        {objects.btn_back_1, UiController::on_sensors_info_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_rh_info, UiController::on_rh_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_ah_info, UiController::on_ah_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mr_info, UiController::on_mr_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dp_info, UiController::on_dp_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_info_graph, UiController::on_info_graph_event_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_range_1h, UiController::on_temp_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_range_3h, UiController::on_temp_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_range_24h, UiController::on_temp_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_rh_range_1h, UiController::on_rh_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_rh_range_3h, UiController::on_rh_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_rh_range_24h, UiController::on_rh_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_voc_range_1h, UiController::on_voc_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_voc_range_3h, UiController::on_voc_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_voc_range_24h, UiController::on_voc_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_nox_range_1h, UiController::on_nox_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_nox_range_3h, UiController::on_nox_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_nox_range_24h, UiController::on_nox_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_hcho_range_1h, UiController::on_hcho_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_hcho_range_3h, UiController::on_hcho_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_hcho_range_24h, UiController::on_hcho_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_range_1h, UiController::on_co2_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_range_3h, UiController::on_co2_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_range_24h, UiController::on_co2_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm05_range_1h, UiController::on_pm05_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm05_range_3h, UiController::on_pm05_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm05_range_24h, UiController::on_pm05_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm25_4_range_1h, UiController::on_pm25_4_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm25_4_range_3h, UiController::on_pm25_4_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm25_4_range_24h, UiController::on_pm25_4_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm1_10_range_1h, UiController::on_pm1_10_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm1_10_range_3h, UiController::on_pm1_10_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pm1_10_range_24h, UiController::on_pm1_10_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co_range_1h, UiController::on_co_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co_range_3h, UiController::on_co_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co_range_24h, UiController::on_co_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pressure_range_1h, UiController::on_pressure_range_1h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pressure_range_3h, UiController::on_pressure_range_3h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_pressure_range_24h, UiController::on_pressure_range_24h_event_cb, LV_EVENT_CLICKED},
        {objects.btn_info_graph, UiController::on_info_graph_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_temp_range_1h, UiController::on_temp_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_temp_range_3h, UiController::on_temp_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_temp_range_24h, UiController::on_temp_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_rh_range_1h, UiController::on_rh_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_rh_range_3h, UiController::on_rh_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_rh_range_24h, UiController::on_rh_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_voc_range_1h, UiController::on_voc_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_voc_range_3h, UiController::on_voc_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_voc_range_24h, UiController::on_voc_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_nox_range_1h, UiController::on_nox_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_nox_range_3h, UiController::on_nox_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_nox_range_24h, UiController::on_nox_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_hcho_range_1h, UiController::on_hcho_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_hcho_range_3h, UiController::on_hcho_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_hcho_range_24h, UiController::on_hcho_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co2_range_1h, UiController::on_co2_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co2_range_3h, UiController::on_co2_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co2_range_24h, UiController::on_co2_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm05_range_1h, UiController::on_pm05_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm05_range_3h, UiController::on_pm05_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm05_range_24h, UiController::on_pm05_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm25_4_range_1h, UiController::on_pm25_4_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm25_4_range_3h, UiController::on_pm25_4_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm25_4_range_24h, UiController::on_pm25_4_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm1_10_range_1h, UiController::on_pm1_10_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm1_10_range_3h, UiController::on_pm1_10_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pm1_10_range_24h, UiController::on_pm1_10_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co_range_1h, UiController::on_co_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co_range_3h, UiController::on_co_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_co_range_24h, UiController::on_co_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pressure_range_1h, UiController::on_pressure_range_1h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pressure_range_3h, UiController::on_pressure_range_3h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_pressure_range_24h, UiController::on_pressure_range_24h_event_cb, LV_EVENT_SHORT_CLICKED},
        {objects.btn_3h_pressure_info, UiController::on_pressure_3h_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_24h_pressure_info, UiController::on_pressure_24h_info_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi, UiController::on_wifi_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_back, UiController::on_wifi_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt, UiController::on_mqtt_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt_back, UiController::on_mqtt_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt_reconnect, UiController::on_mqtt_reconnect_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_reconnect, UiController::on_wifi_reconnect_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_start_ap, UiController::on_wifi_start_ap_event_cb, LV_EVENT_CLICKED},
        {objects.btn_time_date, UiController::on_time_date_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_dim, UiController::on_auto_night_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_head_status_1, UiController::on_backlight_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_language, UiController::on_language_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_back, UiController::on_backlight_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_back, UiController::on_auto_night_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_hours_minus, UiController::on_auto_night_start_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_hours_plus, UiController::on_auto_night_start_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_minutes_minus, UiController::on_auto_night_start_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_minutes_plus, UiController::on_auto_night_start_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_hours_minus, UiController::on_auto_night_end_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_hours_plus, UiController::on_auto_night_end_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_minutes_minus, UiController::on_auto_night_end_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_minutes_plus, UiController::on_auto_night_end_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_always_on, UiController::on_backlight_preset_always_on_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_30s, UiController::on_backlight_preset_30s_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_1m, UiController::on_backlight_preset_1m_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_hours_minus, UiController::on_backlight_sleep_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_hours_plus, UiController::on_backlight_sleep_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_minutes_minus, UiController::on_backlight_sleep_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_minutes_plus, UiController::on_backlight_sleep_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_hours_minus, UiController::on_backlight_wake_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_hours_plus, UiController::on_backlight_wake_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_minutes_minus, UiController::on_backlight_wake_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_minutes_plus, UiController::on_backlight_wake_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_restart, UiController::on_restart_event_cb, LV_EVENT_CLICKED},
        {objects.btn_factory_reset, UiController::on_factory_reset_event_cb, LV_EVENT_CLICKED},
        {objects.btn_voc_reset, UiController::on_voc_reset_event_cb, LV_EVENT_CLICKED},
        {objects.btn_confirm_ok, UiController::on_confirm_ok_event_cb, LV_EVENT_CLICKED},
        {objects.btn_confirm_cancel, UiController::on_confirm_cancel_event_cb, LV_EVENT_CLICKED},
        {objects.btn_datetime_back, UiController::on_datetime_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_datetime_apply, UiController::on_datetime_apply_event_cb, LV_EVENT_CLICKED},
        {objects.chip_rtc_status, UiController::on_rtc_status_event_cb, LV_EVENT_CLICKED},
        {objects.chip_rtc_detection_auto, UiController::on_rtc_detection_auto_event_cb, LV_EVENT_CLICKED},
        {objects.chip_rtc_detection_pcf8523, UiController::on_rtc_detection_pcf8523_event_cb, LV_EVENT_CLICKED},
        {objects.chip_rtc_detection_ds3231, UiController::on_rtc_detection_ds3231_event_cb, LV_EVENT_CLICKED},
        {objects.btn_tz_plus, UiController::on_tz_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_tz_minus, UiController::on_tz_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_hours_minus, UiController::on_set_time_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_hours_plus, UiController::on_set_time_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_minutes_minus, UiController::on_set_time_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_minutes_plus, UiController::on_set_time_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_day_minus, UiController::on_set_date_day_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_day_plus, UiController::on_set_date_day_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_month_minus, UiController::on_set_date_month_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_month_plus, UiController::on_set_date_month_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_year_minus, UiController::on_set_date_year_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_year_plus, UiController::on_set_date_year_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_forget, UiController::on_wifi_forget_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib, UiController::on_co2_calib_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib_back, UiController::on_co2_calib_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib_start, UiController::on_co2_calib_start_event_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_offset_minus, UiController::on_temp_offset_minus_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_offset_plus, UiController::on_temp_offset_plus_cb, LV_EVENT_CLICKED},
        {objects.btn_hum_offset_minus, UiController::on_hum_offset_minus_cb, LV_EVENT_CLICKED},
        {objects.btn_hum_offset_plus, UiController::on_hum_offset_plus_cb, LV_EVENT_CLICKED},
        {objects.btn_diag, UiController::on_diag_event_cb, LV_EVENT_CLICKED},
        {objects.btn_diag_back, UiController::on_diag_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_theme_color, UiController::on_theme_color_event_cb, LV_EVENT_CLICKED},
        {objects.btn_theme_back, UiController::on_theme_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_diag_continue, UiController::on_boot_diag_continue_cb, LV_EVENT_CLICKED},
        {objects.btn_diag_errors, UiController::on_boot_diag_errors_cb, LV_EVENT_CLICKED},
        {objects.btn_dac_settings, UiController::on_dac_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dac_settings_back, UiController::on_dac_settings_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dak_manual_start, UiController::on_dac_manual_start_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dak_manual_stop, UiController::on_dac_manual_stop_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dak_manual_auto, UiController::on_dac_manual_auto_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dak_auto_on_toggle, UiController::on_dac_auto_start_event_cb, LV_EVENT_CLICKED},
        {objects.btn_dak_manual_stop_1, UiController::on_dac_auto_stop_event_cb, LV_EVENT_CLICKED},
    };

    const EventBinding value_bindings[] = {
        {objects.btn_head_status, UiController::on_head_status_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_wifi_toggle, UiController::on_wifi_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_mqtt_toggle, UiController::on_mqtt_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_auto_night_toggle, UiController::on_auto_night_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_backlight_schedule_toggle, UiController::on_backlight_schedule_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_backlight_alarm_wake, UiController::on_backlight_alarm_wake_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_night_mode, UiController::on_night_mode_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_units, UiController::on_units_c_f_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_1224_toggle, UiController::on_time_format_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_led_indicators, UiController::on_led_indicators_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_alert_blink, UiController::on_alert_blink_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co2_calib_asc, UiController::on_co2_calib_asc_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_ntp_toggle, UiController::on_ntp_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm25_info, UiController::on_pm25_info_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm4_info, UiController::on_pm4_info_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm10_info, UiController::on_pm10_info_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm1_info, UiController::on_pm1_info_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_info_graph, UiController::on_info_graph_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_temp_range_1h, UiController::on_temp_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_temp_range_3h, UiController::on_temp_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_temp_range_24h, UiController::on_temp_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_rh_range_1h, UiController::on_rh_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_rh_range_3h, UiController::on_rh_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_rh_range_24h, UiController::on_rh_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_voc_range_1h, UiController::on_voc_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_voc_range_3h, UiController::on_voc_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_voc_range_24h, UiController::on_voc_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_nox_range_1h, UiController::on_nox_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_nox_range_3h, UiController::on_nox_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_nox_range_24h, UiController::on_nox_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_hcho_range_1h, UiController::on_hcho_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_hcho_range_3h, UiController::on_hcho_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_hcho_range_24h, UiController::on_hcho_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co2_range_1h, UiController::on_co2_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co2_range_3h, UiController::on_co2_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co2_range_24h, UiController::on_co2_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm05_range_1h, UiController::on_pm05_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm05_range_3h, UiController::on_pm05_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm05_range_24h, UiController::on_pm05_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm25_4_range_1h, UiController::on_pm25_4_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm25_4_range_3h, UiController::on_pm25_4_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm25_4_range_24h, UiController::on_pm25_4_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm1_10_range_1h, UiController::on_pm1_10_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm1_10_range_3h, UiController::on_pm1_10_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pm1_10_range_24h, UiController::on_pm1_10_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co_range_1h, UiController::on_co_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co_range_3h, UiController::on_co_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co_range_24h, UiController::on_co_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pressure_range_1h, UiController::on_pressure_range_1h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pressure_range_3h, UiController::on_pressure_range_3h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_pressure_range_24h, UiController::on_pressure_range_24h_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dac_manual_on, UiController::on_dac_manual_on_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dac_auto_on, UiController::on_dac_auto_on_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_1, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_2, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_3, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_4, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_5, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_6, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_7, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_8, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_9, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_toggle_10, UiController::on_dac_manual_level_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_10min, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_30min, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_1hour, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_2hours, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_4hours, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_dak_manual_timer_toggle_8hours, UiController::on_dac_manual_timer_event_cb, LV_EVENT_VALUE_CHANGED},
    };

    constexpr lv_coord_t kExtendedHitAreaPx = 16;
    lv_obj_t *extended_hit_buttons[] = {
        objects.btn_settings_1,
        objects.btn_back_1,
        objects.btn_mqtt_back,
        objects.btn_backlight_back,
        objects.btn_auto_night_back,
        objects.btn_co2_calib_back,
        objects.btn_datetime_back,
        objects.btn_theme_back,
        objects.btn_wifi_back,
        objects.btn_back,
        objects.btn_dac_settings_back,
    };

    for (lv_obj_t *btn : extended_hit_buttons) {
        if (!objectBelongsToScreen(btn, screen_root)) {
            continue;
        }
        lv_obj_set_ext_click_area(btn, kExtendedHitAreaPx);
    }

    auto bind_events = [screen_root](const EventBinding *bindings, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            const EventBinding &binding = bindings[i];
            if (!binding.obj || !binding.cb) {
                continue;
            }
            if (!objectBelongsToScreen(binding.obj, screen_root)) {
                continue;
            }
            lv_obj_remove_event_cb(binding.obj, binding.cb);
            lv_obj_add_event_cb(binding.obj, binding.cb, binding.code, nullptr);
        }
    };

    bind_events(click_bindings, sizeof(click_bindings) / sizeof(click_bindings[0]));
    bind_events(value_bindings, sizeof(value_bindings) / sizeof(value_bindings[0]));
}

void UiEventBinder::applyToggleStylesForAvailableObjects(UiController &owner, int screen_id) {
    lv_obj_t *screen_root = screenRootById(screen_id);
    if (!screen_root) {
        return;
    }

    lv_obj_t *toggle_buttons[] = {
        objects.btn_night_mode,
        objects.btn_auto_dim,
        objects.btn_head_status_1,
        objects.btn_wifi,
        objects.btn_mqtt,
        objects.btn_wifi_reconnect,
        objects.btn_wifi_start_ap,
        objects.btn_units,
        objects.btn_1224_toggle,
        objects.btn_led_indicators,
        objects.btn_alert_blink,
        objects.btn_co2_calib_asc,
        objects.btn_head_status,
        objects.btn_wifi_toggle,
        objects.btn_mqtt_toggle,
        objects.btn_ntp_toggle,
        objects.btn_backlight_schedule_toggle,
        objects.btn_backlight_alarm_wake,
        objects.btn_backlight_always_on,
        objects.btn_backlight_30s,
        objects.btn_backlight_1m,
        objects.btn_auto_night_toggle,
        objects.btn_rh_info,
        objects.btn_ah_info,
        objects.btn_info_graph,
        objects.btn_temp_range_1h,
        objects.btn_temp_range_3h,
        objects.btn_temp_range_24h,
        objects.btn_rh_range_1h,
        objects.btn_rh_range_3h,
        objects.btn_rh_range_24h,
        objects.btn_voc_range_1h,
        objects.btn_voc_range_3h,
        objects.btn_voc_range_24h,
        objects.btn_nox_range_1h,
        objects.btn_nox_range_3h,
        objects.btn_nox_range_24h,
        objects.btn_hcho_range_1h,
        objects.btn_hcho_range_3h,
        objects.btn_hcho_range_24h,
        objects.btn_co2_range_1h,
        objects.btn_co2_range_3h,
        objects.btn_co2_range_24h,
        objects.btn_pm05_range_1h,
        objects.btn_pm05_range_3h,
        objects.btn_pm05_range_24h,
        objects.btn_pm25_4_range_1h,
        objects.btn_pm25_4_range_3h,
        objects.btn_pm25_4_range_24h,
        objects.btn_pm1_10_range_1h,
        objects.btn_pm1_10_range_3h,
        objects.btn_pm1_10_range_24h,
        objects.btn_co_range_1h,
        objects.btn_co_range_3h,
        objects.btn_co_range_24h,
        objects.btn_pressure_range_1h,
        objects.btn_pressure_range_3h,
        objects.btn_pressure_range_24h,
        objects.btn_pm25_info,
        objects.btn_pm4_info,
        objects.btn_pm10_info,
        objects.btn_pm1_info,
        objects.btn_mr_info,
        objects.btn_dp_info,
        objects.btn_3h_pressure_info,
        objects.btn_24h_pressure_info,
        objects.btn_dac_auto_on,
        objects.btn_dac_manual_on,
        objects.btn_dak_manual_toggle_1,
        objects.btn_dak_manual_toggle_2,
        objects.btn_dak_manual_toggle_3,
        objects.btn_dak_manual_toggle_4,
        objects.btn_dak_manual_toggle_5,
        objects.btn_dak_manual_toggle_6,
        objects.btn_dak_manual_toggle_7,
        objects.btn_dak_manual_toggle_8,
        objects.btn_dak_manual_toggle_9,
        objects.btn_dak_manual_toggle_10,
        objects.btn_dak_manual_timer_toggle_10min,
        objects.btn_dak_manual_timer_toggle_30min,
        objects.btn_dak_manual_timer_toggle_1hour,
        objects.btn_dak_manual_timer_toggle_2hours,
        objects.btn_dak_manual_timer_toggle_4hours,
        objects.btn_dak_manual_timer_toggle_8hours,
        objects.chip_rtc_detection_auto,
        objects.chip_rtc_detection_pcf8523,
        objects.chip_rtc_detection_ds3231,
    };

    for (lv_obj_t *btn : toggle_buttons) {
        if (!objectBelongsToScreen(btn, screen_root)) {
            continue;
        }
        owner.apply_toggle_style(btn);
    }
}

void UiEventBinder::applyCheckedStatesForAvailableObjects(UiController &owner, int screen_id) {
    lv_obj_t *screen_root = screenRootById(screen_id);
    if (!screen_root) {
        return;
    }
    owner.refreshConnectivitySnapshot();

    auto set_checked = [screen_root](lv_obj_t *btn, bool checked) {
        if (!btn) {
            return;
        }
        if (!objectBelongsToScreen(btn, screen_root)) {
            return;
        }
        if (checked) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    };

    set_checked(objects.btn_head_status, owner.header_status_enabled);
    set_checked(objects.btn_head_status_1, owner.backlightManager.isScheduleEnabled());
    set_checked(objects.btn_backlight_alarm_wake, owner.backlightManager.isAlarmWakeEnabled());
    set_checked(objects.btn_wifi_start_ap,
                owner.connectivity_.wifi_enabled &&
                owner.connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG));
    set_checked(objects.btn_night_mode, owner.night_mode);
    set_checked(objects.btn_units, owner.temp_units_c);
    set_checked(objects.btn_1224_toggle, owner.time_format_24h_);
    set_checked(objects.btn_led_indicators, owner.led_indicators_enabled);
    set_checked(objects.btn_alert_blink, owner.alert_blink_enabled);
    set_checked(objects.btn_co2_calib_asc, owner.co2_asc_enabled);
    set_checked(objects.btn_rh_info, owner.info_sensor == UiController::INFO_RH);
    set_checked(objects.btn_ah_info, owner.info_sensor == UiController::INFO_AH);
    set_checked(objects.btn_mr_info, owner.info_sensor == UiController::INFO_MR);
    set_checked(objects.btn_dp_info, owner.info_sensor == UiController::INFO_DP);
    const bool pressure_info_selected =
        owner.info_sensor == UiController::INFO_PRESSURE_3H ||
        owner.info_sensor == UiController::INFO_PRESSURE_24H;
    const bool info_graph_checked =
        ((owner.info_sensor == UiController::INFO_TEMP) && owner.temp_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_RH) && owner.rh_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_VOC) && owner.voc_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_NOX) && owner.nox_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_HCHO) && owner.hcho_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_CO2) && owner.co2_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_PM05) && owner.pm05_graph_mode_) ||
        (((owner.info_sensor == UiController::INFO_PM25) || (owner.info_sensor == UiController::INFO_PM4)) && owner.pm25_4_graph_mode_) ||
        (((owner.info_sensor == UiController::INFO_PM1) || (owner.info_sensor == UiController::INFO_PM10)) && owner.pm1_10_graph_mode_) ||
        ((owner.info_sensor == UiController::INFO_CO) && owner.co_graph_mode_) ||
        (pressure_info_selected && owner.pressure_graph_mode_);
    set_checked(objects.btn_info_graph, info_graph_checked);
    set_checked(objects.btn_temp_range_1h, owner.temp_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_temp_range_3h, owner.temp_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_temp_range_24h, owner.temp_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_rh_range_1h, owner.rh_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_rh_range_3h, owner.rh_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_rh_range_24h, owner.rh_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_voc_range_1h, owner.voc_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_voc_range_3h, owner.voc_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_voc_range_24h, owner.voc_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_nox_range_1h, owner.nox_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_nox_range_3h, owner.nox_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_nox_range_24h, owner.nox_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_hcho_range_1h, owner.hcho_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_hcho_range_3h, owner.hcho_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_hcho_range_24h, owner.hcho_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_co2_range_1h, owner.co2_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_co2_range_3h, owner.co2_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_co2_range_24h, owner.co2_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pm05_range_1h, owner.pm05_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm05_range_3h, owner.pm05_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm05_range_24h, owner.pm05_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pm25_4_range_1h, owner.pm25_4_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm25_4_range_3h, owner.pm25_4_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm25_4_range_24h, owner.pm25_4_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pm1_10_range_1h, owner.pm1_10_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pm1_10_range_3h, owner.pm1_10_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pm1_10_range_24h, owner.pm1_10_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pm25_info, owner.info_sensor == UiController::INFO_PM25);
    set_checked(objects.btn_pm4_info, owner.info_sensor == UiController::INFO_PM4);
    set_checked(objects.btn_co_range_1h, owner.co_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_co_range_3h, owner.co_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_co_range_24h, owner.co_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_pressure_range_1h, owner.pressure_graph_range_ == UiController::TEMP_GRAPH_RANGE_1H);
    set_checked(objects.btn_pressure_range_3h, owner.pressure_graph_range_ == UiController::TEMP_GRAPH_RANGE_3H);
    set_checked(objects.btn_pressure_range_24h, owner.pressure_graph_range_ == UiController::TEMP_GRAPH_RANGE_24H);
    set_checked(objects.btn_3h_pressure_info, owner.info_sensor != UiController::INFO_PRESSURE_24H);
    set_checked(objects.btn_24h_pressure_info, owner.info_sensor == UiController::INFO_PRESSURE_24H);
}

void UiEventBinder::initThemeControlsIfAvailable(UiController &owner) {
    if (!objects.page_theme) {
        return;
    }

    if (!owner.theme_events_bound_) {
        owner.themeManager.registerEvents(UiController::apply_toggle_style_cb,
                                          UiController::on_theme_swatch_event_cb,
                                          UiController::on_theme_tab_event_cb);
        owner.theme_events_bound_ = true;
    }

    owner.themeManager.selectSwatchByCurrent();
    bool presets = owner.themeManager.isCurrentPreset();
    if (objects.btn_theme_presets) {
        if (presets) lv_obj_add_state(objects.btn_theme_presets, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_theme_presets, LV_STATE_CHECKED);
    }
    if (objects.btn_theme_custom) {
        if (presets) lv_obj_clear_state(objects.btn_theme_custom, LV_STATE_CHECKED);
        else lv_obj_add_state(objects.btn_theme_custom, LV_STATE_CHECKED);
    }
    owner.update_theme_custom_info(presets);
}
