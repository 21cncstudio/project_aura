#pragma once

#include <Arduino.h>
#include "config/AppData.h"
#include <lvgl.h>
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"

class StorageManager;
class AuraNetworkManager;
class MqttManager;
class ThemeManager;
class BacklightManager;
class NightModeManager;

struct UiContext {
    StorageManager &storage;
    AuraNetworkManager &networkManager;
    MqttManager &mqttManager;
    SensorManager &sensorManager;
    TimeManager &timeManager;
    ThemeManager &themeManager;
    BacklightManager &backlightManager;
    NightModeManager &nightModeManager;
    SensorData &currentData;
    bool &night_mode;
    bool &temp_units_c;
    bool &led_indicators_enabled;
    bool &alert_blink_enabled;
    bool &co2_asc_enabled;
    float &temp_offset;
    float &hum_offset;
};

class UiController {
public:
    explicit UiController(const UiContext &context);

    void setLvglReady(bool ready);
    void begin();
    void onSensorPoll(const SensorManager::PollResult &poll);
    void onTimePoll(const TimeManager::PollResult &poll);
    void markDatetimeDirty();
    void apply_auto_night_now();
    void mqtt_sync_with_wifi();
    void poll(uint32_t now_ms);

private:
    enum ConfirmAction {
        CONFIRM_NONE = 0,
        CONFIRM_VOC_RESET,
        CONFIRM_RESTART,
        CONFIRM_FACTORY_RESET,
    };

    void update_temp_offset_label();
    void update_hum_offset_label();
    void update_wifi_ui();
    void update_mqtt_ui();
    void update_status_icons();
    void update_ui();
    void update_settings_header();
    void update_theme_custom_info(bool presets);
    void update_status_message(uint32_t now_ms, bool gas_warmup);
    void update_clock_labels();
    void update_datetime_ui();
    void update_boot_diag(uint32_t now_ms);
    bool boot_diag_has_errors(uint32_t now_ms);
    void update_led_indicators();
    void update_co2_bar(int co2, bool valid);
    void init_ui_defaults();

    void safe_label_set_text(lv_obj_t *obj, const char *new_text);
    lv_color_t color_inactive();
    lv_color_t color_green();
    lv_color_t color_yellow();
    lv_color_t color_orange();
    lv_color_t color_red();
    lv_color_t color_blue();
    lv_color_t color_card_border();
    lv_color_t getTempColor(float t);
    lv_color_t getHumidityColor(float h);
    float compute_dew_point_c(float temp_c, float rh);
    lv_color_t getDewPointColor(float dew_c);
    lv_color_t getCO2Color(int co2);
    lv_color_t getPM25Color(float pm);
    lv_color_t getPM10Color(float pm);
    lv_color_t getPressureDeltaColor(float delta, bool valid, bool is24h);
    lv_color_t getVOCColor(int voc);
    lv_color_t getNOxColor(int nox);
    lv_color_t getHCHOColor(float hcho_ppb, bool valid);
    AirQuality getAirQuality(const SensorData &data);
    lv_color_t blink_red(lv_color_t color);
    lv_color_t night_alert_color(lv_color_t color);
    lv_color_t alert_color_for_mode(lv_color_t color);
    void compute_header_style(const AirQuality &aq, lv_color_t &color, lv_opa_t &shadow_opa);
    lv_color_t active_text_color();

    void sync_wifi_toggle_state();
    void sync_mqtt_toggle_state();
    void sync_alert_blink_toggle_state();
    void sync_auto_dim_button_state();
    void sync_night_mode_toggle_ui();

    void apply_toggle_style(lv_obj_t *btn);
    void set_dot_color(lv_obj_t *obj, lv_color_t color);
    void set_chip_color(lv_obj_t *obj, lv_color_t color);
    void set_button_enabled(lv_obj_t *btn, bool enabled);
    void confirm_set_visible(lv_obj_t *obj, bool visible);
    void confirm_show(ConfirmAction action);
    void confirm_hide();

    void night_mode_on_enter();
    void night_mode_on_exit();
    void set_night_mode_state(bool enabled, bool save_pref);

    void mqtt_apply_pending();

