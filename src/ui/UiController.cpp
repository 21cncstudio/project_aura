// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include <WiFi.h>
#include <esp_heap_caps.h>

#include "lvgl_v8_port.h"
#include "ui/ui.h"
#include "ui/styles.h"
#include "ui/images.h"
#include "web/WebHandlers.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "core/BootState.h"
#include "modules/StorageManager.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"
#include "ui/ThemeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

using namespace Config;

namespace {

const char *reset_reason_to_string(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

bool is_crash_reset(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

void set_visible(lv_obj_t *obj, bool visible) {
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

enum StatusSeverity : uint8_t {
    STATUS_NONE = 0,
    STATUS_YELLOW = 1,
    STATUS_ORANGE = 2,
    STATUS_RED = 3,
};

enum StatusSensor : uint8_t {
    STATUS_SENSOR_NOX = 0,
    STATUS_SENSOR_HCHO,
    STATUS_SENSOR_PM25,
    STATUS_SENSOR_PM10,
    STATUS_SENSOR_VOC,
    STATUS_SENSOR_CO2,
    STATUS_SENSOR_TEMP,
    STATUS_SENSOR_HUM,
    STATUS_SENSOR_DP,
};

struct StatusMessage {
    const char *text;
    StatusSeverity severity;
    StatusSensor sensor;
};

constexpr uint32_t STATUS_ROTATE_MS = 5000;

float map_float_clamped(float value, float in_min, float in_max, float out_min, float out_max) {
    if (in_max <= in_min) return out_min;
    float v = value;
    if (v < in_min) v = in_min;
    if (v > in_max) v = in_max;
    return out_min + (out_max - out_min) * (v - in_min) / (in_max - in_min);
}

int score_from_thresholds(float value, float min_val, float t_good, float t_mod, float t_poor) {
    if (value <= t_good) {
        return static_cast<int>(lroundf(map_float_clamped(value, min_val, t_good, 0.0f, 25.0f)));
    }
    if (value <= t_mod) {
        return static_cast<int>(lroundf(map_float_clamped(value, t_good, t_mod, 25.0f, 50.0f)));
    }
    if (value <= t_poor) {
        return static_cast<int>(lroundf(map_float_clamped(value, t_mod, t_poor, 50.0f, 75.0f)));
    }
    float cap = t_poor * 1.5f;
    float score = map_float_clamped(value, t_poor, cap, 75.0f, 100.0f);
    if (score < 75.0f) score = 75.0f;
    if (score > 100.0f) score = 100.0f;
    return static_cast<int>(lroundf(score));
}

int score_from_voc(float value) {
    if (value <= 100.0f) {
        return static_cast<int>(lroundf(map_float_clamped(value, 0.0f, 100.0f, 0.0f, 25.0f)));
    }
    if (value <= 150.0f) {
        return static_cast<int>(lroundf(map_float_clamped(value, 100.0f, 150.0f, 25.0f, 50.0f)));
    }
    float score = map_float_clamped(value, 150.0f, 500.0f, 50.0f, 75.0f);
    if (score < 50.0f) score = 50.0f;
    if (score > 75.0f) score = 75.0f;
    return static_cast<int>(lroundf(score));
}

} // namespace

UiController *UiController::instance_ = nullptr;

UiController::UiController(const UiContext &context)
    : storage(context.storage),
      networkManager(context.networkManager),
      mqttManager(context.mqttManager),
      sensorManager(context.sensorManager),
      timeManager(context.timeManager),
      themeManager(context.themeManager),
      backlightManager(context.backlightManager),
      nightModeManager(context.nightModeManager),
      currentData(context.currentData),
      night_mode(context.night_mode),
      temp_units_c(context.temp_units_c),
      led_indicators_enabled(context.led_indicators_enabled),
      alert_blink_enabled(context.alert_blink_enabled),
      co2_asc_enabled(context.co2_asc_enabled),
      temp_offset(context.temp_offset),
      hum_offset(context.hum_offset) {
    instance_ = this;
}

void UiController::setLvglReady(bool ready) {
    lvgl_ready = ready;
}

void UiController::on_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_settings_event(e); }
void UiController::on_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_back_event(e); }
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
void UiController::on_backlight_preset_always_on_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_always_on_event(e); }
void UiController::on_backlight_preset_30s_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_30s_event(e); }
void UiController::on_backlight_preset_1m_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_1m_event(e); }
void UiController::on_backlight_preset_5m_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_preset_5m_event(e); }
void UiController::on_backlight_sleep_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_hours_minus_event(e); }
void UiController::on_backlight_sleep_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_hours_plus_event(e); }
void UiController::on_backlight_sleep_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_minutes_minus_event(e); }
void UiController::on_backlight_sleep_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_sleep_minutes_plus_event(e); }
void UiController::on_backlight_wake_hours_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_hours_minus_event(e); }
void UiController::on_backlight_wake_hours_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_hours_plus_event(e); }
void UiController::on_backlight_wake_minutes_minus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_minutes_minus_event(e); }
void UiController::on_backlight_wake_minutes_plus_event_cb(lv_event_t *e) { if (instance_) instance_->on_backlight_wake_minutes_plus_event(e); }
void UiController::on_datetime_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_datetime_back_event(e); }
void UiController::on_datetime_apply_event_cb(lv_event_t *e) { if (instance_) instance_->on_datetime_apply_event(e); }
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
void UiController::on_temp_offset_minus_cb(lv_event_t *e) { if (instance_) instance_->on_temp_offset_minus(e); }
void UiController::on_temp_offset_plus_cb(lv_event_t *e) { if (instance_) instance_->on_temp_offset_plus(e); }
void UiController::on_hum_offset_minus_cb(lv_event_t *e) { if (instance_) instance_->on_hum_offset_minus(e); }
void UiController::on_hum_offset_plus_cb(lv_event_t *e) { if (instance_) instance_->on_hum_offset_plus(e); }
void UiController::on_boot_diag_continue_cb(lv_event_t *e) { if (instance_) instance_->on_boot_diag_continue(e); }
void UiController::apply_toggle_style_cb(lv_obj_t *btn) { if (instance_) instance_->apply_toggle_style(btn); }
void UiController::mqtt_sync_with_wifi_cb() { if (instance_) instance_->mqtt_sync_with_wifi(); }

