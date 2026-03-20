// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <WiFi.h>
#include <math.h>
#include <esp_wifi.h>

#include "config/AppConfig.h"
#include "core/Logger.h"
#include "core/SafeRestart.h"
#include "web/WebRuntime.h"
#include "lvgl_v8_port.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/StorageManager.h"
#include "modules/TimeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/ThemeManager.h"
#include "ui/UiLocalization.h"
#include "ui/UiText.h"
#include "ui/ui.h"

using namespace Config;

namespace {

constexpr uint32_t kWifiActionFeedbackMs = 220;

bool persist_ui_config(StorageManager &storage, const char *what) {
    if (storage.saveConfig(true)) {
        return true;
    }
    storage.requestSave();
    LOGE("UI", "failed to persist %s", what ? what : "settings");
    return false;
}

void wifi_action_feedback_timer_cb(lv_timer_t *timer) {
    if (!timer) {
        return;
    }

    lv_obj_t *btn = static_cast<lv_obj_t *>(timer->user_data);
    if (btn && lv_obj_is_valid(btn)) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_obj_invalidate(btn);
    }
    lv_timer_del(timer);
}

void show_wifi_action_feedback(lv_obj_t *btn) {
    if (!btn || !lv_obj_is_valid(btn)) {
        return;
    }

    lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_invalidate(btn);
    lv_timer_t *timer = lv_timer_create(wifi_action_feedback_timer_cb, kWifiActionFeedbackMs, btn);
    if (timer) {
        lv_timer_set_repeat_count(timer, 1);
    } else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_obj_invalidate(btn);
    }
}

} // namespace