    void on_settings_event(lv_event_t *e);
    void on_back_event(lv_event_t *e);
    void on_wifi_settings_event(lv_event_t *e);
    void on_wifi_back_event(lv_event_t *e);
    void on_mqtt_settings_event(lv_event_t *e);
    void on_mqtt_back_event(lv_event_t *e);
    void on_theme_color_event(lv_event_t *e);
    void on_theme_back_event(lv_event_t *e);
    void on_theme_tab_event(lv_event_t *e);
    void on_theme_swatch_event(lv_event_t *e);
    void on_wifi_toggle_event(lv_event_t *e);
    void on_mqtt_toggle_event(lv_event_t *e);
    void on_mqtt_reconnect_event(lv_event_t *e);
    void on_wifi_reconnect_event(lv_event_t *e);
    void on_wifi_start_ap_event(lv_event_t *e);
    void on_wifi_forget_event(lv_event_t *e);
    void on_head_status_event(lv_event_t *e);
    void on_auto_night_settings_event(lv_event_t *e);
    void on_auto_night_back_event(lv_event_t *e);
    void on_auto_night_toggle_event(lv_event_t *e);
    void on_auto_night_start_hours_minus_event(lv_event_t *e);
    void on_auto_night_start_hours_plus_event(lv_event_t *e);
    void on_auto_night_start_minutes_minus_event(lv_event_t *e);
    void on_auto_night_start_minutes_plus_event(lv_event_t *e);
    void on_auto_night_end_hours_minus_event(lv_event_t *e);
    void on_auto_night_end_hours_plus_event(lv_event_t *e);
    void on_auto_night_end_minutes_minus_event(lv_event_t *e);
    void on_auto_night_end_minutes_plus_event(lv_event_t *e);
    void on_confirm_ok_event(lv_event_t *e);
    void on_confirm_cancel_event(lv_event_t *e);
    void on_night_mode_event(lv_event_t *e);
    void on_units_c_f_event(lv_event_t *e);
    void on_led_indicators_event(lv_event_t *e);
    void on_alert_blink_event(lv_event_t *e);
    void on_co2_calib_event(lv_event_t *e);
    void on_co2_calib_back_event(lv_event_t *e);
    void on_co2_calib_asc_event(lv_event_t *e);
    void on_co2_calib_start_event(lv_event_t *e);
    void on_time_date_event(lv_event_t *e);
    void on_backlight_settings_event(lv_event_t *e);
    void on_backlight_back_event(lv_event_t *e);
    void on_backlight_schedule_toggle_event(lv_event_t *e);
    void on_backlight_preset_always_on_event(lv_event_t *e);
    void on_backlight_preset_30s_event(lv_event_t *e);
    void on_backlight_preset_1m_event(lv_event_t *e);
    void on_backlight_preset_5m_event(lv_event_t *e);
    void on_backlight_sleep_hours_minus_event(lv_event_t *e);
    void on_backlight_sleep_hours_plus_event(lv_event_t *e);
    void on_backlight_sleep_minutes_minus_event(lv_event_t *e);
    void on_backlight_sleep_minutes_plus_event(lv_event_t *e);
    void on_backlight_wake_hours_minus_event(lv_event_t *e);
    void on_backlight_wake_hours_plus_event(lv_event_t *e);
    void on_backlight_wake_minutes_minus_event(lv_event_t *e);
    void on_backlight_wake_minutes_plus_event(lv_event_t *e);
    void on_datetime_back_event(lv_event_t *e);
    void on_datetime_apply_event(lv_event_t *e);
    void on_ntp_toggle_event(lv_event_t *e);
    void on_tz_plus_event(lv_event_t *e);
    void on_tz_minus_event(lv_event_t *e);
    void on_set_time_hours_minus_event(lv_event_t *e);
    void on_set_time_hours_plus_event(lv_event_t *e);
    void on_set_time_minutes_minus_event(lv_event_t *e);
    void on_set_time_minutes_plus_event(lv_event_t *e);
    void on_set_date_day_minus_event(lv_event_t *e);
    void on_set_date_day_plus_event(lv_event_t *e);
    void on_set_date_month_minus_event(lv_event_t *e);
    void on_set_date_month_plus_event(lv_event_t *e);
    void on_set_date_year_minus_event(lv_event_t *e);
    void on_set_date_year_plus_event(lv_event_t *e);
    void on_restart_event(lv_event_t *e);
    void on_factory_reset_event(lv_event_t *e);
    void on_voc_reset_event(lv_event_t *e);
    void on_temp_offset_minus(lv_event_t *e);
    void on_temp_offset_plus(lv_event_t *e);
    void on_hum_offset_minus(lv_event_t *e);
    void on_hum_offset_plus(lv_event_t *e);
    void on_boot_diag_continue(lv_event_t *e);