void UiController::begin() {
    instance_ = this;
    if (!lvgl_ready) {
        return;
    }
    lvgl_port_lock(-1);
    ui_init();
    themeManager.initAfterUi(storage, night_mode, datetime_ui_dirty);
    if (night_mode) {
        night_mode_on_enter();
    }
    init_ui_defaults();
    if (objects.label_boot_ver) {
        char version_text[24];
        snprintf(version_text, sizeof(version_text), "v%s", APP_VERSION);
        safe_label_set_text(objects.label_boot_ver, version_text);
    }
    current_screen_id = SCREEN_ID_PAGE_MAIN;
    pending_screen_id = SCREEN_ID_PAGE_MAIN;
    if (objects.btn_settings) {
        lv_obj_add_event_cb(objects.btn_settings, on_settings_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_back) {
        lv_obj_add_event_cb(objects.btn_back, on_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_wifi) {
        lv_obj_add_event_cb(objects.btn_wifi, on_wifi_settings_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_wifi_back) {
        lv_obj_add_event_cb(objects.btn_wifi_back, on_wifi_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_mqtt) {
        lv_obj_add_event_cb(objects.btn_mqtt, on_mqtt_settings_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_mqtt_back) {
        lv_obj_add_event_cb(objects.btn_mqtt_back, on_mqtt_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    apply_toggle_style(objects.btn_night_mode);
    apply_toggle_style(objects.btn_auto_dim);
    apply_toggle_style(objects.btn_wifi);
    apply_toggle_style(objects.btn_mqtt);
    apply_toggle_style(objects.btn_units_c_f);
    apply_toggle_style(objects.btn_led_indicators);
    apply_toggle_style(objects.btn_alert_blink);
    apply_toggle_style(objects.btn_co2_calib_asc);
    apply_toggle_style(objects.btn_head_status);
    apply_toggle_style(objects.btn_wifi_toggle);
    apply_toggle_style(objects.btn_ntp_toggle);
    apply_toggle_style(objects.btn_backlight_schedule_toggle);
    apply_toggle_style(objects.btn_backlight_always_on);
    apply_toggle_style(objects.btn_backlight_30s);
    apply_toggle_style(objects.btn_backlight_1m);
    apply_toggle_style(objects.btn_backlight_5m);
    apply_toggle_style(objects.btn_auto_night_toggle);
    if (objects.btn_head_status) {
        lv_obj_add_state(objects.btn_head_status, LV_STATE_CHECKED);
        lv_obj_add_event_cb(objects.btn_head_status, on_head_status_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_wifi_toggle) {
        lv_obj_add_event_cb(objects.btn_wifi_toggle, on_wifi_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_mqtt_toggle) {
        apply_toggle_style(objects.btn_mqtt_toggle);
        lv_obj_add_event_cb(objects.btn_mqtt_toggle, on_mqtt_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_mqtt_reconnect) {
        lv_obj_add_event_cb(objects.btn_mqtt_reconnect, on_mqtt_reconnect_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_wifi_reconnect) {
        lv_obj_add_event_cb(objects.btn_wifi_reconnect, on_wifi_reconnect_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_wifi_start_ap) {
        lv_obj_add_event_cb(objects.btn_wifi_start_ap, on_wifi_start_ap_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_time_date) {
        lv_obj_add_event_cb(objects.btn_time_date, on_time_date_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_dim) {
        lv_obj_add_event_cb(objects.btn_auto_dim, on_auto_night_settings_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_head_status_1) {
        lv_obj_add_event_cb(objects.btn_head_status_1, on_backlight_settings_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_back) {
        lv_obj_add_event_cb(objects.btn_backlight_back, on_backlight_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_back) {
        lv_obj_add_event_cb(objects.btn_auto_night_back, on_auto_night_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_toggle) {
        lv_obj_add_event_cb(objects.btn_auto_night_toggle, on_auto_night_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_auto_night_start_hours_minus) {
        lv_obj_add_event_cb(objects.btn_auto_night_start_hours_minus, on_auto_night_start_hours_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_start_hours_plus) {
        lv_obj_add_event_cb(objects.btn_auto_night_start_hours_plus, on_auto_night_start_hours_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_start_minutes_minus) {
        lv_obj_add_event_cb(objects.btn_auto_night_start_minutes_minus, on_auto_night_start_minutes_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_start_minutes_plus) {
        lv_obj_add_event_cb(objects.btn_auto_night_start_minutes_plus, on_auto_night_start_minutes_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_end_hours_minus) {
        lv_obj_add_event_cb(objects.btn_auto_night_end_hours_minus, on_auto_night_end_hours_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_end_hours_plus) {
        lv_obj_add_event_cb(objects.btn_auto_night_end_hours_plus, on_auto_night_end_hours_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_end_minutes_minus) {
        lv_obj_add_event_cb(objects.btn_auto_night_end_minutes_minus, on_auto_night_end_minutes_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_auto_night_end_minutes_plus) {
        lv_obj_add_event_cb(objects.btn_auto_night_end_minutes_plus, on_auto_night_end_minutes_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_schedule_toggle) {
        lv_obj_add_event_cb(objects.btn_backlight_schedule_toggle, on_backlight_schedule_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_backlight_always_on) {
        lv_obj_add_event_cb(objects.btn_backlight_always_on, on_backlight_preset_always_on_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_30s) {
        lv_obj_add_event_cb(objects.btn_backlight_30s, on_backlight_preset_30s_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_1m) {
        lv_obj_add_event_cb(objects.btn_backlight_1m, on_backlight_preset_1m_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_5m) {
        lv_obj_add_event_cb(objects.btn_backlight_5m, on_backlight_preset_5m_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_sleep_hours_minus) {
        lv_obj_add_event_cb(objects.btn_backlight_sleep_hours_minus, on_backlight_sleep_hours_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_sleep_hours_plus) {
        lv_obj_add_event_cb(objects.btn_backlight_sleep_hours_plus, on_backlight_sleep_hours_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_sleep_minutes_minus) {
        lv_obj_add_event_cb(objects.btn_backlight_sleep_minutes_minus, on_backlight_sleep_minutes_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_sleep_minutes_plus) {
        lv_obj_add_event_cb(objects.btn_backlight_sleep_minutes_plus, on_backlight_sleep_minutes_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_wake_hours_minus) {
        lv_obj_add_event_cb(objects.btn_backlight_wake_hours_minus, on_backlight_wake_hours_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_wake_hours_plus) {
        lv_obj_add_event_cb(objects.btn_backlight_wake_hours_plus, on_backlight_wake_hours_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_wake_minutes_minus) {
        lv_obj_add_event_cb(objects.btn_backlight_wake_minutes_minus, on_backlight_wake_minutes_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_backlight_wake_minutes_plus) {
        lv_obj_add_event_cb(objects.btn_backlight_wake_minutes_plus, on_backlight_wake_minutes_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_night_mode) {
        if (night_mode) {
            lv_obj_add_state(objects.btn_night_mode, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_night_mode, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.btn_night_mode, on_night_mode_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_units_c_f) {
        if (temp_units_c) {
            lv_obj_add_state(objects.btn_units_c_f, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_units_c_f, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.btn_units_c_f, on_units_c_f_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_restart) {
        lv_obj_add_event_cb(objects.btn_restart, on_restart_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_factory_reset) {
        lv_obj_add_event_cb(objects.btn_factory_reset, on_factory_reset_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_voc_reset) {
        lv_obj_add_event_cb(objects.btn_voc_reset, on_voc_reset_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_confirm_ok) {
        lv_obj_add_event_cb(objects.btn_confirm_ok, on_confirm_ok_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_confirm_cancel) {
        lv_obj_add_event_cb(objects.btn_confirm_cancel, on_confirm_cancel_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_datetime_back) {
        lv_obj_add_event_cb(objects.btn_datetime_back, on_datetime_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_datetime_apply) {
        lv_obj_add_event_cb(objects.btn_datetime_apply, on_datetime_apply_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_ntp_toggle) {
        lv_obj_add_event_cb(objects.btn_ntp_toggle, on_ntp_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_tz_plus) {
        lv_obj_add_event_cb(objects.btn_tz_plus, on_tz_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_tz_minus) {
        lv_obj_add_event_cb(objects.btn_tz_minus, on_tz_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_time_hours_minus) {
        lv_obj_add_event_cb(objects.btn_set_time_hours_minus, on_set_time_hours_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_time_hours_plus) {
        lv_obj_add_event_cb(objects.btn_set_time_hours_plus, on_set_time_hours_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_time_minutes_minus) {
        lv_obj_add_event_cb(objects.btn_set_time_minutes_minus, on_set_time_minutes_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_time_minutes_plus) {
        lv_obj_add_event_cb(objects.btn_set_time_minutes_plus, on_set_time_minutes_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_day_minus) {
        lv_obj_add_event_cb(objects.btn_set_date_day_minus, on_set_date_day_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_day_plus) {
        lv_obj_add_event_cb(objects.btn_set_date_day_plus, on_set_date_day_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_month_minus) {
        lv_obj_add_event_cb(objects.btn_set_date_month_minus, on_set_date_month_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_month_plus) {
        lv_obj_add_event_cb(objects.btn_set_date_month_plus, on_set_date_month_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_year_minus) {
        lv_obj_add_event_cb(objects.btn_set_date_year_minus, on_set_date_year_minus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_set_date_year_plus) {
        lv_obj_add_event_cb(objects.btn_set_date_year_plus, on_set_date_year_plus_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_led_indicators) {
        if (led_indicators_enabled) {
            lv_obj_add_state(objects.btn_led_indicators, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_led_indicators, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.btn_led_indicators, on_led_indicators_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_alert_blink) {
        if (alert_blink_enabled) {
            lv_obj_add_state(objects.btn_alert_blink, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_alert_blink, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.btn_alert_blink, on_alert_blink_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_co2_calib_asc) {
        if (co2_asc_enabled) {
            lv_obj_add_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(objects.btn_co2_calib_asc, on_co2_calib_asc_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    if (objects.btn_wifi_forget) {
        lv_obj_add_event_cb(objects.btn_wifi_forget, on_wifi_forget_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_co2_calib) {
        lv_obj_add_event_cb(objects.btn_co2_calib, on_co2_calib_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_co2_calib_back) {
        lv_obj_add_event_cb(objects.btn_co2_calib_back, on_co2_calib_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_co2_calib_start) {
        lv_obj_add_event_cb(objects.btn_co2_calib_start, on_co2_calib_start_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_temp_offset_minus) {
        lv_obj_add_event_cb(objects.btn_temp_offset_minus, on_temp_offset_minus_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_temp_offset_plus) {
        lv_obj_add_event_cb(objects.btn_temp_offset_plus, on_temp_offset_plus_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_hum_offset_minus) {
        lv_obj_add_event_cb(objects.btn_hum_offset_minus, on_hum_offset_minus_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_hum_offset_plus) {
        lv_obj_add_event_cb(objects.btn_hum_offset_plus, on_hum_offset_plus_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_theme_color) {
        lv_obj_add_event_cb(objects.btn_theme_color, on_theme_color_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_theme_back) {
        lv_obj_add_event_cb(objects.btn_theme_back, on_theme_back_event_cb, LV_EVENT_CLICKED, nullptr);
    }
    if (objects.btn_diag_continue) {
        lv_obj_add_event_cb(objects.btn_diag_continue, on_boot_diag_continue_cb, LV_EVENT_CLICKED, nullptr);
    }
    themeManager.registerEvents(apply_toggle_style_cb, on_theme_swatch_event_cb, on_theme_tab_event_cb);
    {
        bool presets = themeManager.isCurrentPreset();
        if (objects.btn_theme_presets) {
            if (presets) lv_obj_add_state(objects.btn_theme_presets, LV_STATE_CHECKED);
            else lv_obj_clear_state(objects.btn_theme_presets, LV_STATE_CHECKED);
        }
        if (objects.btn_theme_custom) {
            if (presets) lv_obj_clear_state(objects.btn_theme_custom, LV_STATE_CHECKED);
            else lv_obj_add_state(objects.btn_theme_custom, LV_STATE_CHECKED);
        }
        update_theme_custom_info(presets);
    }
    if (objects.page_boot_logo) {
        loadScreen(SCREEN_ID_PAGE_BOOT_LOGO);
        current_screen_id = SCREEN_ID_PAGE_BOOT_LOGO;
        pending_screen_id = 0;
        boot_logo_active = true;
        boot_logo_start_ms = millis();
    }
    lvgl_port_unlock();
    last_clock_tick_ms = millis();
}

void UiController::onSensorPoll(const SensorManager::PollResult &poll) {
    if (poll.data_changed || poll.warmup_changed) {
        data_dirty = true;
    }
}

void UiController::onTimePoll(const TimeManager::PollResult &poll) {
    if (poll.state_changed) {
        datetime_ui_dirty = true;
    }
    if (poll.time_updated) {
        apply_auto_night_now();
        clock_ui_dirty = true;
        datetime_ui_dirty = true;
    }
}

void UiController::markDatetimeDirty() {
    datetime_ui_dirty = true;
}

void UiController::mqtt_sync_with_wifi() {
    mqttManager.syncWithWifi();
    sync_mqtt_toggle_state();
}

void UiController::poll(uint32_t now) {
    bool desired = false;
    if (nightModeManager.poll(night_mode, desired)) {
        set_night_mode_state(desired, true);
    }
    if (now - last_clock_tick_ms >= CLOCK_TICK_MS) {
        last_clock_tick_ms = now;
        clock_ui_dirty = true;
    }
    if (now - last_blink_ms >= BLINK_PERIOD_MS) {
        last_blink_ms = now;
        if (alert_blink_enabled) {
            blink_state = !blink_state;
            if (current_screen_id == SCREEN_ID_PAGE_MAIN ||
                current_screen_id == SCREEN_ID_PAGE_SETTINGS) {
                data_dirty = true;
            }
        }
    }
    if (current_screen_id == SCREEN_ID_PAGE_MAIN &&
        status_msg_count > 1 &&
        (now - status_msg_last_ms) >= STATUS_ROTATE_MS) {
        data_dirty = true;
    }

    if (!lvgl_ready) {
        return;
    }

    if (boot_logo_active &&
        (now - boot_logo_start_ms) >= Config::BOOT_LOGO_MS &&
        current_screen_id == SCREEN_ID_PAGE_BOOT_LOGO &&
        pending_screen_id == 0) {
        if (objects.page_boot_diag) {
            pending_screen_id = SCREEN_ID_PAGE_BOOT_DIAG;
            boot_diag_active = true;
            boot_diag_has_error = false;
            boot_diag_start_ms = now;
            last_boot_diag_update_ms = 0;
        } else {
            pending_screen_id = SCREEN_ID_PAGE_MAIN;
        }
        boot_logo_active = false;
        data_dirty = true;
    }

    if (boot_diag_active &&
        current_screen_id == SCREEN_ID_PAGE_BOOT_DIAG &&
        pending_screen_id == 0 &&
        !boot_diag_has_error &&
        (now - boot_diag_start_ms) >= Config::BOOT_DIAG_MS) {
        pending_screen_id = SCREEN_ID_PAGE_MAIN;
        boot_diag_active = false;
        data_dirty = true;
    }

    bool allow_ui_update = true;
    if (networkManager.state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG &&
        (now - last_ui_update_ms) < WIFI_UI_UPDATE_MS) {
        allow_ui_update = false;
    }
    lvgl_port_lock(-1);
    mqtt_apply_pending();
    if ((now - last_ui_tick_ms) >= UI_TICK_MS) {
        ui_tick();
        last_ui_tick_ms = now;
    }
    backlightManager.poll(lvgl_ready);
    update_status_icons();
    if (pending_screen_id != 0) {
        int next_screen = pending_screen_id;
        loadScreen(static_cast<ScreensEnum>(next_screen));
        current_screen_id = next_screen;
        pending_screen_id = 0;
        if (current_screen_id == SCREEN_ID_PAGE_SETTINGS) {
            temp_offset_ui_dirty = true;
            hum_offset_ui_dirty = true;
            data_dirty = true;
        } else if (current_screen_id == SCREEN_ID_PAGE_MAIN) {
            data_dirty = true;
        } else if (current_screen_id == SCREEN_ID_PAGE_CLOCK) {
            datetime_ui_dirty = true;
            clock_ui_dirty = true;
        } else if (current_screen_id == SCREEN_ID_PAGE_WIFI) {
            networkManager.markUiDirty();
        } else if (current_screen_id == SCREEN_ID_PAGE_BACKLIGHT) {
            backlightManager.markUiDirty();
        } else if (current_screen_id == SCREEN_ID_PAGE_AUTO_NIGHT_MODE) {
            nightModeManager.markUiDirty();
        }
    }
    if (allow_ui_update &&
        current_screen_id == SCREEN_ID_PAGE_BOOT_DIAG &&
        (now - last_boot_diag_update_ms) >= 200) {
        update_boot_diag(now);
        last_boot_diag_update_ms = now;
    }
    if (allow_ui_update) {
        bool did_update = false;
        if (temp_offset_ui_dirty) {
            update_temp_offset_label();
            temp_offset_ui_dirty = false;
            did_update = true;
        }
        if (hum_offset_ui_dirty) {
            update_hum_offset_label();
            hum_offset_ui_dirty = false;
            did_update = true;
        }
        if (networkManager.isUiDirty()) {
            update_wifi_ui();
            networkManager.clearUiDirty();
            did_update = true;
        }
        if (mqttManager.isUiDirty()) {
            update_mqtt_ui();
            mqttManager.clearUiDirty();
            did_update = true;
        }
        if (clock_ui_dirty) {
            update_clock_labels();
            clock_ui_dirty = false;
            did_update = true;
        }
        if (datetime_ui_dirty && current_screen_id == SCREEN_ID_PAGE_CLOCK) {
            update_datetime_ui();
            datetime_ui_dirty = false;
            did_update = true;
        }
        if (backlightManager.isUiDirty() && current_screen_id == SCREEN_ID_PAGE_BACKLIGHT) {
            backlightManager.updateUi();
            did_update = true;
        }
        if (nightModeManager.isUiDirty() && current_screen_id == SCREEN_ID_PAGE_AUTO_NIGHT_MODE) {
            nightModeManager.updateUi();
            did_update = true;
        }
        if (data_dirty) {
            if (current_screen_id == SCREEN_ID_PAGE_MAIN) {
                update_ui();
            } else if (current_screen_id == SCREEN_ID_PAGE_SETTINGS) {
                update_settings_header();
            }
            data_dirty = false;
            did_update = true;
        }
        if (did_update) {
            last_ui_update_ms = now;
        }
    }
    lvgl_port_unlock();
}

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
    LOGD("UI", "back pressed");
    if (temp_offset_dirty || hum_offset_dirty) {
        auto &cfg = storage.config();
        if (temp_offset_dirty) {
            cfg.temp_offset = temp_offset;
            temp_offset_saved = temp_offset;
            temp_offset_dirty = false;
        }
        if (hum_offset_dirty) {
            cfg.hum_offset = hum_offset;
            hum_offset_saved = hum_offset;
            hum_offset_dirty = false;
        }
        storage.saveConfig(true);
        LOGI("UI", "offsets saved");
    }
    pending_screen_id = SCREEN_ID_PAGE_MAIN;
}

void UiController::on_wifi_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    sync_wifi_toggle_state();
    pending_screen_id = SCREEN_ID_PAGE_WIFI;
}

void UiController::on_wifi_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    networkManager.applyEnabledIfDirty();
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_mqtt_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    mqttManager.markUiDirty();
    networkManager.setMqttScreenOpen(true);
    pending_screen_id = SCREEN_ID_PAGE_MQTT;
}

void UiController::on_mqtt_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    networkManager.setMqttScreenOpen(false);
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
    themeManager.setThemeScreenOpen(true);
    themeManager.setCustomTabSelected(!presets);
    pending_screen_id = SCREEN_ID_PAGE_THEME;
}

void UiController::on_theme_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (themeManager.hasPreview()) {
        themeManager.applyPreviewAsCurrent(storage, night_mode, datetime_ui_dirty);
    }
    themeManager.setThemeScreenOpen(false);
    themeManager.setCustomTabSelected(false);
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
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == networkManager.isEnabled()) {
        return;
    }
    networkManager.setEnabled(enabled);
    sync_wifi_toggle_state();
    if (timeManager.updateWifiState(networkManager.isEnabled(), networkManager.isConnected())) {
        datetime_ui_dirty = true;
    }
    mqtt_sync_with_wifi();
    datetime_ui_dirty = true;
}

void UiController::on_mqtt_toggle_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
    if (enabled == mqttManager.isUserEnabled()) {
        return;
    }
    mqttManager.setUserEnabled(enabled);
    mqtt_sync_with_wifi();
}

void UiController::on_mqtt_reconnect_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!mqttManager.isEnabled() || !networkManager.isEnabled() || !networkManager.isConnected()) {
        return;
    }
    mqttManager.requestReconnect();
    mqttManager.markUiDirty();
}

void UiController::on_wifi_reconnect_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!networkManager.isEnabled()) {
        networkManager.setEnabled(true);
    } else if (networkManager.ssid().isEmpty()) {
        networkManager.startApOnDemand();
    } else {
        networkManager.connectSta();
    }
    sync_wifi_toggle_state();
    mqtt_sync_with_wifi();
    datetime_ui_dirty = true;
}

void UiController::on_wifi_start_ap_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    networkManager.startApOnDemand();
    sync_wifi_toggle_state();
    mqtt_sync_with_wifi();
    datetime_ui_dirty = true;
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
    networkManager.clearCredentials();
    datetime_ui_dirty = true;
}

void UiController::on_head_status_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    header_status_enabled = lv_obj_has_state(btn, LV_STATE_CHECKED);
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
    apply_auto_night_now();
    mqttManager.updateNightModeAvailability(nightModeManager.isAutoEnabled());
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
        delay(100);
        ESP.restart();
    } else if (action == CONFIRM_FACTORY_RESET) {
        LOGW("UI", "factory reset requested");
        storage.clearAll();
        WiFi.disconnect(true, true);
        delay(100);
        ESP.restart();
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
    if (use_c == temp_units_c) {
        return;
    }
    temp_units_c = use_c;
    storage.config().units_c = temp_units_c;
    storage.saveConfig(true);
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
    storage.saveConfig(true);
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
    storage.saveConfig(true);
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

void UiController::on_backlight_preset_5m_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (backlightManager.isPresetSyncing()) {
        return;
    }
    backlightManager.setTimeoutMs(BACKLIGHT_TIMEOUT_5M);
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
    storage.saveConfig(true);
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
    temp_offset -= 0.1f;
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
    temp_offset += 0.1f;
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
    pending_screen_id = SCREEN_ID_PAGE_MAIN;
    boot_diag_active = false;
    data_dirty = true;
}

void UiController::safe_label_set_text(lv_obj_t *obj, const char *new_text) {
    if (!obj) return;
    const char *current = lv_label_get_text(obj);
    if (current && strcmp(current, new_text) == 0) return;
    lv_label_set_text(obj, new_text);
}

lv_color_t UiController::color_inactive() { return lv_color_hex(0x3a3a3a); }

lv_color_t UiController::color_green() { return lv_color_hex(0x00c853); }
lv_color_t UiController::color_yellow() { return lv_color_hex(0xffeb3b); }
lv_color_t UiController::color_orange() { return lv_color_hex(0xff9800); }
lv_color_t UiController::color_red() { return lv_color_hex(0xff1100); }
lv_color_t UiController::color_blue() { return lv_color_hex(0x2196f3); }
lv_color_t UiController::color_card_border() {
    if (objects.card_co2) {
        return lv_obj_get_style_border_color(objects.card_co2, LV_PART_MAIN);
    }
    return lv_color_hex(0xffe19756);
}

lv_color_t UiController::getTempColor(float t) {
    if (t >= 21.0f && t <= 25.0f) return color_green();
    if ((t >= 20.0f && t < 21.0f) || (t > 25.0f && t <= 26.0f)) return color_yellow();
    if ((t >= 19.0f && t < 20.0f) || (t > 26.0f && t <= 27.0f)) return color_orange();
    return color_red();
}

lv_color_t UiController::getHumidityColor(float h) {
    if (h >= 40.0f && h <= 60.0f) return color_green();
    if ((h >= 30.0f && h < 40.0f) || (h > 60.0f && h <= 65.0f)) return color_yellow();
    if ((h >= 20.0f && h < 30.0f) || (h > 65.0f && h <= 70.0f)) return color_orange();
    return color_red();
}

float UiController::compute_dew_point_c(float temp_c, float rh) {
    if (!isfinite(temp_c) || !isfinite(rh) || rh <= 0.0f) {
        return NAN;
    }
    float rh_clamped = fminf(fmaxf(rh, 1.0f), 100.0f);
    constexpr float kA = 17.62f;
    constexpr float kB = 243.12f;
    float gamma = logf(rh_clamped / 100.0f) + (kA * temp_c) / (kB + temp_c);
    return (kB * gamma) / (kA - gamma);
}

lv_color_t UiController::getDewPointColor(float dew_c) {
    if (!isfinite(dew_c)) return color_inactive();
    if (dew_c < 5.0f) return color_red();
    if (dew_c <= 10.0f) return color_orange();
    if (dew_c <= 16.0f) return color_green();
    if (dew_c <= 18.0f) return color_yellow();
    if (dew_c <= 21.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getCO2Color(int co2) {
    if (co2 < 800) return color_green();
    if (co2 <= 1000) return color_yellow();
    if (co2 <= 1500) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM25Color(float pm) {
    if (pm <= 12.0f) return color_green();
    if (pm <= 35.0f) return color_yellow();
    if (pm <= 55.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM10Color(float pm) {
    if (pm <= 54.0f) return color_green();
    if (pm <= 154.0f) return color_yellow();
    if (pm <= 254.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getPressureDeltaColor(float delta, bool valid, bool is24h) {
    if (!valid) return color_inactive();
    float d = fabsf(delta);
    if (is24h) {
        if (d < 2.0f) return color_green();
        if (d <= 6.0f) return color_yellow();
        if (d <= 10.0f) return color_orange();
        return color_red();
    }
    if (d < 1.0f) return color_green();
    if (d <= 3.0f) return color_yellow();
    if (d <= 6.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getVOCColor(int voc) {
    if (voc <= 150) return color_green();
    if (voc <= 250) return color_yellow();
    if (voc <= 350) return color_orange();
    return color_red();
}

lv_color_t UiController::getNOxColor(int nox) {
    if (nox <= 50) return color_green();
    if (nox <= 100) return color_yellow();
    if (nox <= 200) return color_orange();
    return color_red();
}

lv_color_t UiController::getHCHOColor(float hcho_ppb, bool valid) {
    if (!valid || !isfinite(hcho_ppb) || hcho_ppb < 0.0f) return color_inactive();
    if (hcho_ppb < 30.0f) return color_green();
    if (hcho_ppb <= 60.0f) return color_yellow();
    if (hcho_ppb <= 100.0f) return color_orange();
    return color_red();
}

AirQuality UiController::getAirQuality(const SensorData &data) {
    AirQuality aq;
    bool gas_warmup = sensorManager.isWarmupActive();
    bool has_valid = false;
    int max_score = 0;

    if (data.co2_valid && data.co2 > 0) {
        int score = score_from_thresholds(static_cast<float>(data.co2), 400.0f, 800.0f, 1000.0f, 1500.0f);
        max_score = max(max_score, score);
        has_valid = true;
    }

    if (data.pm25_valid && isfinite(data.pm25) && data.pm25 >= 0.0f) {
        int score = score_from_thresholds(data.pm25, 0.0f, 12.0f, 35.0f, 55.0f);
        max_score = max(max_score, score);
        has_valid = true;
    }

    if (data.hcho_valid && isfinite(data.hcho) && data.hcho >= 0.0f) {
        int score = score_from_thresholds(data.hcho, 0.0f, 30.0f, 60.0f, 100.0f);
        max_score = max(max_score, score);
        has_valid = true;
    }

    if (!gas_warmup && data.nox_valid && data.nox_index >= 0) {
        int score = score_from_thresholds(static_cast<float>(data.nox_index), 1.0f, 50.0f, 100.0f, 200.0f);
        max_score = max(max_score, score);
        has_valid = true;
    }

    if (!gas_warmup && data.voc_valid && data.voc_index >= 0) {
        int score = score_from_voc(static_cast<float>(data.voc_index));
        max_score = max(max_score, score);
        has_valid = true;
    }

    if (!has_valid) {
        aq.status = "Initializing";
        aq.score = 0;
        aq.color = color_blue();
        return aq;
    }

    aq.score = max_score;

    if (aq.score <= 25)      { aq.status = "Excellent"; aq.color = color_green(); }
    else if (aq.score <= 50) { aq.status = "Good";      aq.color = color_green(); }
    else if (aq.score <= 75) { aq.status = "Moderate";  aq.color = color_yellow(); }
    else                     { aq.status = "Poor";      aq.color = color_red(); }

    return aq;
}

void UiController::set_dot_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (color.full == color_inactive().full) {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

lv_color_t UiController::blink_red(lv_color_t color) {
    if (alert_blink_enabled && (color.full == color_red().full) && !blink_state) {
        return color_inactive();
    }
    return color;
}

lv_color_t UiController::night_alert_color(lv_color_t color) {
    if (color.full == color_red().full) {
        return color_red();
    }
    return color_inactive();
}

lv_color_t UiController::alert_color_for_mode(lv_color_t color) {
    if (night_mode) {
        return night_alert_color(color);
    }
    return blink_red(color);
}

void UiController::compute_header_style(const AirQuality &aq, lv_color_t &color, lv_opa_t &shadow_opa) {
    lv_color_t base = header_status_enabled ? aq.color : color_card_border();
    shadow_opa = header_status_enabled ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (alert_blink_enabled && header_status_enabled && (base.full == color_red().full) && !blink_state) {
        color = color_inactive();
        shadow_opa = LV_OPA_TRANSP;
        return;
    }
    color = base;
}

void UiController::apply_toggle_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_border_color(btn, color_green(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_color(btn, color_green(), LV_PART_MAIN | LV_STATE_CHECKED);
}

void UiController::update_clock_labels() {
    char buf[16];
    tm local_tm = {};
    if (!timeManager.getLocalTime(local_tm)) {
        if (objects.label_time_value) safe_label_set_text(objects.label_time_value, "--:--");
        if (objects.label_date_value) safe_label_set_text(objects.label_date_value, "--.--.----");
        if (objects.label_time_value_1) safe_label_set_text(objects.label_time_value_1, "--:--");
        if (objects.label_date_value_1) safe_label_set_text(objects.label_date_value_1, "--.--.----");
        return;
    }
    snprintf(buf, sizeof(buf), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
    if (objects.label_time_value) safe_label_set_text(objects.label_time_value, buf);
    if (objects.label_time_value_1) safe_label_set_text(objects.label_time_value_1, buf);
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
             local_tm.tm_mday,
             local_tm.tm_mon + 1,
             local_tm.tm_year + 1900);
    if (objects.label_date_value) safe_label_set_text(objects.label_date_value, buf);
    if (objects.label_date_value_1) safe_label_set_text(objects.label_date_value_1, buf);
}

bool UiController::boot_diag_has_errors(uint32_t now_ms) {
    bool has_error = false;
    if (!storage.isMounted()) {
        has_error = true;
    }
    if (!boot_i2c_recovered) {
        has_error = true;
    }
    if (!boot_touch_detected) {
        has_error = true;
    }
    if (is_crash_reset(boot_reset_reason)) {
        has_error = true;
    }
    if (!sensorManager.isOk()) {
        uint32_t retry_at = sensorManager.retryAtMs();
        if (retry_at == 0 || now_ms >= retry_at) {
            has_error = true;
        }
    }
    if (!sensorManager.isDpsOk()) {
        has_error = true;
    }
    if (!sensorManager.isSfaOk()) {
        has_error = true;
    }
    if (timeManager.isRtcPresent()) {
        if (timeManager.isRtcLostPower() || !timeManager.isRtcValid()) {
            has_error = true;
        }
    }
    return has_error;
}

void UiController::update_boot_diag(uint32_t now_ms) {
    char buf[64];

    if (objects.lbl_diag_app_ver) {
        snprintf(buf, sizeof(buf), "v%s", APP_VERSION);
        safe_label_set_text(objects.lbl_diag_app_ver, buf);
    }
    if (objects.lbl_diag_mac) {
        String mac = WiFi.macAddress();
        safe_label_set_text(objects.lbl_diag_mac, mac.c_str());
    }
    if (objects.lbl_diag_reason) {
        const char *reason = reset_reason_to_string(boot_reset_reason);
        if (safe_boot_stage > 0) {
            snprintf(buf, sizeof(buf), "%s / boot=%lu safe=%lu",
                     reason,
                     static_cast<unsigned long>(boot_count),
                     static_cast<unsigned long>(safe_boot_stage));
        } else {
            snprintf(buf, sizeof(buf), "%s / boot=%lu",
                     reason,
                     static_cast<unsigned long>(boot_count));
        }
        safe_label_set_text(objects.lbl_diag_reason, buf);
    }
    if (objects.lbl_diag_heap) {
        size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t min_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        size_t max_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        snprintf(buf, sizeof(buf), "free %uk / min %uk / max %uk",
                 static_cast<unsigned>(free_bytes / 1024),
                 static_cast<unsigned>(min_bytes / 1024),
                 static_cast<unsigned>(max_bytes / 1024));
        safe_label_set_text(objects.lbl_diag_heap, buf);
    }
    if (objects.lbl_diag_storage) {
        const char *status = "ERR";
        if (storage.isMounted()) {
            status = storage.isConfigLoaded() ? "OK (config)" : "OK (defaults)";
        }
        safe_label_set_text(objects.lbl_diag_storage, status);
    }
    if (objects.lbl_diag_i2c) {
        safe_label_set_text(objects.lbl_diag_i2c, boot_i2c_recovered ? "RECOVERED" : "FAIL");
    }
    if (objects.lbl_diag_touch) {
        safe_label_set_text(objects.lbl_diag_touch, boot_touch_detected ? "DETECTED" : "FAIL");
    }
    if (objects.lbl_diag_sen) {
        const char *status = "ERR";
        if (sensorManager.isOk()) {
            status = "OK";
        } else {
            uint32_t retry_at = sensorManager.retryAtMs();
            if (retry_at != 0 && now_ms < retry_at) {
                status = "STARTING";
            }
        }
        safe_label_set_text(objects.lbl_diag_sen, status);
    }
    if (objects.lbl_diag_dps_label) {
        safe_label_set_text(objects.lbl_diag_dps_label, sensorManager.pressureSensorLabel());
    }
    if (objects.lbl_diag_dps) {
        safe_label_set_text(objects.lbl_diag_dps, sensorManager.isDpsOk() ? "OK" : "ERR");
    }
    if (objects.lbl_diag_sfa) {
        safe_label_set_text(objects.lbl_diag_sfa, sensorManager.isSfaOk() ? "OK" : "ERR");
    }
    if (objects.lbl_diag_rtc) {
        const char *status = "NOT FOUND";
        if (timeManager.isRtcPresent()) {
            if (timeManager.isRtcLostPower()) {
                status = "LOST";
            } else if (timeManager.isRtcValid()) {
                status = "OK";
            } else {
                status = "ERR";
            }
        }
        safe_label_set_text(objects.lbl_diag_rtc, status);
    }

    bool has_errors = boot_diag_has_errors(now_ms);
    boot_diag_has_error = has_errors;
    set_visible(objects.lbl_diag_error, has_errors);
    set_visible(objects.btn_diag_continue, has_errors);
}

void UiController::set_button_enabled(lv_obj_t *btn, bool enabled) {
    if (!btn) return;
    if (enabled) lv_obj_clear_state(btn, LV_STATE_DISABLED);
    else lv_obj_add_state(btn, LV_STATE_DISABLED);
}

lv_color_t UiController::active_text_color() {
    return themeManager.activeTextColor(night_mode);
}

void UiController::update_datetime_ui() {
    if (objects.label_ntp_interval) {
        safe_label_set_text(objects.label_ntp_interval, "Every 6h");
    }

    const TimeZoneEntry &tz = timeManager.getTimezone();
    char offset_buf[12];
    TimeManager::formatTzOffset(tz.offset_min, offset_buf, sizeof(offset_buf));
    if (objects.label_tz_offset_value) {
        safe_label_set_text(objects.label_tz_offset_value, offset_buf);
    }
    if (objects.label_tz_name) {
        safe_label_set_text(objects.label_tz_name, tz.name);
    }

    const lv_color_t text_on = active_text_color();
    const lv_color_t text_off = color_inactive();
    bool controls_enabled = !timeManager.isManualLocked(millis());

    set_button_enabled(objects.btn_set_time_hours_minus, controls_enabled);
    set_button_enabled(objects.btn_set_time_hours_plus, controls_enabled);
    set_button_enabled(objects.btn_set_time_minutes_minus, controls_enabled);
    set_button_enabled(objects.btn_set_time_minutes_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_day_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_day_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_month_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_month_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_year_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_year_plus, controls_enabled);
    set_button_enabled(objects.btn_datetime_apply, controls_enabled);

    if (objects.label_set_time_hours_value) {
        lv_obj_set_style_text_color(objects.label_set_time_hours_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_time_minutes_value) {
        lv_obj_set_style_text_color(objects.label_set_time_minutes_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_day_value) {
        lv_obj_set_style_text_color(objects.label_set_date_day_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_month_value) {
        lv_obj_set_style_text_color(objects.label_set_date_month_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_year_value) {
        lv_obj_set_style_text_color(objects.label_set_date_year_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", set_hour);
    if (objects.label_set_time_hours_value) safe_label_set_text(objects.label_set_time_hours_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_minute);
    if (objects.label_set_time_minutes_value) safe_label_set_text(objects.label_set_time_minutes_value, buf);

    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    if (set_day < 1) set_day = 1;
    snprintf(buf, sizeof(buf), "%02d", set_day);
    if (objects.label_set_date_day_value) safe_label_set_text(objects.label_set_date_day_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_month);
    if (objects.label_set_date_month_value) safe_label_set_text(objects.label_set_date_month_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_year % 100);
    if (objects.label_set_date_year_value) safe_label_set_text(objects.label_set_date_year_value, buf);

    if (objects.btn_ntp_toggle) {
        ntp_toggle_syncing = true;
        if (timeManager.isNtpEnabled()) lv_obj_add_state(objects.btn_ntp_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_ntp_toggle, LV_STATE_CHECKED);
        ntp_toggle_syncing = false;
    }
    set_button_enabled(objects.btn_ntp_toggle, networkManager.isEnabled());

    TimeManager::NtpUiState ntp_state = timeManager.getNtpUiState(millis());
    lv_color_t ntp_color = color_yellow();
    const char *ntp_label = "OFF";
    if (ntp_state == TimeManager::NTP_UI_SYNCING) {
        ntp_color = color_blue();
        ntp_label = "SYNC";
    } else if (ntp_state == TimeManager::NTP_UI_OK) {
        ntp_color = color_green();
        ntp_label = "OK";
    } else if (ntp_state == TimeManager::NTP_UI_ERR) {
        ntp_color = color_red();
        ntp_label = "ERR";
    }
    if (objects.dot_ntp_status) {
        set_dot_color(objects.dot_ntp_status, ntp_color);
    }
    if (objects.label_ntp_status) {
        safe_label_set_text(objects.label_ntp_status, ntp_label);
    }
    if (objects.chip_ntp_status) {
        set_chip_color(objects.chip_ntp_status, ntp_color);
    }

    if (objects.label_rtc_status) {
        if (!timeManager.isRtcPresent()) {
            safe_label_set_text(objects.label_rtc_status, "OFF");
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_yellow());
        } else if (!timeManager.isRtcValid()) {
            safe_label_set_text(objects.label_rtc_status, "ERR");
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_red());
        } else {
            safe_label_set_text(objects.label_rtc_status, "OK");
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_green());
        }
    }

    if (objects.label_wifi_status_1) {
        bool wifi_enabled = networkManager.isEnabled();
        AuraNetworkManager::WifiState wifi_state = networkManager.state();
        if (!wifi_enabled) {
            safe_label_set_text(objects.label_wifi_status_1, "OFF");
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_yellow());
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            safe_label_set_text(objects.label_wifi_status_1, "ON");
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_green());
        } else {
            safe_label_set_text(objects.label_wifi_status_1, "ON");
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_blue());
        }
    }
}

void UiController::set_night_mode_state(bool enabled, bool save_pref) {
    if (enabled == night_mode) {
        return;
    }
    if (save_pref) {
        storage.config().night_mode = enabled;
        storage.saveConfig(true);
    }
    night_mode = enabled;
    if (!lvgl_ready) {
        return;
    }
    if (enabled) {
        night_mode_on_enter();
    }
    themeManager.applyActive(night_mode, datetime_ui_dirty);
    if (!enabled) {
        night_mode_on_exit();
    }
    data_dirty = true;
}

void UiController::apply_auto_night_now() {
    bool desired = false;
    if (nightModeManager.applyNow(night_mode, desired)) {
        set_night_mode_state(desired, true);
    }
}

void UiController::sync_night_mode_toggle_ui() {
    if (!objects.btn_night_mode) {
        return;
    }
    set_button_enabled(objects.btn_night_mode, !nightModeManager.isAutoEnabled());
    if (night_mode) {
        lv_obj_add_state(objects.btn_night_mode, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_night_mode, LV_STATE_CHECKED);
    }
}

void UiController::sync_auto_dim_button_state() {
    if (!objects.btn_auto_dim) {
        return;
    }
    if (nightModeManager.isAutoEnabled()) {
        lv_obj_add_state(objects.btn_auto_dim, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_auto_dim, LV_STATE_CHECKED);
    }
}

void UiController::confirm_set_visible(lv_obj_t *obj, bool visible) {
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiController::confirm_show(ConfirmAction action) {
    confirm_action = action;
    if (!objects.container_confirm) {
        return;
    }
    bool show_voc = (action == CONFIRM_VOC_RESET);
    bool show_restart = (action == CONFIRM_RESTART);
    bool show_reset = (action == CONFIRM_FACTORY_RESET);

    confirm_set_visible(objects.container_confirm, true);
    confirm_set_visible(objects.container_confirm_card, true);
    confirm_set_visible(objects.btn_confirm_ok, true);
    confirm_set_visible(objects.btn_confirm_cancel, true);
    confirm_set_visible(objects.label_btn_confirm_cancel, true);
    lv_obj_add_flag(objects.container_confirm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(objects.container_confirm);

    confirm_set_visible(objects.label_btn_confirm_voc, show_voc);
    confirm_set_visible(objects.label_confirm_title_voc, show_voc);
    confirm_set_visible(objects.container_confirm_voc_text, show_voc);

    confirm_set_visible(objects.label_btn_confirm_restart, show_restart);
    confirm_set_visible(objects.label_confirm_title_restart, show_restart);
    confirm_set_visible(objects.container_confirm_restart_text, show_restart);

    confirm_set_visible(objects.label_btn_confirm_reset, show_reset);
    confirm_set_visible(objects.label_confirm_title_reset, show_reset);
    confirm_set_visible(objects.container_confirm_reset_text, show_reset);
}

void UiController::confirm_hide() {
    confirm_action = CONFIRM_NONE;
    confirm_set_visible(objects.container_confirm, false);
}

void UiController::mqtt_apply_pending() {
    MqttManager::PendingCommands pending;
    if (!mqttManager.takePending(pending)) {
        return;
    }
    bool publish_needed = false;
    if (pending.night_mode) {
        bool prev_night = night_mode;
        set_night_mode_state(pending.night_mode_value, true);
        sync_night_mode_toggle_ui();
        if (night_mode != prev_night) {
            publish_needed = true;
        }
    }
    if (pending.alert_blink) {
        if (alert_blink_enabled != pending.alert_blink_value) {
            alert_blink_enabled = pending.alert_blink_value;
            storage.config().alert_blink = alert_blink_enabled;
            storage.saveConfig(true);
            if (alert_blink_enabled) {
                blink_state = true;
                last_blink_ms = millis();
            }
            sync_alert_blink_toggle_state();
            data_dirty = true;
            publish_needed = true;
        }
    }
    if (pending.backlight) {
        bool prev_backlight = backlightManager.isOn();
        backlightManager.setOn(pending.backlight_value);
        if (backlightManager.isOn() != prev_backlight) {
            publish_needed = true;
        }
    }
    if (pending.restart) {
        LOGI("UI", "MQTT restart requested");
        delay(100);
        ESP.restart();
    }
    if (publish_needed) {
        mqttManager.requestPublish();
    }
}

void UiController::sync_alert_blink_toggle_state() {
    if (!objects.btn_alert_blink) {
        return;
    }
    alert_blink_syncing = true;
    if (alert_blink_enabled) {
        lv_obj_add_state(objects.btn_alert_blink, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_alert_blink, LV_STATE_CHECKED);
    }
    alert_blink_syncing = false;
}

void UiController::night_mode_on_enter() {
    alert_blink_before_night = alert_blink_enabled;
    night_blink_restore_pending = true;
    night_blink_user_changed = false;
    if (alert_blink_enabled) {
        alert_blink_enabled = false;
        sync_alert_blink_toggle_state();
    }
}

void UiController::night_mode_on_exit() {
    if (night_blink_restore_pending && !night_blink_user_changed) {
        if (alert_blink_enabled != alert_blink_before_night) {
            alert_blink_enabled = alert_blink_before_night;
            if (alert_blink_enabled) {
                blink_state = true;
                last_blink_ms = millis();
            }
            sync_alert_blink_toggle_state();
        }
    }
    night_blink_restore_pending = false;
    night_blink_user_changed = false;
}

void UiController::sync_wifi_toggle_state() {
    bool wifi_enabled = networkManager.isEnabled();
    if (objects.btn_wifi) {
        if (wifi_enabled) lv_obj_add_state(objects.btn_wifi, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_wifi, LV_STATE_CHECKED);
    }
    if (objects.btn_wifi_toggle) {
        if (wifi_enabled) lv_obj_add_state(objects.btn_wifi_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_wifi_toggle, LV_STATE_CHECKED);
    }
}

void UiController::sync_mqtt_toggle_state() {
    if (objects.btn_mqtt) {
        if (mqttManager.isEnabled()) lv_obj_add_state(objects.btn_mqtt, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_mqtt, LV_STATE_CHECKED);
    }
    if (objects.btn_mqtt_toggle) {
        if (mqttManager.isEnabled()) lv_obj_add_state(objects.btn_mqtt_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_mqtt_toggle, LV_STATE_CHECKED);
    }
}

void UiController::update_temp_offset_label() {
    if (!objects.label_temp_offset_value) {
        return;
    }
    float val = temp_offset;
    if (fabsf(val) < 0.05f) {
        val = 0.0f;
    }
    char buf[16];
    if (val > 0.0f) {
        snprintf(buf, sizeof(buf), "+%.1f", val);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", val);
    }
    safe_label_set_text(objects.label_temp_offset_value, buf);
}

void UiController::update_hum_offset_label() {
    if (!objects.label_hum_offset_value) {
        return;
    }
    float val = hum_offset;
    if (fabsf(val) < 0.5f) {
        val = 0.0f;
    }
    char buf[16];
    if (val > 0.0f) {
        snprintf(buf, sizeof(buf), "+%.0f%%", val);
    } else if (val < 0.0f) {
        snprintf(buf, sizeof(buf), "%.0f%%", val);
    } else {
        strcpy(buf, "0%");
    }
    safe_label_set_text(objects.label_hum_offset_value, buf);
}

void UiController::update_led_indicators() {
    const bool visible = led_indicators_enabled;
    if (objects.dot_co2) visible ? lv_obj_clear_flag(objects.dot_co2, LV_OBJ_FLAG_HIDDEN)
                                 : lv_obj_add_flag(objects.dot_co2, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_temp) visible ? lv_obj_clear_flag(objects.dot_temp, LV_OBJ_FLAG_HIDDEN)
                                  : lv_obj_add_flag(objects.dot_temp, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_hum) visible ? lv_obj_clear_flag(objects.dot_hum, LV_OBJ_FLAG_HIDDEN)
                                 : lv_obj_add_flag(objects.dot_hum, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_dp) visible ? lv_obj_clear_flag(objects.dot_dp, LV_OBJ_FLAG_HIDDEN)
                                : lv_obj_add_flag(objects.dot_dp, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_pm25) visible ? lv_obj_clear_flag(objects.dot_pm25, LV_OBJ_FLAG_HIDDEN)
                                  : lv_obj_add_flag(objects.dot_pm25, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_pm10) visible ? lv_obj_clear_flag(objects.dot_pm10, LV_OBJ_FLAG_HIDDEN)
                                  : lv_obj_add_flag(objects.dot_pm10, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_voc) visible ? lv_obj_clear_flag(objects.dot_voc, LV_OBJ_FLAG_HIDDEN)
                                 : lv_obj_add_flag(objects.dot_voc, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_nox) visible ? lv_obj_clear_flag(objects.dot_nox, LV_OBJ_FLAG_HIDDEN)
                                 : lv_obj_add_flag(objects.dot_nox, LV_OBJ_FLAG_HIDDEN);
    if (objects.dot_hcho) visible ? lv_obj_clear_flag(objects.dot_hcho, LV_OBJ_FLAG_HIDDEN)
                                  : lv_obj_add_flag(objects.dot_hcho, LV_OBJ_FLAG_HIDDEN);
}

void UiController::set_chip_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;
    lv_obj_set_style_border_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (color.full == color_inactive().full) {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::update_co2_bar(int co2, bool valid) {
    if (!objects.co2_bar_fill || !objects.co2_marker) {
        return;
    }
    if (!valid) {
        if (objects.co2_bar_mask) {
            lv_obj_set_width(objects.co2_bar_mask, 0);
        } else {
            lv_obj_set_width(objects.co2_bar_fill, 0);
        }
        lv_obj_set_x(objects.co2_marker, 2);
        return;
    }

    int bar_max = 330;
    int fill_w = lv_obj_get_width(objects.co2_bar_fill);
    if (fill_w > 0) {
        bar_max = fill_w;
    }
    int clamped = constrain(co2, 400, 2000);
    int w = map(clamped, 400, 2000, 0, bar_max);
    w = constrain(w, 0, bar_max);
    if (objects.co2_bar_mask) {
        lv_obj_set_width(objects.co2_bar_mask, w);
    } else {
        lv_obj_set_width(objects.co2_bar_fill, w);
    }

    const int marker_w = 14;
    int center = 4 + w;
    int x = center - (marker_w / 2);
    int track_w = objects.co2_bar_track ? lv_obj_get_width(objects.co2_bar_track) : 0;
    int max_x = (track_w > 0) ? (track_w - marker_w - 2) : (340 - marker_w - 2);
    x = constrain(x, 2, max_x);
    lv_obj_set_x(objects.co2_marker, x);
}

void UiController::update_ui() {
    AirQuality aq = getAirQuality(currentData);
    bool gas_warmup = sensorManager.isWarmupActive();
    bool show_co2_bar = !night_mode;
    const uint32_t now_ms = millis();
    update_status_message(now_ms, gas_warmup);
    lv_color_t header_col;
    lv_opa_t header_shadow;
    compute_header_style(aq, header_col, header_shadow);
    if (night_mode && header_status_enabled) {
        header_col = night_alert_color(aq.color);
        header_shadow = (header_col.full == color_red().full) ? LV_OPA_COVER : LV_OPA_TRANSP;
    }
    lv_obj_set_style_border_color(objects.container_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(objects.container_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(objects.container_header, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (objects.container_settings_header) {
        lv_obj_set_style_border_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(objects.container_settings_header, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    char buf[16];

    if (currentData.co2_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.co2);
    } else {
        strcpy(buf, "---");
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
        strcpy(buf, "---");
    }
    if (objects.label_temp_value) {
        safe_label_set_text(objects.label_temp_value, buf);
    }
    if (objects.label_temp_unit) {
        safe_label_set_text(objects.label_temp_unit, temp_units_c ? "C" : "F");
    }
    lv_color_t temp_col = currentData.temp_valid ? getTempColor(currentData.temperature) : color_inactive();
    set_dot_color(objects.dot_temp, alert_color_for_mode(temp_col));

    if (currentData.hum_valid) {
        snprintf(buf, sizeof(buf), "%.0f", currentData.humidity);
    } else {
        strcpy(buf, "---");
    }
    safe_label_set_text(objects.label_hum_value, buf);
    lv_color_t hum_col = currentData.hum_valid ? getHumidityColor(currentData.humidity) : color_inactive();
    set_dot_color(objects.dot_hum, alert_color_for_mode(hum_col));

    float dew_c = NAN;
    if (currentData.temp_valid && currentData.hum_valid) {
        dew_c = compute_dew_point_c(currentData.temperature, currentData.humidity);
    }
    if (objects.label_dew_value) {
        if (isfinite(dew_c)) {
            float dew_display = dew_c;
            if (!temp_units_c) {
                dew_display = (dew_display * 9.0f / 5.0f) + 32.0f;
            }
            snprintf(buf, sizeof(buf), "%.1f", dew_display);
        } else {
            strcpy(buf, "---");
        }
        safe_label_set_text(objects.label_dew_value, buf);
    }
    if (objects.label_dew_unit) {
        safe_label_set_text(objects.label_dew_unit, temp_units_c ? "C" : "F");
    }
    if (objects.dot_dp) {
        lv_color_t dp_col = getDewPointColor(dew_c);
        set_dot_color(objects.dot_dp, alert_color_for_mode(dp_col));
    }

    if (currentData.pm25_valid) {
        if (currentData.pm25 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm25);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm25);
    } else {
        strcpy(buf, "---");
    }
    safe_label_set_text(objects.label_pm25_value, buf);
    lv_color_t pm25_col = currentData.pm25_valid ? getPM25Color(currentData.pm25) : color_inactive();
    set_dot_color(objects.dot_pm25, alert_color_for_mode(pm25_col));

    if (currentData.pm10_valid) {
        if (currentData.pm10 < 10.0f) snprintf(buf, sizeof(buf), "%.1f", currentData.pm10);
        else snprintf(buf, sizeof(buf), "%.0f", currentData.pm10);
    } else {
        strcpy(buf, "---");
    }
    safe_label_set_text(objects.label_pm10_value, buf);
    lv_color_t pm10_col = currentData.pm10_valid ? getPM10Color(currentData.pm10) : color_inactive();
    set_dot_color(objects.dot_pm10, alert_color_for_mode(pm10_col));

    if (currentData.voc_valid) {
        snprintf(buf, sizeof(buf), "%d", currentData.voc_index);
    } else {
        strcpy(buf, "---");
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
        strcpy(buf, "---");
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
        safe_label_set_text(objects.label_hcho_title, hcho_available ? "HCHO" : "AQI");
    }
    if (objects.label_hcho_unit) {
        safe_label_set_text(objects.label_hcho_unit, hcho_available ? "ppb" : "Index");
    }
    if (objects.label_hcho_value) {
        if (hcho_available) {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(lroundf(currentData.hcho)));
        } else {
            snprintf(buf, sizeof(buf), "%d", aq.score);
        }
        safe_label_set_text(objects.label_hcho_value, buf);
    }
    lv_color_t hcho_col = hcho_available ? getHCHOColor(currentData.hcho, true) : aq.color;
    if (objects.dot_hcho) {
        set_dot_color(objects.dot_hcho, alert_color_for_mode(hcho_col));
    }

    if (currentData.pressure_valid) {
        snprintf(buf, sizeof(buf), "%.0f", currentData.pressure);
    } else {
        strcpy(buf, "---");
    }
    safe_label_set_text(objects.label_pressure_value, buf);

    if (currentData.pressure_delta_3h_valid) {
        if (currentData.pressure_delta_3h > 0.05f) {
            snprintf(buf, sizeof(buf), "+%.1f", currentData.pressure_delta_3h);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", currentData.pressure_delta_3h);
        }
    } else {
        strcpy(buf, "--");
    }
    safe_label_set_text(objects.label_delta_3h_value, buf);

    if (currentData.pressure_delta_24h_valid) {
        if (currentData.pressure_delta_24h > 0.05f) {
            snprintf(buf, sizeof(buf), "+%.1f", currentData.pressure_delta_24h);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", currentData.pressure_delta_24h);
        }
    } else {
        strcpy(buf, "--");
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

void UiController::update_settings_header() {
    if (!objects.container_settings_header) {
        return;
    }
    AirQuality aq = getAirQuality(currentData);
    lv_color_t header_col;
    lv_opa_t header_shadow;
    compute_header_style(aq, header_col, header_shadow);
    if (night_mode && header_status_enabled) {
        header_col = night_alert_color(aq.color);
        header_shadow = (header_col.full == color_red().full) ? LV_OPA_COVER : LV_OPA_TRANSP;
    }
    lv_obj_set_style_border_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(objects.container_settings_header, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    sync_night_mode_toggle_ui();
    sync_auto_dim_button_state();
}

void UiController::update_theme_custom_info(bool presets) {
    set_visible(objects.container_theme_custom_info, !presets);
    if (!presets && objects.qrcode_theme_custom) {
        static const char kThemeUrl[] = "http://aura.local/theme";
        lv_qrcode_update(objects.qrcode_theme_custom, kThemeUrl, strlen(kThemeUrl));
    }
}

void UiController::update_status_message(uint32_t now_ms, bool gas_warmup) {
    StatusMessage messages[12];
    size_t count = 0;
    bool has_valid = false;

    StatusSeverity co2_sev = STATUS_NONE;
    StatusSeverity voc_sev = STATUS_NONE;
    StatusSeverity hcho_sev = STATUS_NONE;
    StatusSeverity temp_sev = STATUS_NONE;
    StatusSeverity pm25_sev = STATUS_NONE;
    StatusSeverity pm10_sev = STATUS_NONE;
    StatusSeverity nox_sev = STATUS_NONE;
    StatusSeverity hum_sev = STATUS_NONE;
    StatusSeverity dp_sev = STATUS_NONE;

    const char *co2_msg = nullptr;
    const char *voc_msg = nullptr;
    const char *hcho_msg = nullptr;
    const char *temp_msg = nullptr;
    const char *pm25_msg = nullptr;
    const char *pm10_msg = nullptr;
    const char *nox_msg = nullptr;
    const char *hum_msg = nullptr;
    const char *dp_msg = nullptr;

    if (currentData.co2_valid && currentData.co2 > 0) {
        has_valid = true;
        if (currentData.co2 >= 1500) {
            co2_sev = STATUS_RED;
            co2_msg = "CO2 Very High! Ventilate now";
        } else if (currentData.co2 >= 1000) {
            co2_sev = STATUS_ORANGE;
            co2_msg = "CO2 High - Ventilate soon";
        } else if (currentData.co2 >= 800) {
            co2_sev = STATUS_YELLOW;
            co2_msg = "CO2 Rising - Consider ventilation";
        }
    }

    if (currentData.pm25_valid && isfinite(currentData.pm25) && currentData.pm25 >= 0.0f) {
        has_valid = true;
        if (currentData.pm25 >= 55.0f) {
            pm25_sev = STATUS_RED;
            pm25_msg = "PM2.5 Very High! Purify - stop smoke";
        } else if (currentData.pm25 >= 35.0f) {
            pm25_sev = STATUS_ORANGE;
            pm25_msg = "PM2.5 High - Purify - check source";
        } else if (currentData.pm25 >= 12.0f) {
            pm25_sev = STATUS_YELLOW;
            pm25_msg = "PM2.5 Elevated - Consider purifier";
        }
    }

    if (currentData.pm10_valid && isfinite(currentData.pm10) && currentData.pm10 >= 0.0f) {
        has_valid = true;
        if (currentData.pm10 >= 254.0f) {
            pm10_sev = STATUS_RED;
            pm10_msg = "PM10 Very High! Purify - check source";
        } else if (currentData.pm10 >= 154.0f) {
            pm10_sev = STATUS_ORANGE;
            pm10_msg = "PM10 High - Purify - reduce dust";
        } else if (currentData.pm10 >= 54.0f) {
            pm10_sev = STATUS_YELLOW;
            pm10_msg = "PM10 Elevated - Purify - reduce dust";
        }
    }

    if (currentData.hcho_valid && isfinite(currentData.hcho) && currentData.hcho >= 0.0f) {
        has_valid = true;
        if (currentData.hcho >= 100.0f) {
            hcho_sev = STATUS_RED;
            hcho_msg = "HCHO Very High! Ventilate - remove source";
        } else if (currentData.hcho >= 60.0f) {
            hcho_sev = STATUS_ORANGE;
            hcho_msg = "HCHO High - Ventilate - remove source";
        } else if (currentData.hcho >= 30.0f) {
            hcho_sev = STATUS_YELLOW;
            hcho_msg = "HCHO Detected - Check sources";
        }
    }

    if (!gas_warmup && currentData.voc_valid && currentData.voc_index > 0) {
        has_valid = true;
        if (currentData.voc_index >= 250) {
            voc_sev = STATUS_RED;
            voc_msg = "VOC Very High! Find source - vent";
        } else if (currentData.voc_index >= 150) {
            voc_sev = STATUS_ORANGE;
            voc_msg = "VOC High - Increase ventilation";
        } else if (currentData.voc_index >= 100) {
            voc_sev = STATUS_YELLOW;
            voc_msg = "VOC Elevated - Ventilate room";
        }
    }

    if (!gas_warmup && currentData.nox_valid && currentData.nox_index > 0) {
        has_valid = true;
        if (currentData.nox_index >= 200) {
            nox_sev = STATUS_RED;
            nox_msg = "NOx Very High! Stop source - ventilate";
        } else if (currentData.nox_index >= 100) {
            nox_sev = STATUS_ORANGE;
            nox_msg = "NOx High - Use hood - check source";
        } else if (currentData.nox_index >= 50) {
            nox_sev = STATUS_YELLOW;
            nox_msg = "NOx Elevated - Check stove/outside";
        }
    }

    if (currentData.temp_valid && isfinite(currentData.temperature)) {
        has_valid = true;
        const float t = currentData.temperature;
        if (t < 19.0f) {
            temp_sev = STATUS_RED;
            temp_msg = "Temp Too Cold! Heat now";
        } else if (t < 20.0f) {
            temp_sev = STATUS_ORANGE;
            temp_msg = "Temp Cold - Heat the room";
        } else if (t < 21.0f) {
            temp_sev = STATUS_YELLOW;
            temp_msg = "Temp Slightly Cool - Add heat";
        } else if (t > 27.0f) {
            temp_sev = STATUS_RED;
            temp_msg = "Temp Too Hot! Cool now";
        } else if (t > 26.0f) {
            temp_sev = STATUS_ORANGE;
            temp_msg = "Temp Warm - Ventilate / cool";
        } else if (t > 25.0f) {
            temp_sev = STATUS_YELLOW;
            temp_msg = "Temp Slightly Warm - Increase airflow";
        }
    }

    float dew_c = NAN;
    bool dp_low = false;
    bool dp_high = false;
    if (currentData.temp_valid && currentData.hum_valid) {
        dew_c = compute_dew_point_c(currentData.temperature, currentData.humidity);
        if (isfinite(dew_c)) {
            if (dew_c < 5.0f) {
                dp_sev = STATUS_RED;
                dp_msg = "DP Very Low! Humidify";
                dp_low = true;
            } else if (dew_c < 10.0f) {
                dp_sev = STATUS_YELLOW;
                dp_msg = "DP Low - Air may feel dry";
                dp_low = true;
            } else if (dew_c > 21.0f) {
                dp_sev = STATUS_RED;
                dp_msg = "DP Muggy! Dehumidify now";
                dp_high = true;
            } else if (dew_c > 18.0f) {
                dp_sev = STATUS_ORANGE;
                dp_msg = "DP Very High - Vent/dehumidify";
                dp_high = true;
            } else if (dew_c > 16.0f) {
                dp_sev = STATUS_YELLOW;
                dp_msg = "DP High - Air may feel muggy";
                dp_high = true;
            }
        }
    }

    if (currentData.hum_valid && isfinite(currentData.humidity)) {
        has_valid = true;
        const float h = currentData.humidity;
        if (h < 20.0f) {
            hum_sev = STATUS_RED;
            hum_msg = "Humidity Extremely Low! Humidify";
        } else if (h < 30.0f) {
            hum_sev = STATUS_ORANGE;
            hum_msg = "Humidity Very Low - Use humidifier";
        } else if (h < 40.0f) {
            hum_sev = STATUS_YELLOW;
            hum_msg = "Humidity Low - Consider humidifier";
        } else if (h > 70.0f) {
            hum_sev = STATUS_RED;
            hum_msg = "Humidity Extremely High! Dehumidify";
        } else if (h > 65.0f) {
            hum_sev = STATUS_ORANGE;
            hum_msg = "Humidity Very High - Dehumidify";
        } else if (h > 60.0f) {
            hum_sev = STATUS_YELLOW;
            hum_msg = "Humidity High - Increase airflow";
        }
    }

    if (dp_high && currentData.hum_valid && currentData.humidity > 60.0f) {
        hum_sev = STATUS_NONE;
        hum_msg = nullptr;
    }
    if (dp_low && currentData.hum_valid && currentData.humidity < 40.0f) {
        dp_sev = STATUS_NONE;
        dp_msg = nullptr;
    }

    auto add_msg = [&](StatusSeverity target_sev, StatusSeverity sensor_sev, StatusSensor sensor, const char *text) {
        if (sensor_sev != target_sev || sensor_sev == STATUS_NONE || !text) return;
        if (count >= (sizeof(messages) / sizeof(messages[0]))) return;
        messages[count++] = { text, sensor_sev, sensor };
    };

    auto add_by_severity = [&](StatusSeverity sev) {
        add_msg(sev, nox_sev, STATUS_SENSOR_NOX, nox_msg);
        add_msg(sev, hcho_sev, STATUS_SENSOR_HCHO, hcho_msg);
        add_msg(sev, pm25_sev, STATUS_SENSOR_PM25, pm25_msg);
        add_msg(sev, pm10_sev, STATUS_SENSOR_PM10, pm10_msg);
        add_msg(sev, voc_sev, STATUS_SENSOR_VOC, voc_msg);
        add_msg(sev, co2_sev, STATUS_SENSOR_CO2, co2_msg);
        add_msg(sev, temp_sev, STATUS_SENSOR_TEMP, temp_msg);
        add_msg(sev, hum_sev, STATUS_SENSOR_HUM, hum_msg);
        add_msg(sev, dp_sev, STATUS_SENSOR_DP, dp_msg);
    };

    add_by_severity(STATUS_RED);
    add_by_severity(STATUS_ORANGE);
    add_by_severity(STATUS_YELLOW);

    uint32_t signature = static_cast<uint32_t>(count);
    for (size_t i = 0; i < count; ++i) {
        signature = signature * 131u + (static_cast<uint32_t>(messages[i].sensor) << 2) +
                    static_cast<uint32_t>(messages[i].severity);
    }
    if (signature != status_msg_signature) {
        status_msg_signature = signature;
        status_msg_index = 0;
        status_msg_last_ms = now_ms;
    }

    status_msg_count = static_cast<uint8_t>(count);

    if (count > 1 && (now_ms - status_msg_last_ms) >= STATUS_ROTATE_MS) {
        status_msg_index = static_cast<uint8_t>((status_msg_index + 1) % count);
        status_msg_last_ms = now_ms;
    }
    if (status_msg_index >= count) {
        status_msg_index = 0;
    }

    const char *status_text = nullptr;
    if (!has_valid) {
        status_text = "Initializing";
    } else if (count == 0) {
        status_text = "Fresh Air - All Good";
    } else {
        status_text = messages[status_msg_index].text;
    }

    if (objects.label_status_value) {
        safe_label_set_text(objects.label_status_value, status_text ? status_text : "---");
    }
}

void UiController::update_wifi_ui() {
    bool wifi_enabled = networkManager.isEnabled();
    AuraNetworkManager::WifiState wifi_state = networkManager.state();
    const String &wifi_ssid = networkManager.ssid();
    uint8_t wifi_retry_count = networkManager.retryCount();
    if (objects.label_wifi_status_value) {
        const char *status = "OFF";
        if (wifi_enabled) {
            if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) status = "Connected";
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) status = "AP Mode";
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_OFF &&
                     wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) status = "Error";
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTING ||
                     wifi_state == AuraNetworkManager::WIFI_STATE_OFF) status = "Connecting";
        }
        safe_label_set_text(objects.label_wifi_status_value, status);
    }
    if (objects.container_wifi_status) {
        apply_toggle_style(objects.container_wifi_status);
        if (wifi_enabled && wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            lv_obj_add_state(objects.container_wifi_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_wifi_status, LV_STATE_CHECKED);
        }
    }

    if (objects.label_wifi_ssid_value) {
        String safe_ssid;
        const char *ssid_text = "---";
        if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            ssid_text = WIFI_AP_SSID;
        } else if (wifi_enabled && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        }
        safe_label_set_text(objects.label_wifi_ssid_value, ssid_text);
    }

    if (objects.label_wifi_ip_value) {
        String ip = "---";
        if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            ip = WiFi.localIP().toString();
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            ip = WiFi.softAPIP().toString();
        }
        safe_label_set_text(objects.label_wifi_ip_value, ip.c_str());
    }
    if (objects.qrcode_wifi_portal) {
        if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            static const char kWifiPortalUrl[] = "http://192.168.4.1";
            lv_obj_clear_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
            lv_qrcode_update(objects.qrcode_wifi_portal, kWifiPortalUrl, strlen(kWifiPortalUrl));
        } else {
            lv_obj_add_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (objects.btn_wifi_reconnect) {
        bool can_reconnect = wifi_enabled && !wifi_ssid.isEmpty();
        set_button_enabled(objects.btn_wifi_reconnect, can_reconnect);
    }
    if (objects.btn_wifi_start_ap) {
        set_button_enabled(objects.btn_wifi_start_ap, wifi_enabled);
    }
    sync_wifi_toggle_state();
}

void UiController::update_status_icons() {
    // WiFi icon states: 0=hidden, 1=green, 2=blue, 3=yellow, 4=red
    int new_wifi_state = 0;
    bool wifi_enabled = networkManager.isEnabled();
    AuraNetworkManager::WifiState wifi_state = networkManager.state();
    uint8_t wifi_retry_count = networkManager.retryCount();

    if (!wifi_enabled) {
        new_wifi_state = 0; // hidden
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
        new_wifi_state = 1; // green
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTING) {
        new_wifi_state = 2; // blue
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
        new_wifi_state = 3; // yellow
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_OFF &&
               wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) {
        new_wifi_state = 4; // red
    }

    int main_wifi_state = new_wifi_state;
    if (night_mode && main_wifi_state != 4) {
        main_wifi_state = 0;
    }
    static int wifi_icon_state_main = -1;
    if (main_wifi_state != wifi_icon_state_main) {
        wifi_icon_state_main = main_wifi_state;
        if (objects.wifi_status_icon) {
            if (wifi_icon_state_main == 0) {
                lv_obj_add_flag(objects.wifi_status_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(objects.wifi_status_icon, LV_OBJ_FLAG_HIDDEN);
                if (wifi_icon_state_main == 1) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_green);
                } else if (wifi_icon_state_main == 2) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_blue);
                } else if (wifi_icon_state_main == 3) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_yellow);
                } else if (wifi_icon_state_main == 4) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_red);
                }
            }
        }
    }

    if (new_wifi_state != wifi_icon_state) {
        wifi_icon_state = new_wifi_state;

        lv_obj_t *wifi_icons[] = {
            objects.wifi_status_icon_1,
            objects.wifi_status_icon_2,
            objects.wifi_status_icon_3
        };
        const size_t wifi_icon_count = sizeof(wifi_icons) / sizeof(wifi_icons[0]);
        for (size_t i = 0; i < wifi_icon_count; i++) {
            if (wifi_icons[i]) {
                if (wifi_icon_state == 0) {
                    lv_obj_add_flag(wifi_icons[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_clear_flag(wifi_icons[i], LV_OBJ_FLAG_HIDDEN);
                    if (wifi_icon_state == 1) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_green);
                    } else if (wifi_icon_state == 2) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_blue);
                    } else if (wifi_icon_state == 3) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_yellow);
                    } else if (wifi_icon_state == 4) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_red);
                    }
                }
            }
        }
    }

    // MQTT icon states: 0=hidden, 1=green, 2=blue, 3=red, 4=yellow
    int new_mqtt_state = 0;

    if (!mqttManager.isEnabled() || !wifi_enabled ||
        wifi_state != AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
        new_mqtt_state = 0; // hidden - MQTT disabled or WiFi not ready
    } else if (mqttManager.isConnected()) {
        new_mqtt_state = 1; // green
    } else {
        uint32_t attempts = mqttManager.connectAttempts();
        const uint32_t stage_limit = static_cast<uint32_t>(MQTT_CONNECT_MAX_FAILS);
        if (mqttManager.retryExhausted() || attempts >= stage_limit * 2) {
            new_mqtt_state = 3; // red
        } else if (attempts >= stage_limit) {
            new_mqtt_state = 4; // yellow
        } else {
            new_mqtt_state = 2; // blue
        }
    }

    int main_mqtt_state = new_mqtt_state;
    if (night_mode && main_mqtt_state != 3) {
        main_mqtt_state = 0;
    }
    static int mqtt_icon_state_main = -1;
    if (main_mqtt_state != mqtt_icon_state_main) {
        mqtt_icon_state_main = main_mqtt_state;
        if (objects.mqtt_status_icon) {
            if (mqtt_icon_state_main == 0) {
                lv_obj_add_flag(objects.mqtt_status_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(objects.mqtt_status_icon, LV_OBJ_FLAG_HIDDEN);
                if (mqtt_icon_state_main == 1) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_green);
                } else if (mqtt_icon_state_main == 2) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_blue);
                } else if (mqtt_icon_state_main == 3) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_red);
                } else if (mqtt_icon_state_main == 4) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_yellow);
                }
            }
        }
    }

    if (new_mqtt_state != mqtt_icon_state) {
        mqtt_icon_state = new_mqtt_state;

        lv_obj_t *mqtt_icons[] = {
            objects.mqtt_status_icon_1,
            objects.mqtt_status_icon_2,
            objects.mqtt_status_icon_3
        };
        const size_t mqtt_icon_count = sizeof(mqtt_icons) / sizeof(mqtt_icons[0]);
        for (size_t i = 0; i < mqtt_icon_count; i++) {
            if (mqtt_icons[i]) {
                if (mqtt_icon_state == 0) {
                    lv_obj_add_flag(mqtt_icons[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_clear_flag(mqtt_icons[i], LV_OBJ_FLAG_HIDDEN);
                    if (mqtt_icon_state == 1) {
                        lv_img_set_src(mqtt_icons[i], &img_home_green);
                    } else if (mqtt_icon_state == 2) {
                        lv_img_set_src(mqtt_icons[i], &img_home_blue);
                    } else if (mqtt_icon_state == 3) {
                        lv_img_set_src(mqtt_icons[i], &img_home_red);
                    } else if (mqtt_icon_state == 4) {
                        lv_img_set_src(mqtt_icons[i], &img_home_yellow);
                    }
                }
            }
        }
    }
}

void UiController::update_mqtt_ui() {
    bool wifi_ready = networkManager.isEnabled() && networkManager.isConnected();

    // Update MQTT status label
    if (objects.label_mqtt_status_value) {
        const char *status = "Disabled";
        if (mqttManager.isUserEnabled()) {
            if (!wifi_ready) {
                status = "No WiFi";
            } else if (mqttManager.isConnected()) {
                status = "Connected";
            } else {
                uint32_t attempts = mqttManager.connectAttempts();
                const uint32_t stage_limit = static_cast<uint32_t>(MQTT_CONNECT_MAX_FAILS);
                if (mqttManager.retryExhausted()) {
                    status = "Error";
                } else if (attempts >= stage_limit * 2) {
                    status = "Retrying (1h)";
                } else if (attempts >= stage_limit) {
                    status = "Retrying (10m)";
                } else {
                    status = "Connecting...";
                }
            }
        }
        safe_label_set_text(objects.label_mqtt_status_value, status);
    }

    // Update MQTT status container style
    if (objects.container_mqtt_status) {
        apply_toggle_style(objects.container_mqtt_status);
        if (mqttManager.isEnabled() && mqttManager.isConnected()) {
            lv_obj_add_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        }
    }

    // Update Broker IP
    if (objects.label_mqtt_broker_value) {
        String broker_addr = "---";
        if (mqttManager.isUserEnabled() && !mqttManager.host().isEmpty()) {
            broker_addr = mqttManager.host() + ":" + String(mqttManager.port());
        }
        safe_label_set_text(objects.label_mqtt_broker_value, broker_addr.c_str());
    }

    // Update Device IP
    if (objects.label_mqtt_device_ip_value) {
        String device_ip = "---";
        if (networkManager.isConnected()) {
            device_ip = WiFi.localIP().toString();
        }
        safe_label_set_text(objects.label_mqtt_device_ip_value, device_ip.c_str());
    }

    // Update Topic
    if (objects.label_mqtt_topic_value) {
        String topic = "---";
        if (mqttManager.isUserEnabled() && !mqttManager.baseTopic().isEmpty()) {
            topic = mqttManager.baseTopic();
        }
        safe_label_set_text(objects.label_mqtt_topic_value, topic.c_str());
    }

    // Update QR code - show only when WiFi is connected.
    if (objects.qrcode_mqtt_portal) {
        if (wifi_ready) {
            String mqtt_url = "http://aura.local/mqtt";
            lv_obj_clear_flag(objects.qrcode_mqtt_portal, LV_OBJ_FLAG_HIDDEN);
            lv_qrcode_update(objects.qrcode_mqtt_portal, mqtt_url.c_str(), mqtt_url.length());
        } else {
            lv_obj_add_flag(objects.qrcode_mqtt_portal, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update toggle button text and state
    if (objects.label_btn_mqtt_toggle) {
        safe_label_set_text(objects.label_btn_mqtt_toggle, "ON / OFF");
    }
    sync_mqtt_toggle_state();
    set_button_enabled(objects.btn_mqtt_toggle, wifi_ready);
    set_button_enabled(objects.btn_mqtt, wifi_ready);

    // Update reconnect button state
    if (objects.btn_mqtt_reconnect) {
        bool can_reconnect = mqttManager.isEnabled() && wifi_ready;
        if (can_reconnect) {
            lv_obj_clear_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        }
    }
}

void UiController::init_ui_defaults() {
    if (objects.co2_bar_mask) {
        lv_obj_clear_flag(objects.co2_bar_mask, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_clear_flag(objects.co2_bar_mask, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (objects.wifi_status_icon) lv_obj_add_flag(objects.wifi_status_icon, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_1) lv_obj_add_flag(objects.wifi_status_icon_1, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_2) lv_obj_add_flag(objects.wifi_status_icon_2, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_3) lv_obj_add_flag(objects.wifi_status_icon_3, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon) lv_obj_add_flag(objects.mqtt_status_icon, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_1) lv_obj_add_flag(objects.mqtt_status_icon_1, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_2) lv_obj_add_flag(objects.mqtt_status_icon_2, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_3) lv_obj_add_flag(objects.mqtt_status_icon_3, LV_OBJ_FLAG_HIDDEN);

    if (objects.btn_mqtt) {
        lv_obj_set_style_bg_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.label_btn_mqtt) {
        lv_obj_set_style_text_color(objects.label_btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.btn_night_mode) {
        lv_obj_set_style_bg_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.label_btn_night_mode) {
        lv_obj_set_style_text_color(objects.label_btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }

    update_clock_labels();
    timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    update_datetime_ui();
    backlightManager.updateUi();
    nightModeManager.updateUi();
    update_led_indicators();
    update_temp_offset_label();
    update_hum_offset_label();
    update_wifi_ui();
    update_mqtt_ui();
    update_ui();
    confirm_hide();
}