void UiController::on_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_settings_event(e); }
void UiController::on_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_back_event(e); }
void UiController::on_about_event_cb(lv_event_t *e) { if (instance_) instance_->on_about_event(e); }
void UiController::on_about_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_about_back_event(e); }
void UiController::on_web_page_event_cb(lv_event_t *e) { if (instance_) instance_->on_web_page_event(e); }
void UiController::on_web_page_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_web_page_back_event(e); }
void UiController::on_wifi_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_settings_event(e); }
void UiController::on_wifi_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_back_event(e); }
void UiController::on_mqtt_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_mqtt_settings_event(e); }
void UiController::on_mqtt_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_mqtt_back_event(e); }
void UiController::on_theme_color_event_cb(lv_event_t *e) { if (instance_) instance_->on_theme_color_event(e); }
void UiController::on_theme_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_theme_back_event(e); }
void UiController::on_theme_tab_event_cb(lv_event_t *e) { if (instance_) instance_->on_theme_tab_event(e); }
void UiController::on_theme_swatch_event_cb(lv_event_t *e) { if (instance_) instance_->on_theme_swatch_event(e); }
void UiController::on_wifi_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_toggle_event(e); }
void UiController::on_mqtt_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_mqtt_toggle_event(e); }
void UiController::on_mqtt_reconnect_event_cb(lv_event_t *e) { if (instance_) instance_->on_mqtt_reconnect_event(e); }
void UiController::on_wifi_reconnect_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_reconnect_event(e); }
void UiController::on_wifi_start_ap_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_start_ap_event(e); }
void UiController::on_wifi_forget_event_cb(lv_event_t *e) { if (instance_) instance_->on_wifi_forget_event(e); }
void UiController::on_head_status_event_cb(lv_event_t *e) { if (instance_) instance_->on_head_status_event(e); }
void UiController::on_auto_night_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_settings_event(e); }
void UiController::on_auto_night_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_back_event(e); }
void UiController::on_auto_night_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_toggle_event(e); }
void UiController::on_auto_night_start_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_start_hours_minus_event(e); }
void UiController::on_auto_night_start_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_start_hours_plus_event(e); }
void UiController::on_auto_night_start_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_start_minutes_minus_event(e); }
void UiController::on_auto_night_start_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_start_minutes_plus_event(e); }
void UiController::on_auto_night_end_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_end_hours_minus_event(e); }
void UiController::on_auto_night_end_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_end_hours_plus_event(e); }
void UiController::on_auto_night_end_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_end_minutes_minus_event(e); }
void UiController::on_auto_night_end_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_auto_night_end_minutes_plus_event(e); }
void UiController::on_confirm_ok_event_cb(lv_event_t *e) { if (instance_) instance_->on_confirm_ok_event(e); }
void UiController::on_confirm_cancel_event_cb(lv_event_t *e) { if (instance_) instance_->on_confirm_cancel_event(e); }
void UiController::on_night_mode_event_cb(lv_event_t *e) { if (instance_) instance_->on_night_mode_event(e); }
void UiController::on_units_c_f_event_cb(lv_event_t *e) { if (instance_) instance_->on_units_c_f_event(e); }
void UiController::on_time_format_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_time_format_toggle_event(e); }
void UiController::on_units_mdy_event_cb(lv_event_t *e) { if (instance_) instance_->on_units_mdy_event(e); }
void UiController::on_led_indicators_event_cb(lv_event_t *e) { if (instance_) instance_->on_led_indicators_event(e); }
void UiController::on_alert_blink_event_cb(lv_event_t *e) { if (instance_) instance_->on_alert_blink_event(e); }
void UiController::on_co2_calib_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_calib_event(e); }
void UiController::on_co2_calib_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_calib_back_event(e); }
void UiController::on_co2_calib_asc_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_calib_asc_event(e); }
void UiController::on_co2_calib_start_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_calib_start_event(e); }
void UiController::on_time_date_event_cb(lv_event_t *e) { if (instance_) instance_->on_time_date_event(e); }
void UiController::on_backlight_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_settings_event(e); }
void UiController::on_backlight_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_back_event(e); }
void UiController::on_backlight_schedule_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_schedule_toggle_event(e); }
void UiController::on_backlight_alarm_wake_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_alarm_wake_event(e); }
void UiController::on_backlight_preset_always_on_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_always_on_event(e); }
void UiController::on_backlight_preset_30s_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_30s_event(e); }
void UiController::on_backlight_preset_1m_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_1m_event(e); }
void UiController::on_backlight_sleep_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_hours_minus_event(e); }
void UiController::on_backlight_sleep_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_hours_plus_event(e); }
void UiController::on_backlight_sleep_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_minutes_minus_event(e); }
void UiController::on_backlight_sleep_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_minutes_plus_event(e); }
void UiController::on_backlight_wake_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_hours_minus_event(e); }
void UiController::on_backlight_wake_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_hours_plus_event(e); }
void UiController::on_backlight_wake_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_minutes_minus_event(e); }
void UiController::on_backlight_wake_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_minutes_plus_event(e); }
void UiController::on_language_event_cb(lv_event_t *e) { if (instance_) instance_->on_language_event(e); }
void UiController::on_datetime_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_datetime_back_event(e); }
void UiController::on_datetime_apply_event_cb(lv_event_t *e) { if (instance_) instance_->on_datetime_apply_event(e); }
void UiController::on_rtc_status_event_cb(lv_event_t *e) { if (instance_) instance_->on_rtc_status_event(e); }
void UiController::on_rtc_detection_auto_event_cb(lv_event_t *e) { if (instance_) instance_->on_rtc_detection_auto_event(e); }
void UiController::on_rtc_detection_pcf8523_event_cb(lv_event_t *e) { if (instance_) instance_->on_rtc_detection_pcf8523_event(e); }
void UiController::on_rtc_detection_ds3231_event_cb(lv_event_t *e) { if (instance_) instance_->on_rtc_detection_ds3231_event(e); }
void UiController::on_ntp_toggle_event_cb(lv_event_t *e) { if (instance_) instance_->on_ntp_toggle_event(e); }
void UiController::on_tz_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_tz_plus_event(e); }
void UiController::on_tz_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_tz_minus_event(e); }
void UiController::on_set_time_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_time_hours_minus_event(e); }
void UiController::on_set_time_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_time_hours_plus_event(e); }
void UiController::on_set_time_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_time_minutes_minus_event(e); }
void UiController::on_set_time_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_time_minutes_plus_event(e); }
void UiController::on_set_date_day_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_day_minus_event(e); }
void UiController::on_set_date_day_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_day_plus_event(e); }
void UiController::on_set_date_month_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_month_minus_event(e); }
void UiController::on_set_date_month_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_month_plus_event(e); }
void UiController::on_set_date_year_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_year_minus_event(e); }
void UiController::on_set_date_year_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_set_date_year_plus_event(e); }
void UiController::on_restart_event_cb(lv_event_t *e) { if (instance_) instance_->on_restart_event(e); }
void UiController::on_factory_reset_event_cb(lv_event_t *e) { if (instance_) instance_->on_factory_reset_event(e); }
void UiController::on_voc_reset_event_cb(lv_event_t *e) { if (instance_) instance_->on_voc_reset_event(e); }
void UiController::on_card_temp_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_temp_event(e); }
void UiController::on_card_voc_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_voc_event(e); }
void UiController::on_card_nox_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_nox_event(e); }
void UiController::on_card_hcho_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_hcho_event(e); }
void UiController::on_card_co2_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_co2_event(e); }
void UiController::on_card_hum_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_hum_event(e); }
void UiController::on_rh_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_rh_info_event(e); }
void UiController::on_ah_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_ah_info_event(e); }
void UiController::on_mr_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_mr_info_event(e); }
void UiController::on_dp_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_dp_info_event(e); }
void UiController::on_info_graph_event_cb(lv_event_t *e) { if (instance_) instance_->on_info_graph_event(e); }
void UiController::on_temp_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_temp_range_1h_event(e); }
void UiController::on_temp_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_temp_range_3h_event(e); }
void UiController::on_temp_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_temp_range_24h_event(e); }
void UiController::on_rh_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_rh_range_1h_event(e); }
void UiController::on_rh_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_rh_range_3h_event(e); }
void UiController::on_rh_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_rh_range_24h_event(e); }
void UiController::on_voc_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_voc_range_1h_event(e); }
void UiController::on_voc_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_voc_range_3h_event(e); }
void UiController::on_voc_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_voc_range_24h_event(e); }
void UiController::on_nox_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_nox_range_1h_event(e); }
void UiController::on_nox_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_nox_range_3h_event(e); }
void UiController::on_nox_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_nox_range_24h_event(e); }
void UiController::on_hcho_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_hcho_range_1h_event(e); }
void UiController::on_hcho_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_hcho_range_3h_event(e); }
void UiController::on_hcho_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_hcho_range_24h_event(e); }
void UiController::on_co2_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_range_1h_event(e); }
void UiController::on_co2_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_range_3h_event(e); }
void UiController::on_co2_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co2_range_24h_event(e); }
void UiController::on_pm05_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm05_range_1h_event(e); }
void UiController::on_pm05_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm05_range_3h_event(e); }
void UiController::on_pm05_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm05_range_24h_event(e); }
void UiController::on_pm25_4_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm25_4_range_1h_event(e); }
void UiController::on_pm25_4_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm25_4_range_3h_event(e); }
void UiController::on_pm25_4_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm25_4_range_24h_event(e); }
void UiController::on_pm1_10_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm1_10_range_1h_event(e); }
void UiController::on_pm1_10_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm1_10_range_3h_event(e); }
void UiController::on_pm1_10_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm1_10_range_24h_event(e); }
void UiController::on_co_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co_range_1h_event(e); }
void UiController::on_co_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co_range_3h_event(e); }
void UiController::on_co_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_co_range_24h_event(e); }
void UiController::on_pressure_range_1h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pressure_range_1h_event(e); }
void UiController::on_pressure_range_3h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pressure_range_3h_event(e); }
void UiController::on_pressure_range_24h_event_cb(lv_event_t *e) { if (instance_) instance_->on_pressure_range_24h_event(e); }
void UiController::on_pm25_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm25_info_event(e); }
void UiController::on_pm4_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm4_info_event(e); }
void UiController::on_pm10_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm10_info_event(e); }
void UiController::on_pm1_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pm1_info_event(e); }
void UiController::on_card_pm05_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_pm05_event(e); }
void UiController::on_card_pm25_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_pm25_event(e); }
void UiController::on_card_pm10_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_pm10_event(e); }
void UiController::on_card_co_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_co_event(e); }
void UiController::on_card_pressure_event_cb(lv_event_t *e) { if (instance_) instance_->on_card_pressure_event(e); }
void UiController::on_pressure_3h_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pressure_3h_info_event(e); }
void UiController::on_pressure_24h_info_event_cb(lv_event_t *e) { if (instance_) instance_->on_pressure_24h_info_event(e); }
void UiController::on_sensors_info_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_sensors_info_back_event(e); }
void UiController::on_temp_offset_minus_cb(lv_event_t *e) { if (instance_) instance_->on_temp_offset_minus(e); }
void UiController::on_temp_offset_plus_cb(lv_event_t *e) { if (instance_) instance_->on_temp_offset_plus(e); }
void UiController::on_hum_offset_minus_cb(lv_event_t *e) { if (instance_) instance_->on_hum_offset_minus(e); }
void UiController::on_hum_offset_plus_cb(lv_event_t *e) { if (instance_) instance_->on_hum_offset_plus(e); }
void UiController::on_diag_event_cb(lv_event_t *e) { if (instance_) instance_->on_diag_event(e); }
void UiController::on_diag_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_diag_back_event(e); }
void UiController::on_boot_diag_continue_cb(lv_event_t *e) { if (instance_) instance_->on_boot_diag_continue(e); }
void UiController::on_boot_diag_errors_cb(lv_event_t *e) { if (instance_) instance_->on_boot_diag_errors(e); }
void UiController::apply_toggle_style_cb(lv_obj_t *btn) { if (instance_) instance_->apply_toggle_style(btn); }
void UiController::mqtt_sync_with_wifi_cb() { if (instance_) instance_->mqtt_sync_with_wifi(); }