    static void on_settings_event_cb(lv_event_t *e);
    static void on_back_event_cb(lv_event_t *e);
    static void on_wifi_settings_event_cb(lv_event_t *e);
    static void on_wifi_back_event_cb(lv_event_t *e);
    static void on_mqtt_settings_event_cb(lv_event_t *e);
    static void on_mqtt_back_event_cb(lv_event_t *e);
    static void on_theme_color_event_cb(lv_event_t *e);
    static void on_theme_back_event_cb(lv_event_t *e);
    static void on_theme_tab_event_cb(lv_event_t *e);
    static void on_theme_swatch_event_cb(lv_event_t *e);
    static void on_wifi_toggle_event_cb(lv_event_t *e);
    static void on_mqtt_toggle_event_cb(lv_event_t *e);
    static void on_mqtt_reconnect_event_cb(lv_event_t *e);
    static void on_wifi_reconnect_event_cb(lv_event_t *e);
    static void on_wifi_start_ap_event_cb(lv_event_t *e);
    static void on_wifi_forget_event_cb(lv_event_t *e);
    static void on_head_status_event_cb(lv_event_t *e);
    static void on_auto_night_settings_event_cb(lv_event_t *e);
    static void on_auto_night_back_event_cb(lv_event_t *e);
    static void on_auto_night_toggle_event_cb(lv_event_t *e);
    static void on_auto_night_start_hours_minus_event_cb(lv_event_t *e);
    static void on_auto_night_start_hours_plus_event_cb(lv_event_t *e);
    static void on_auto_night_start_minutes_minus_event_cb(lv_event_t *e);
    static void on_auto_night_start_minutes_plus_event_cb(lv_event_t *e);
    static void on_auto_night_end_hours_minus_event_cb(lv_event_t *e);
    static void on_auto_night_end_hours_plus_event_cb(lv_event_t *e);
    static void on_auto_night_end_minutes_minus_event_cb(lv_event_t *e);
    static void on_auto_night_end_minutes_plus_event_cb(lv_event_t *e);
    static void on_confirm_ok_event_cb(lv_event_t *e);
    static void on_confirm_cancel_event_cb(lv_event_t *e);
    static void on_night_mode_event_cb(lv_event_t *e);
    static void on_units_c_f_event_cb(lv_event_t *e);
    static void on_led_indicators_event_cb(lv_event_t *e);
    static void on_alert_blink_event_cb(lv_event_t *e);
    static void on_co2_calib_event_cb(lv_event_t *e);
    static void on_co2_calib_back_event_cb(lv_event_t *e);
    static void on_co2_calib_asc_event_cb(lv_event_t *e);
    static void on_co2_calib_start_event_cb(lv_event_t *e);
    static void on_time_date_event_cb(lv_event_t *e);
    static void on_backlight_settings_event_cb(lv_event_t *e);
    static void on_backlight_back_event_cb(lv_event_t *e);
    static void on_backlight_schedule_toggle_event_cb(lv_event_t *e);
    static void on_backlight_preset_always_on_event_cb(lv_event_t *e);
    static void on_backlight_preset_30s_event_cb(lv_event_t *e);
    static void on_backlight_preset_1m_event_cb(lv_event_t *e);
    static void on_backlight_preset_5m_event_cb(lv_event_t *e);
    static void on_backlight_sleep_hours_minus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_hours_plus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_minutes_minus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_minutes_plus_event_cb(lv_event_t *e);
    static void on_backlight_wake_hours_minus_event_cb(lv_event_t *e);
    static void on_backlight_wake_hours_plus_event_cb(lv_event_t *e);
    static void on_backlight_wake_minutes_minus_event_cb(lv_event_t *e);
    static void on_backlight_wake_minutes_plus_event_cb(lv_event_t *e);
    static void on_datetime_back_event_cb(lv_event_t *e);
    static void on_datetime_apply_event_cb(lv_event_t *e);
    static void on_ntp_toggle_event_cb(lv_event_t *e);
    static void on_tz_plus_event_cb(lv_event_t *e);
    static void on_tz_minus_event_cb(lv_event_t *e);
    static void on_set_time_hours_minus_event_cb(lv_event_t *e);
    static void on_set_time_hours_plus_event_cb(lv_event_t *e);
    static void on_set_time_minutes_minus_event_cb(lv_event_t *e);
    static void on_set_time_minutes_plus_event_cb(lv_event_t *e);
    static void on_set_date_day_minus_event_cb(lv_event_t *e);
    static void on_set_date_day_plus_event_cb(lv_event_t *e);
    static void on_set_date_month_minus_event_cb(lv_event_t *e);
    static void on_set_date_month_plus_event_cb(lv_event_t *e);
    static void on_set_date_year_minus_event_cb(lv_event_t *e);
    static void on_set_date_year_plus_event_cb(lv_event_t *e);
    static void on_restart_event_cb(lv_event_t *e);
    static void on_factory_reset_event_cb(lv_event_t *e);
    static void on_voc_reset_event_cb(lv_event_t *e);
    static void on_temp_offset_minus_cb(lv_event_t *e);
    static void on_temp_offset_plus_cb(lv_event_t *e);
    static void on_hum_offset_minus_cb(lv_event_t *e);
    static void on_hum_offset_plus_cb(lv_event_t *e);
    static void on_boot_diag_continue_cb(lv_event_t *e);
    static void apply_toggle_style_cb(lv_obj_t *btn);
    static void mqtt_sync_with_wifi_cb();