void UiController::on_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    LOGD("UI", "settings pressed");
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (current_screen_id == SCREEN_ID_PAGE_SETTINGS &&
        objects.container_about &&
        !lv_obj_has_flag(objects.container_about, LV_OBJ_FLAG_HIDDEN)) {
        LOGD("UI", "back pressed (close about)");
        lv_obj_add_flag(objects.container_about, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (current_screen_id == SCREEN_ID_PAGE_SETTINGS &&
        objects.container_web_page &&
        !lv_obj_has_flag(objects.container_web_page, LV_OBJ_FLAG_HIDDEN)) {
        LOGD("UI", "back pressed (close web page)");
        lv_obj_add_flag(objects.container_web_page, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    LOGD("UI", "back pressed");
    bool save_config = false;
    bool offsets_saved = false;
    bool language_saved = false;
    auto &cfg = storage.config();
    if (temp_offset_dirty) {
        cfg.temp_offset = temp_offset;
        temp_offset_saved = temp_offset;
        temp_offset_dirty = false;
        offsets_saved = true;
        save_config = true;
    }
    if (hum_offset_dirty) {
        cfg.hum_offset = hum_offset;
        hum_offset_saved = hum_offset;
        hum_offset_dirty = false;
        offsets_saved = true;
        save_config = true;
    }
    if (language_dirty) {
        cfg.language = ui_language;
        language_dirty = false;
        save_config = true;
        language_saved = true;
    }
    if (save_config) {
        const bool persisted = persist_ui_config(storage, "offset/language settings");
        if (offsets_saved) LOGI("UI", persisted ? "offsets saved" : "offsets queued for save");
        if (language_saved) LOGI("UI", persisted ? "language saved" : "language queued for save");
    }
    pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
}

void UiController::on_about_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    LOGD("UI", "about pressed");
    if (objects.container_about) {
        lv_obj_clear_flag(objects.container_about, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.container_about, LV_OBJ_FLAG_CLICKABLE);
    }
}

void UiController::on_about_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    LOGD("UI", "about back pressed");
    if (objects.container_about) {
        lv_obj_add_flag(objects.container_about, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiController::update_web_page_panel() {
    refreshConnectivitySnapshot();
    const bool ap_mode = connectivity_.wifi_enabled &&
                         connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG);
    const bool sta_mode = connectivity_.wifi_enabled &&
                          connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED);
    const bool off_mode = !ap_mode && !sta_mode;
    const String &local_url = connectivity_.dashboard_local_url;
    String ip_url = "http://<device-ip>/dashboard";
    if (sta_mode) {
        ip_url = connectivity_.dashboard_sta_url;
    }

    if (objects.container_web_page_text_ap) {
        safe_label_set_text(objects.container_web_page_text_ap, UiText::LabelWebPageHelpAp());
        set_visible(objects.container_web_page_text_ap, ap_mode);
    }

    if (objects.container_web_page_text_sta) {
        String sta_text = UiText::LabelWebPageHelpSta();
        sta_text.replace("{{LOCAL_URL}}", local_url);
        sta_text.replace("{{IP_URL}}", ip_url);
        safe_label_set_text(objects.container_web_page_text_sta, sta_text.c_str());
        set_visible(objects.container_web_page_text_sta, sta_mode);
    }

    if (objects.container_web_page_text_off) {
        safe_label_set_text(objects.container_web_page_text_off, UiText::LabelWebPageHelpOff());
        set_visible(objects.container_web_page_text_off, off_mode);
    }

    if (objects.web_page_qr) {
        String web_url;
        if (ap_mode) {
            web_url = "http://192.168.4.1/dashboard";
        } else if (sta_mode) {
            web_url = ip_url;
        }

        if (!web_url.isEmpty()) {
            lv_obj_clear_flag(objects.web_page_qr, LV_OBJ_FLAG_HIDDEN);
            update_qrcode_if_needed(objects.web_page_qr,
                                    web_url.c_str(),
                                    web_page_qr_cache_,
                                    sizeof(web_page_qr_cache_));
        } else {
            lv_obj_add_flag(objects.web_page_qr, LV_OBJ_FLAG_HIDDEN);
        }
    }

    web_page_panel_dirty = false;
}

void UiController::on_web_page_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    LOGD("UI", "web page pressed");
    if (objects.container_web_page) {
        lv_obj_clear_flag(objects.container_web_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.container_web_page, LV_OBJ_FLAG_CLICKABLE);
    }
    update_web_page_panel();
}

void UiController::on_web_page_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    LOGD("UI", "web page back pressed");
    if (objects.container_web_page) {
        lv_obj_add_flag(objects.container_web_page, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiController::on_language_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    UiLocalization::cycleLanguage(*this);
    update_ui();
    update_wifi_ui();
    update_mqtt_ui();
    update_datetime_ui();
}

void UiController::on_wifi_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    update_wifi_ui();
    pending_screen_id = SCREEN_ID_PAGE_WIFI;
}

void UiController::on_wifi_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    applyPendingWifiEnabled();
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_mqtt_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    setMqttScreenOpenState(true);
    pending_screen_id = SCREEN_ID_PAGE_MQTT;
}

void UiController::on_mqtt_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    setMqttScreenOpenState(false);
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_theme_color_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    bool has_unsaved = themeManager.hasUnsavedPreview();
    if (!has_unsaved) {
        themeManager.syncPreviewWithCurrent();
    }
    if (!has_unsaved) {
        themeManager.selectSwatchByCurrent();
    }
    bool presets = !has_unsaved && themeManager.isCurrentPreset();
    if (objects.btn_theme_presets) {
        if (presets) lv_obj_add_state(objects.btn_theme_presets, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_theme_presets, LV_STATE_CHECKED);
    }
    if (objects.btn_theme_custom) {
        if (presets) lv_obj_clear_state(objects.btn_theme_custom, LV_STATE_CHECKED);
        else lv_obj_add_state(objects.btn_theme_custom, LV_STATE_CHECKED);
    }
    update_theme_custom_info(presets);
    setThemeScreenOpenState(true);
    themeManager.setCustomTabSelected(!presets);
    pending_screen_id = SCREEN_ID_PAGE_THEME;
}

void UiController::on_theme_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (themeManager.hasUnsavedPreview()) {
        themeManager.applyPreviewAsCurrent(storage, night_mode, datetime_ui_dirty);
    }
    setThemeScreenOpenState(false);
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_theme_tab_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool presets = (btn == objects.btn_theme_presets);
    if (objects.btn_theme_presets) {
        if (presets) lv_obj_add_state(objects.btn_theme_presets, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_theme_presets, LV_STATE_CHECKED);
    }
    if (objects.btn_theme_custom) {
        if (presets) lv_obj_clear_state(objects.btn_theme_custom, LV_STATE_CHECKED);
        else lv_obj_add_state(objects.btn_theme_custom, LV_STATE_CHECKED);
    }
    update_theme_custom_info(presets);
    themeManager.setCustomTabSelected(!presets);
}

void UiController::on_theme_swatch_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ThemeSwatch *swatch = static_cast<ThemeSwatch *>(lv_event_get_user_data(e));
    if (!swatch) {
        return;
    }
    themeManager.applyPreviewFromSwatch(*swatch);
}

void UiController::on_wifi_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (wifi_toggle_syncing_) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    setWifiEnabledFromUi(enabled);
}

void UiController::on_mqtt_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (mqtt_toggle_syncing_) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    setMqttUserEnabledFromUi(enabled);
}

void UiController::on_mqtt_reconnect_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    requestMqttReconnectFromUi();
}

void UiController::on_wifi_reconnect_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    show_wifi_action_feedback(btn);
    requestWifiReconnectFromUi();
}

void UiController::on_wifi_start_ap_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    toggleWifiApModeFromUi();
}

void UiController::on_wifi_forget_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
    LOGI("UI", "WiFi credentials cleared");
    clearWifiCredentialsFromUi();
}

void UiController::on_head_status_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    header_status_enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    storage.config().header_status_enabled = header_status_enabled;
    if (!storage.saveConfig(true)) {
        storage.requestSave();
        LOGE("UI", "failed to persist header status setting");
    }
    data_dirty = true;
}

void UiController::on_auto_night_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
    nightModeManager.markUiDirty();
    pending_screen_id = SCREEN_ID_PAGE_AUTO_NIGHT_MODE;
}

void UiController::on_auto_night_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.savePrefs(storage);
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_auto_night_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (nightModeManager.isToggleSyncing()) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == nightModeManager.isAutoEnabled()) {
        return;
    }
    nightModeManager.setAutoEnabled(enabled);
    if (enabled) {
        apply_auto_night_now();
    } else if (night_mode) {
        // Auto-night is being disabled while forced night mode is active:
        // return to normal mode immediately and restore blink state.
        set_night_mode_state(false, true);
    }
    sync_night_mode_toggle_ui();
    sync_auto_dim_button_state();
    data_dirty = true;
}

void UiController::on_auto_night_start_hours_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustStartHour(-1);
    apply_auto_night_now();
}

void UiController::on_auto_night_start_hours_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustStartHour(1);
    apply_auto_night_now();
}

void UiController::on_auto_night_start_minutes_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustStartMinute(-1);
    apply_auto_night_now();
}

void UiController::on_auto_night_start_minutes_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustStartMinute(1);
    apply_auto_night_now();
}

void UiController::on_auto_night_end_hours_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustEndHour(-1);
    apply_auto_night_now();
}

void UiController::on_auto_night_end_hours_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustEndHour(1);
    apply_auto_night_now();
}

void UiController::on_auto_night_end_minutes_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustEndMinute(-1);
    apply_auto_night_now();
}

void UiController::on_auto_night_end_minutes_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    nightModeManager.adjustEndMinute(1);
    apply_auto_night_now();
}

void UiController::on_confirm_ok_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ConfirmAction action = confirm_action;
    confirm_hide();
    if (action == CONFIRM_VOC_RESET) {
        LOGI("UI", "VOC state reset requested");
        sensorManager.clearVocState(storage);
        currentData.voc_valid = false;
        currentData.nox_valid = false;
        data_dirty = true;
        if (!sensorManager.isOk()) {
            LOGW("UI", "SEN66 not ready for VOC reset");
            return;
        }
        if (!sensorManager.deviceReset()) {
            LOGW("UI", "SEN66 device reset failed");
            return;
        }
        sensorManager.scheduleRetry(SEN66_START_RETRY_MS);
        LOGI("UI", "SEN66 device reset done");
    } else if (action == CONFIRM_RESTART) {
        LOGW("UI", "restart requested");
        WebHandlersRequestRestart();
    } else if (action == CONFIRM_FACTORY_RESET) {
        LOGW("UI", "factory reset requested");
        storage.clearAll();
        WiFi.disconnect(true, true);
        WebHandlersRequestRestart();
    }
}