    static UiController *instance_;

    StorageManager &storage;
    AuraNetworkManager &networkManager;
    MqttManager &mqttManager;
    SensorManager &sensorManager;
    TimeManager &timeManager;
    ThemeManager &themeManager;
    BacklightManager &backlightManager;
    NightModeManager &nightModeManager;
    SensorData &currentData;

    bool &night_mode;
    bool &temp_units_c;
    bool &led_indicators_enabled;
    bool &alert_blink_enabled;
    bool &co2_asc_enabled;
    float &temp_offset;
    float &hum_offset;

    bool data_dirty = true;
    bool lvgl_ready = false;
    int pending_screen_id = 0;
    int current_screen_id = 0;
    bool header_status_enabled = true;
    int wifi_icon_state = -1;
    int mqtt_icon_state = -1;
    bool clock_ui_dirty = true;
    bool datetime_ui_dirty = true;
    bool ntp_toggle_syncing = false;
    ConfirmAction confirm_action = CONFIRM_NONE;
    uint32_t last_clock_tick_ms = 0;
    int set_hour = 0;
    int set_minute = 0;
    int set_day = 1;
    int set_month = 1;
    int set_year = 2024;
    bool datetime_changed = false;
    bool alert_blink_syncing = false;
    bool alert_blink_before_night = true;
    bool night_blink_restore_pending = false;
    bool night_blink_user_changed = false;
    float temp_offset_saved = 0.0f;
    bool temp_offset_dirty = false;
    bool temp_offset_ui_dirty = false;
    float hum_offset_saved = 0.0f;
    bool hum_offset_dirty = false;
    bool hum_offset_ui_dirty = false;
    bool blink_state = true;
    uint32_t last_blink_ms = 0;
    uint32_t last_ui_update_ms = 0;
    uint32_t last_ui_tick_ms = 0;
    uint32_t status_msg_last_ms = 0;
    uint32_t status_msg_signature = 0;
    uint8_t status_msg_index = 0;
    uint8_t status_msg_count = 0;
    bool boot_logo_active = false;
    uint32_t boot_logo_start_ms = 0;
    bool boot_diag_active = false;
    bool boot_diag_has_error = false;
    uint32_t boot_diag_start_ms = 0;
    uint32_t last_boot_diag_update_ms = 0;
};