void UiController::on_confirm_cancel_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    confirm_hide();
}

void UiController::on_night_mode_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (nightModeManager.isAutoEnabled()) {
        sync_night_mode_toggle_ui();
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    set_night_mode_state(enabled, true);
}

void UiController::on_units_c_f_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool use_c = lv_obj_has_state(btn, LV_STATE_CHECKED);
    const bool use_mdy = !use_c;
    if (use_c == temp_units_c && use_mdy == date_units_mdy) {
        return;
    }
    temp_units_c = use_c;
    date_units_mdy = use_mdy;
    storage.config().units_c = temp_units_c;
    storage.config().units_mdy = date_units_mdy;
    persist_ui_config(storage, "unit system");
    clock_ui_dirty = true;
    update_clock_labels();
    update_ui();
}

void UiController::on_time_format_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    const bool use_24h = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (use_24h == time_format_24h_) {
        return;
    }
    time_format_24h_ = use_24h;
    storage.config().time_format_24h = time_format_24h_;
    persist_ui_config(storage, "time format");
    clock_ui_dirty = true;
    update_clock_labels();
}

void UiController::on_units_mdy_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool use_mdy = lv_obj_has_state(btn, LV_STATE_CHECKED);
    const bool use_c = !use_mdy;
    if (use_c == temp_units_c && use_mdy == date_units_mdy) {
        return;
    }
    date_units_mdy = use_mdy;
    temp_units_c = use_c;
    storage.config().units_mdy = date_units_mdy;
    storage.config().units_c = temp_units_c;
    persist_ui_config(storage, "unit system");
    clock_ui_dirty = true;
    update_clock_labels();
    update_ui();
}

void UiController::on_restart_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    confirm_show(CONFIRM_RESTART);
}

void UiController::on_factory_reset_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    confirm_show(CONFIRM_FACTORY_RESET);
}

void UiController::on_voc_reset_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    confirm_show(CONFIRM_VOC_RESET);
}

void UiController::on_card_temp_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_TEMP;
    restore_sensor_info_selection();
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_voc_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_VOC;
    restore_sensor_info_selection();
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_nox_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_NOX;
    restore_sensor_info_selection();
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_hcho_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_HCHO;
    restore_sensor_info_selection();
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_co2_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_CO2;
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
    update_sensor_info_ui();
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_hum_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_humidity_info(INFO_RH);
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_rh_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_humidity_info(INFO_RH);
}

void UiController::on_ah_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_humidity_info(INFO_AH);
}

void UiController::on_mr_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_humidity_info(INFO_MR);
}

void UiController::on_dp_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_humidity_info(INFO_DP);
    if (current_screen_id != SCREEN_ID_PAGE_SENSORS_INFO) {
        pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
    }
}

void UiController::on_info_graph_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI",
         "info/graph pressed, code=%d info_sensor=%d temp_mode=%d rh_mode=%d voc_mode=%d nox_mode=%d hcho_mode=%d co2_mode=%d pm05_mode=%d pm25_4_mode=%d pm_mode=%d co_mode=%d pressure_mode=%d",
         static_cast<int>(code),
         static_cast<int>(info_sensor),
         temp_graph_mode_ ? 1 : 0,
         rh_graph_mode_ ? 1 : 0,
         voc_graph_mode_ ? 1 : 0,
         nox_graph_mode_ ? 1 : 0,
         hcho_graph_mode_ ? 1 : 0,
         co2_graph_mode_ ? 1 : 0,
         pm05_graph_mode_ ? 1 : 0,
         pm25_4_graph_mode_ ? 1 : 0,
         pm1_10_graph_mode_ ? 1 : 0,
         co_graph_mode_ ? 1 : 0,
         pressure_graph_mode_ ? 1 : 0);

    const bool was_graph_mode =
        (info_sensor == INFO_TEMP) ? temp_graph_mode_ :
        (info_sensor == INFO_RH) ? rh_graph_mode_ :
        (info_sensor == INFO_VOC) ? voc_graph_mode_ :
        (info_sensor == INFO_NOX) ? nox_graph_mode_ :
        (info_sensor == INFO_HCHO) ? hcho_graph_mode_ :
        (info_sensor == INFO_CO2) ? co2_graph_mode_ :
        (info_sensor == INFO_PM05) ? pm05_graph_mode_ :
        ((info_sensor == INFO_PM25 || info_sensor == INFO_PM4) ? pm25_4_graph_mode_ :
         ((info_sensor == INFO_PM1 || info_sensor == INFO_PM10) ? pm1_10_graph_mode_ :
          ((info_sensor == INFO_CO) ? co_graph_mode_ :
           ((info_sensor == INFO_PRESSURE_3H || info_sensor == INFO_PRESSURE_24H) ? pressure_graph_mode_ : false))));

    if (info_sensor == INFO_TEMP) {
        set_temperature_info_mode(!temp_graph_mode_);
    } else if (info_sensor == INFO_RH) {
        set_rh_info_mode(!rh_graph_mode_);
    } else if (info_sensor == INFO_VOC) {
        set_voc_info_mode(!voc_graph_mode_);
    } else if (info_sensor == INFO_NOX) {
        set_nox_info_mode(!nox_graph_mode_);
    } else if (info_sensor == INFO_HCHO) {
        set_hcho_info_mode(!hcho_graph_mode_);
    } else if (info_sensor == INFO_CO2) {
        set_co2_info_mode(!co2_graph_mode_);
    } else if (info_sensor == INFO_PM05) {
        set_pm05_info_mode(!pm05_graph_mode_);
    } else if (info_sensor == INFO_PM25 || info_sensor == INFO_PM4) {
        set_pm25_4_info_mode(!pm25_4_graph_mode_);
    } else if (info_sensor == INFO_PM1 || info_sensor == INFO_PM10) {
        set_pm1_10_info_mode(!pm1_10_graph_mode_);
    } else if (info_sensor == INFO_CO) {
        set_co_info_mode(!co_graph_mode_);
    } else if (info_sensor == INFO_PRESSURE_3H || info_sensor == INFO_PRESSURE_24H) {
        set_pressure_info_mode(!pressure_graph_mode_);
    } else {
        if (rh_graph_mode_) {
            set_rh_info_mode(false);
        } else {
            sync_info_graph_button_state();
        }
        return;
    }

    const bool is_graph_mode =
        (info_sensor == INFO_TEMP) ? temp_graph_mode_ :
        (info_sensor == INFO_RH) ? rh_graph_mode_ :
        (info_sensor == INFO_VOC) ? voc_graph_mode_ :
        (info_sensor == INFO_NOX) ? nox_graph_mode_ :
        (info_sensor == INFO_HCHO) ? hcho_graph_mode_ :
        (info_sensor == INFO_CO2) ? co2_graph_mode_ :
        (info_sensor == INFO_PM05) ? pm05_graph_mode_ :
        ((info_sensor == INFO_PM25 || info_sensor == INFO_PM4) ? pm25_4_graph_mode_ :
         ((info_sensor == INFO_PM1 || info_sensor == INFO_PM10) ? pm1_10_graph_mode_ :
          ((info_sensor == INFO_CO) ? co_graph_mode_ :
           ((info_sensor == INFO_PRESSURE_3H || info_sensor == INFO_PRESSURE_24H) ? pressure_graph_mode_ : false))));

    if (is_graph_mode && !was_graph_mode) {
        invalidate_active_graph_refresh_cache();
        update_sensor_info_ui();
        return;
    }

    data_dirty = true;
}

void UiController::on_temp_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "temp range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_TEMP) {
        info_sensor = INFO_TEMP;
        restore_sensor_info_selection();
    }
    temp_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_temperature_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_temp_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "temp range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_TEMP) {
        info_sensor = INFO_TEMP;
        restore_sensor_info_selection();
    }
    temp_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_temperature_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_temp_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "temp range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_TEMP) {
        info_sensor = INFO_TEMP;
        restore_sensor_info_selection();
    }
    temp_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_temperature_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_rh_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "rh range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_RH) {
        select_humidity_info(INFO_RH);
    }
    rh_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_rh_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_rh_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "rh range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_RH) {
        select_humidity_info(INFO_RH);
    }
    rh_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_rh_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_rh_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "rh range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_RH) {
        select_humidity_info(INFO_RH);
    }
    rh_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_rh_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_voc_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "voc range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_VOC) {
        info_sensor = INFO_VOC;
        restore_sensor_info_selection();
    }
    voc_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_voc_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_voc_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "voc range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_VOC) {
        info_sensor = INFO_VOC;
        restore_sensor_info_selection();
    }
    voc_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_voc_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_voc_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "voc range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_VOC) {
        info_sensor = INFO_VOC;
        restore_sensor_info_selection();
    }
    voc_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_voc_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_nox_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "nox range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_NOX) {
        info_sensor = INFO_NOX;
        restore_sensor_info_selection();
    }
    nox_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_nox_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_nox_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "nox range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_NOX) {
        info_sensor = INFO_NOX;
        restore_sensor_info_selection();
    }
    nox_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_nox_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_nox_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "nox range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_NOX) {
        info_sensor = INFO_NOX;
        restore_sensor_info_selection();
    }
    nox_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_nox_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_hcho_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "hcho range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_HCHO) {
        info_sensor = INFO_HCHO;
        restore_sensor_info_selection();
    }
    hcho_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_hcho_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_hcho_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "hcho range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_HCHO) {
        info_sensor = INFO_HCHO;
        restore_sensor_info_selection();
    }
    hcho_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_hcho_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_hcho_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "hcho range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_HCHO) {
        info_sensor = INFO_HCHO;
        restore_sensor_info_selection();
    }
    hcho_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_hcho_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co2_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co2 range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO2) {
        info_sensor = INFO_CO2;
        restore_sensor_info_selection();
    }
    co2_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_co2_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co2_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co2 range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO2) {
        info_sensor = INFO_CO2;
        restore_sensor_info_selection();
    }
    co2_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_co2_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co2_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co2 range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO2) {
        info_sensor = INFO_CO2;
        restore_sensor_info_selection();
    }
    co2_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_co2_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm05_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm05 range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM05) {
        select_pm_info(INFO_PM05);
    }
    pm05_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_pm05_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm05_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm05 range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM05) {
        select_pm_info(INFO_PM05);
    }
    pm05_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_pm05_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm05_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm05 range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM05) {
        select_pm_info(INFO_PM05);
    }
    pm05_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_pm05_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm25_4_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm25_4 range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM25 && info_sensor != INFO_PM4) {
        select_pm_info(INFO_PM25);
    }
    pm25_4_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_pm25_4_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm25_4_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm25_4 range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM25 && info_sensor != INFO_PM4) {
        select_pm_info(INFO_PM25);
    }
    pm25_4_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_pm25_4_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm25_4_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm25_4 range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM25 && info_sensor != INFO_PM4) {
        select_pm_info(INFO_PM25);
    }
    pm25_4_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_pm25_4_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm1_10_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm1_10 range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM1 && info_sensor != INFO_PM10) {
        select_pm_info(INFO_PM10);
    }
    pm1_10_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_pm1_10_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm1_10_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm1_10 range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM1 && info_sensor != INFO_PM10) {
        select_pm_info(INFO_PM10);
    }
    pm1_10_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_pm1_10_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm1_10_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pm1_10 range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PM1 && info_sensor != INFO_PM10) {
        select_pm_info(INFO_PM10);
    }
    pm1_10_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_pm1_10_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO) {
        select_pm_info(INFO_CO);
    }
    co_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_co_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO) {
        select_pm_info(INFO_CO);
    }
    co_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_co_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_co_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "co range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_CO) {
        select_pm_info(INFO_CO);
    }
    co_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_co_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pressure_range_1h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pressure range 1h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PRESSURE_3H && info_sensor != INFO_PRESSURE_24H) {
        select_pressure_info(INFO_PRESSURE_3H);
    }
    pressure_graph_range_ = TEMP_GRAPH_RANGE_1H;
    set_pressure_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pressure_range_3h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pressure range 3h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PRESSURE_3H && info_sensor != INFO_PRESSURE_24H) {
        select_pressure_info(INFO_PRESSURE_3H);
    }
    pressure_graph_range_ = TEMP_GRAPH_RANGE_3H;
    set_pressure_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pressure_range_24h_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    LOGD("UI", "pressure range 24h pressed, code=%d", static_cast<int>(code));
    if (info_sensor != INFO_PRESSURE_3H && info_sensor != INFO_PRESSURE_24H) {
        select_pressure_info(INFO_PRESSURE_3H);
    }
    pressure_graph_range_ = TEMP_GRAPH_RANGE_24H;
    set_pressure_info_mode(true);
    update_sensor_info_ui();
}

void UiController::on_pm25_info_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    select_pm_info(INFO_PM25);
}

void UiController::on_pm4_info_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    select_pm_info(INFO_PM4);
}

void UiController::on_pm10_info_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    select_pm_info(INFO_PM10);
}

void UiController::on_pm1_info_event(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    select_pm_info(INFO_PM1);
}

void UiController::on_card_pm05_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pm_info(INFO_PM05);
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_pm25_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pm_info(INFO_PM25);
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_pm10_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pm_info(INFO_PM10);
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_co_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (currentData.co_sensor_present) {
        select_pm_info(INFO_CO);
    } else {
        select_pm_info(INFO_PM1);
    }
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_card_pressure_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pressure_info(INFO_PRESSURE_3H);
    pending_screen_id = SCREEN_ID_PAGE_SENSORS_INFO;
}

void UiController::on_pressure_3h_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pressure_info(INFO_PRESSURE_3H);
}

void UiController::on_pressure_24h_info_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_pressure_info(INFO_PRESSURE_24H);
}

void UiController::on_sensors_info_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    info_sensor = INFO_NONE;
    hide_all_sensor_info_containers();
    pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
}

void UiController::on_led_indicators_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == led_indicators_enabled) {
        return;
    }
    led_indicators_enabled = enabled;
    storage.config().led_indicators = led_indicators_enabled;
    persist_ui_config(storage, "LED indicators");
    update_led_indicators();
}

void UiController::on_co2_calib_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (objects.btn_co2_calib_asc) {
        if (co2_asc_enabled) {
            lv_obj_add_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
        }
    }
    pending_screen_id = SCREEN_ID_PAGE_CO2_CALIB;
}

void UiController::on_co2_calib_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_diag_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
    update_diag_log_ui();
    last_diag_log_update_ms = lv_tick_get();
    pending_screen_id = SCREEN_ID_PAGE_DIAG;
}

void UiController::on_diag_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_co2_calib_asc_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == co2_asc_enabled) {
        return;
    }
    co2_asc_enabled = enabled;
    storage.config().asc_enabled = co2_asc_enabled;
    persist_ui_config(storage, "CO2 ASC");
    if (sensorManager.isOk()) {
        sensorManager.setAscEnabled(co2_asc_enabled);
    }
    data_dirty = true;
}

void UiController::on_co2_calib_start_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!sensorManager.isOk()) {
        LOGW("UI", "SEN66 FRC requested but sensor not ready");
        return;
    }
    uint16_t correction = 0;
    sensorManager.calibrateFrc(SEN66_FRC_REF_PPM, currentData.pressure_valid, currentData.pressure,
                               correction);
}

void UiController::on_time_date_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    datetime_changed = false;
    datetime_ui_dirty = true;
    clock_ui_dirty = true;
    pending_screen_id = SCREEN_ID_PAGE_CLOCK;
}

void UiController::on_backlight_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
    backlightManager.markUiDirty();
    pending_screen_id = SCREEN_ID_PAGE_BACKLIGHT;
}

void UiController::on_backlight_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.savePrefs(storage);
    sync_backlight_settings_button_state();
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_backlight_schedule_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (backlightManager.isScheduleSyncing()) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    backlightManager.setScheduleEnabled(enabled);
    sync_backlight_settings_button_state();
}

void UiController::on_backlight_alarm_wake_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (backlightManager.isAlarmWakeSyncing()) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    backlightManager.setAlarmWakeEnabled(enabled);
}

void UiController::on_backlight_preset_always_on_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (backlightManager.isPresetSyncing()) {
        return;
    }
    backlightManager.setTimeoutMs(0);
}

void UiController::on_backlight_preset_30s_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (backlightManager.isPresetSyncing()) {
        return;
    }
    backlightManager.setTimeoutMs(BACKLIGHT_TIMEOUT_30S);
}

void UiController::on_backlight_preset_1m_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (backlightManager.isPresetSyncing()) {
        return;
    }
    backlightManager.setTimeoutMs(BACKLIGHT_TIMEOUT_1M);
}

void UiController::on_backlight_sleep_hours_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustSleepHour(-1);
}

void UiController::on_backlight_sleep_hours_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustSleepHour(1);
}

void UiController::on_backlight_sleep_minutes_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustSleepMinute(-1);
}

void UiController::on_backlight_sleep_minutes_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustSleepMinute(1);
}

void UiController::on_backlight_wake_hours_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustWakeHour(-1);
}

void UiController::on_backlight_wake_hours_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustWakeHour(1);
}

void UiController::on_backlight_wake_minutes_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustWakeMinute(-1);
}

void UiController::on_backlight_wake_minutes_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    backlightManager.adjustWakeMinute(1);
}

void UiController::on_datetime_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (rtc_detection_overlay_visible()) {
        if (rtc_detection_pending_mode_ == rtc_detection_saved_mode_) {
            close_rtc_detection_overlay();
            return;
        }
        if (!storage.saveRtcMode(rtc_detection_pending_mode_)) {
            datetime_ui_dirty = true;
            return;
        }
        LOGI("UI", "RTC mode set to %s, restart requested",
             TimeManager::rtcModeLabel(rtc_detection_pending_mode_));
        close_rtc_detection_overlay();
        WebHandlersRequestRestart();
    }
    if (datetime_changed && !timeManager.isManualLocked(millis())) {
        if (timeManager.setLocalTime(set_year, set_month, set_day, set_hour, set_minute)) {
            LOGI("UI", "datetime auto-applied");
            apply_auto_night_now();
            clock_ui_dirty = true;
            datetime_ui_dirty = true;
        }
    }
    datetime_changed = false;
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_rtc_status_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    open_rtc_detection_overlay();
}

void UiController::on_rtc_detection_auto_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_rtc_detection_pending_mode(Config::RtcMode::Auto);
}

void UiController::on_rtc_detection_pcf8523_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_rtc_detection_pending_mode(Config::RtcMode::Pcf8523);
}

void UiController::on_rtc_detection_ds3231_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_rtc_detection_pending_mode(Config::RtcMode::Ds3231);
}

void UiController::on_datetime_apply_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    if (!timeManager.setLocalTime(set_year, set_month, set_day, set_hour, set_minute)) {
        return;
    }
    apply_auto_night_now();
    clock_ui_dirty = true;
    datetime_ui_dirty = true;
    datetime_changed = false;
}

void UiController::on_ntp_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (ntp_toggle_syncing) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == timeManager.isNtpEnabledPref()) {
        return;
    }
    timeManager.setNtpEnabledPref(enabled);
    datetime_ui_dirty = true;
}

void UiController::on_tz_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    timeManager.adjustTimezone(1);
    timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    apply_auto_night_now();
    clock_ui_dirty = true;
    datetime_ui_dirty = true;
}

void UiController::on_tz_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    timeManager.adjustTimezone(-1);
    timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    apply_auto_night_now();
    clock_ui_dirty = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_time_hours_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_hour = (set_hour + 23) % 24;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_time_hours_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_hour = (set_hour + 1) % 24;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_time_minutes_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_minute = (set_minute + 59) % 60;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_time_minutes_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_minute = (set_minute + 1) % 60;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_day_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    set_day--;
    if (set_day < 1) {
        set_day = max_day;
    }
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_day_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    set_day++;
    if (set_day > max_day) {
        set_day = 1;
    }
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_month_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_month--;
    if (set_month < 1) {
        set_month = 12;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_month_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_month++;
    if (set_month > 12) {
        set_month = 1;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_year_minus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_year--;
    if (set_year < 2000) {
        set_year = 2099;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_set_date_year_plus_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (timeManager.isManualLocked(millis())) {
        return;
    }
    set_year++;
    if (set_year > 2099) {
        set_year = 2000;
    }
    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    datetime_changed = true;
    datetime_ui_dirty = true;
}

void UiController::on_alert_blink_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (alert_blink_syncing) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == alert_blink_enabled) {
        return;
    }
    alert_blink_enabled = enabled;
    storage.config().alert_blink = alert_blink_enabled;
    persist_ui_config(storage, "alert blink");
    if (night_mode) {
        night_blink_user_changed = true;
    }
    if (alert_blink_enabled) {
        blink_state = true;
        last_blink_ms = millis();
    }
    data_dirty = true;
}

void UiController::on_temp_offset_minus(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (temp_units_c) {
        temp_offset -= 0.1f;
    } else {
        // Keep C as storage, but edit in displayed F offset (0.2F ~= 0.1C step).
        float offset_f = temp_offset * 9.0f / 5.0f;
        offset_f -= 0.2f;
        offset_f = lroundf(offset_f * 10.0f) / 10.0f;
        temp_offset = offset_f * 5.0f / 9.0f;
    }
    temp_offset = lroundf(temp_offset * 10.0f) / 10.0f;
    if (temp_offset < -5.0f) {
        temp_offset = -5.0f;
    }
    temp_offset_dirty = true;
    temp_offset_ui_dirty = true;
    sensorManager.setOffsets(temp_offset, hum_offset);
}

void UiController::on_temp_offset_plus(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (temp_units_c) {
        temp_offset += 0.1f;
    } else {
        // Keep C as storage, but edit in displayed F offset (0.2F ~= 0.1C step).
        float offset_f = temp_offset * 9.0f / 5.0f;
        offset_f += 0.2f;
        offset_f = lroundf(offset_f * 10.0f) / 10.0f;
        temp_offset = offset_f * 5.0f / 9.0f;
    }
    temp_offset = lroundf(temp_offset * 10.0f) / 10.0f;
    if (temp_offset > 5.0f) {
        temp_offset = 5.0f;
    }
    temp_offset_dirty = true;
    temp_offset_ui_dirty = true;
    sensorManager.setOffsets(temp_offset, hum_offset);
}

void UiController::on_hum_offset_minus(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    hum_offset -= HUM_OFFSET_STEP;
    hum_offset = lroundf(hum_offset);
    if (hum_offset < HUM_OFFSET_MIN) {
        hum_offset = HUM_OFFSET_MIN;
    }
    hum_offset_dirty = true;
    hum_offset_ui_dirty = true;
    sensorManager.setOffsets(temp_offset, hum_offset);
}

void UiController::on_hum_offset_plus(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    hum_offset += HUM_OFFSET_STEP;
    hum_offset = lroundf(hum_offset);
    if (hum_offset > HUM_OFFSET_MAX) {
        hum_offset = HUM_OFFSET_MAX;
    }
    hum_offset_dirty = true;
    hum_offset_ui_dirty = true;
    sensorManager.setOffsets(temp_offset, hum_offset);
}

void UiController::on_boot_diag_continue(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (objects.container_diag_errors &&
        !lv_obj_has_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN);
        if (objects.label_diag_errors_text) {
            safe_label_set_text(objects.label_diag_errors_text, "");
        }
        return;
    }
    pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
    boot_diag_active = false;
    data_dirty = true;
}

void UiController::on_boot_diag_errors(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!objects.container_diag_errors) {
        return;
    }
    if (lv_obj_has_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN);
    }
}
