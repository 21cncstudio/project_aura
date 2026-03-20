// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include "config/AppData.h"
#include "config/AppConfig.h"
#include "core/ConnectivityRuntime.h"
#include "core/MqttRuntimeState.h"
#include "core/NetworkCommandQueue.h"
#include "ui/UiDeferredUnload.h"
#include "web/WebUiBridge.h"
#include <lvgl.h>
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"

class StorageManager;
class AuraNetworkManager;
class MqttManager;
class ChartsHistory;
class ThemeManager;
class BacklightManager;
class NightModeManager;
class FanControl;
class UiEventBinder;
class UiBootFlow;
class UiLocalization;
class UiScreenFlow;
class UiRenderLoop;

struct UiContext {
    StorageManager &storage;
    AuraNetworkManager &networkManager;
    MqttManager &mqttManager;
    ConnectivityRuntime &connectivityRuntime;
    MqttRuntimeState &mqttRuntimeState;
    WebUiBridge &webUiBridge;
    NetworkCommandQueue &networkCommandQueue;
    SensorManager &sensorManager;
    ChartsHistory &chartsHistory;
    TimeManager &timeManager;
    ThemeManager &themeManager;
    BacklightManager &backlightManager;
    NightModeManager &nightModeManager;
    FanControl &fanControl;
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
    void markWebPagePanelDirty();
    void apply_auto_night_now();
    void mqtt_sync_with_wifi();
    void poll(uint32_t now_ms);

    bool webNightModeEnabled() const { return night_mode; }
    bool webNightModeLocked() const;
    bool webBacklightOn() const;
    bool webUnitsC() const { return temp_units_c; }
    float webTempOffset() const { return temp_offset; }
    float webHumOffset() const { return hum_offset; }

    bool webSetNightMode(bool enabled);
    bool webSetBacklight(bool enabled);
    bool webSetUnitsC(bool units_c);
    bool webSetOffsets(float temp_offset_c, float hum_offset_pct);
    void webSetFirmwareUpdateScreen(bool active);
    void webRequestRestart();

private:
    friend class UiEventBinder;
    friend class UiBootFlow;
    friend class UiLocalization;
    friend class UiScreenFlow;
    friend class UiRenderLoop;

    enum ConfirmAction {
        CONFIRM_NONE = 0,
        CONFIRM_VOC_RESET,
        CONFIRM_RESTART,
        CONFIRM_FACTORY_RESET,
    };
    enum InfoSensor {
        INFO_NONE = 0,
        INFO_TEMP,
        INFO_VOC,
        INFO_NOX,
        INFO_HCHO,
        INFO_CO2,
        INFO_RH,
        INFO_AH,
        INFO_MR,
        INFO_DP,
        INFO_PM05,
        INFO_PM25,
        INFO_PM4,
        INFO_PM10,
        INFO_PM1,
        INFO_CO,
        INFO_PRESSURE_3H,
        INFO_PRESSURE_24H,
    };
    enum TempGraphRange {
        TEMP_GRAPH_RANGE_1H = 0,
        TEMP_GRAPH_RANGE_3H,
        TEMP_GRAPH_RANGE_24H,
    };
    enum GraphZoneTone : uint8_t {
        GRAPH_ZONE_NONE = 0,
        GRAPH_ZONE_RED,
        GRAPH_ZONE_ORANGE,
        GRAPH_ZONE_YELLOW,
        GRAPH_ZONE_GREEN,
        GRAPH_ZONE_BLUE,
    };
    static constexpr uint8_t kMaxGraphZoneBands = 7;
    static constexpr uint8_t kMaxGraphZoneBounds = kMaxGraphZoneBands + 1;
    struct SensorGraphProfile {
        float min_span = 1.0f;
        float fallback_value = 0.0f;
        uint8_t vertical_divisions = 15;
        uint8_t horizontal_divisions_min = 3;
        uint8_t horizontal_divisions_max = 12;
        const char *unit = nullptr;
        const char *label_min = "MIN";
        const char *label_now = "NOW";
        const char *label_max = "MAX";
        uint8_t zone_count = 0;
        float zone_bounds[kMaxGraphZoneBounds] = {};
        GraphZoneTone zone_tones[kMaxGraphZoneBands] = {};
    };
    struct GraphSeriesStats {
        bool has_values = false;
        float min_value = 0.0f;
        float max_value = 0.0f;
        float latest_value = 0.0f;
    };
    void update_temp_offset_label();
    void update_hum_offset_label();
    void update_wifi_ui();
    void update_mqtt_ui();
    void update_status_icons();
    void update_ui();
    void refreshConnectivitySnapshot();
    void syncConnectivityRuntime();
    void publishWebUiSnapshot();
    void processWebUiBridgeCommands();
    WebUiBridge::Snapshot buildWebUiSnapshot() const;
    bool consumeNetworkUiDirty();
    bool consumeMqttUiDirty();
    void applyPendingWifiEnabled();
    void setMqttScreenOpenState(bool open);
    void setThemeScreenOpenState(bool open);
    void setWifiEnabledFromUi(bool enabled);
    void setMqttUserEnabledFromUi(bool enabled);
    void requestMqttReconnectFromUi();
    void requestWifiReconnectFromUi();
    void toggleWifiApModeFromUi();
    void clearWifiCredentialsFromUi();
    void markWifiUiDirty();
    void update_sensor_info_ui();
    void restore_sensor_info_selection();
    void refresh_texts_for_screen(int screen_id);
    void select_humidity_info(InfoSensor sensor);
    void select_pm_info(InfoSensor sensor);
    void select_pressure_info(InfoSensor sensor);
    void sync_info_graph_button_state();
    bool should_show_threshold_dots() const;
    void sync_threshold_dots_visibility();
    void set_temperature_info_mode(bool graph_mode);
    void set_rh_info_mode(bool graph_mode);
    void set_voc_info_mode(bool graph_mode);
    void set_nox_info_mode(bool graph_mode);
    void set_hcho_info_mode(bool graph_mode);
    void set_co2_info_mode(bool graph_mode);
    void set_co_info_mode(bool graph_mode);
    void set_pm05_info_mode(bool graph_mode);
    void set_pm25_4_info_mode(bool graph_mode);
    void set_pm1_10_info_mode(bool graph_mode);
    void set_pressure_info_mode(bool graph_mode);
    uint16_t graph_points_for_range(TempGraphRange range) const;
    uint8_t graph_vertical_divisions_for_range(TempGraphRange range) const;
    void apply_standard_info_chart_theme(lv_obj_t *chart, uint8_t horizontal_divisions, uint8_t vertical_divisions);
    lv_chart_series_t *ensure_info_chart_series(lv_obj_t *chart, uint16_t points);
    GraphSeriesStats populate_info_chart_series(lv_obj_t *chart,
                                                lv_chart_series_t *series,
                                                uint16_t points,
                                                int metric_id,
                                                float point_scale,
                                                bool require_non_negative,
                                                bool convert_temperature_to_display = false);
    uint16_t humidity_graph_points() const;
    uint16_t voc_graph_points() const;
    uint16_t nox_graph_points() const;
    uint16_t hcho_graph_points() const;
    uint16_t co2_graph_points() const;
    uint16_t co_graph_points() const;
    uint16_t pm05_graph_points() const;
    uint16_t pm25_4_graph_points() const;
    uint16_t pm1_10_graph_points() const;
    uint16_t pressure_graph_points() const;
    SensorGraphProfile build_temperature_graph_profile() const;
    lv_color_t resolve_graph_zone_color(GraphZoneTone tone, lv_color_t chart_bg);
    void apply_temperature_graph_theme(const SensorGraphProfile &profile);
    void update_temperature_info_graph();
    void update_humidity_info_graph();
    void update_voc_info_graph();
    void update_nox_info_graph();
    void update_hcho_info_graph();
    void update_co2_info_graph();
    void update_co_info_graph();
    void update_pm05_info_graph();
    void update_pm25_4_info_graph();
    void update_pm1_10_info_graph();
    void update_pressure_info_graph();
    void ensure_temperature_graph_overlays();
    void update_temperature_graph_overlays(const SensorGraphProfile &profile,
                                           bool has_values,
                                           float min_temp,
                                           float max_temp,
                                           float latest_temp);
    void ensure_temperature_zone_overlay();
    void update_temperature_zone_overlay(const SensorGraphProfile &profile, float y_min_display, float y_max_display);
    void ensure_graph_time_labels(lv_obj_t *graph_container, lv_obj_t *chart, lv_obj_t **labels, uint8_t label_count);
    void update_graph_time_labels(lv_obj_t *graph_container,
                                  lv_obj_t *chart,
                                  lv_obj_t **labels,
                                  uint8_t label_count,
                                  uint16_t points,
                                  bool clear_when_points_lt_two = false,
                                  bool chart_layout_before_position = false,
                                  bool move_foreground_after_position = true);
    void ensure_graph_stat_overlays(lv_obj_t *chart, lv_obj_t *&label_min, lv_obj_t *&label_now, lv_obj_t *&label_max);
    void style_graph_stat_overlays(lv_obj_t *chart, lv_obj_t *label_min, lv_obj_t *label_now, lv_obj_t *label_max);
    void ensure_temperature_time_labels();
    void update_temperature_time_labels();
    void ensure_humidity_graph_overlays();
    void update_humidity_graph_overlays(bool has_values,
                                        float min_humidity,
                                        float max_humidity,
                                        float latest_humidity);
    void ensure_humidity_zone_overlay();
    void update_humidity_zone_overlay(float y_min_display, float y_max_display);
    void ensure_humidity_time_labels();
    void update_humidity_time_labels();
    void ensure_voc_graph_overlays();
    void update_voc_graph_overlays(bool has_values,
                                   float min_voc,
                                   float max_voc,
                                   float latest_voc);
    void ensure_voc_zone_overlay();
    void update_voc_zone_overlay(float y_min_display, float y_max_display);
    void ensure_voc_time_labels();
    void update_voc_time_labels();
    void ensure_nox_graph_overlays();
    void update_nox_graph_overlays(bool has_values,
                                   float min_nox,
                                   float max_nox,
                                   float latest_nox);
    void ensure_nox_zone_overlay();
    void update_nox_zone_overlay(float y_min_display, float y_max_display);
    void ensure_nox_time_labels();
    void update_nox_time_labels();
    void ensure_hcho_graph_overlays();
    void update_hcho_graph_overlays(bool has_values,
                                    float min_hcho,
                                    float max_hcho,
                                    float latest_hcho);
    void ensure_hcho_zone_overlay();
    void update_hcho_zone_overlay(float y_min_display, float y_max_display);
    void ensure_hcho_time_labels();
    void update_hcho_time_labels();
    void ensure_co2_graph_overlays();
    void update_co2_graph_overlays(bool has_values,
                                   float min_co2,
                                   float max_co2,
                                   float latest_co2);
    void ensure_co2_zone_overlay();
    void update_co2_zone_overlay(float y_min_display, float y_max_display);
    void ensure_co2_time_labels();
    void update_co2_time_labels();
    void ensure_co_graph_overlays();
    void update_co_graph_overlays(bool has_values,
                                  float min_co,
                                  float max_co,
                                  float latest_co);
    void ensure_co_zone_overlay();
    void update_co_zone_overlay(float y_min_display, float y_max_display);
    void ensure_co_time_labels();
    void update_co_time_labels();
    void ensure_pm05_graph_overlays();
    void update_pm05_graph_overlays(bool has_values,
                                    float min_value,
                                    float max_value,
                                    float latest_value);
    void ensure_pm05_zone_overlay();
    void update_pm05_zone_overlay(float y_min_display, float y_max_display);
    void ensure_pm05_time_labels();
    void update_pm05_time_labels();
    void ensure_pm25_4_graph_overlays();
    void update_pm25_4_graph_overlays(bool has_values,
                                      float min_value,
                                      float max_value,
                                      float latest_value);
    void ensure_pm25_4_zone_overlay();
    void update_pm25_4_zone_overlay(float y_min_display, float y_max_display);
    void ensure_pm25_4_time_labels();
    void update_pm25_4_time_labels();
    void ensure_pm1_10_graph_overlays();
    void update_pm1_10_graph_overlays(bool has_values,
                                      float min_value,
                                      float max_value,
                                      float latest_value);
    void ensure_pm1_10_zone_overlay();
    void update_pm1_10_zone_overlay(float y_min_display, float y_max_display);
    void ensure_pm1_10_time_labels();
    void update_pm1_10_time_labels();
    void ensure_pressure_graph_overlays();
    void update_pressure_graph_overlays(bool has_values,
                                        float min_pressure,
                                        float max_pressure,
                                        float latest_pressure);
    void ensure_pressure_time_labels();
    void update_pressure_time_labels();
    void release_all_sensor_graph_runtime_objects();
    bool should_refresh_active_graph(InfoSensor sensor, TempGraphRange range, uint16_t points);
    void mark_active_graph_refreshed(InfoSensor sensor, TempGraphRange range, uint16_t points);
    void invalidate_active_graph_refresh_cache();
    uint32_t active_graph_theme_signature();
    uint16_t temperature_graph_points() const;
    void update_sensor_cards(const AirQuality &aq, bool gas_warmup, bool show_co2_bar);
    void update_settings_header();
    void update_theme_custom_info(bool presets);
    void update_web_page_panel();
    void update_status_message(uint32_t now_ms, bool gas_warmup);
    void update_diag_log_ui();
    void update_clock_labels();
    float pressure_to_display(float pressure_hpa) const;
    float pressure_delta_to_display(float pressure_delta_hpa) const;
    const char *pressure_display_unit() const;
    bool pressure_display_uses_inhg() const { return !temp_units_c; }
    void update_datetime_ui();
    void update_rtc_detection_ui();
    void update_settings_texts();
    void update_main_texts();
    void update_sensor_info_texts();
    void update_confirm_texts();
    void update_wifi_texts();
    void update_mqtt_texts();
    void update_datetime_texts();
    void update_theme_texts();
    void update_auto_night_texts();
    void update_backlight_texts();
    void update_co2_calib_texts();
    void update_fw_update_texts();
    void update_boot_diag_texts();
    void update_dac_ui(uint32_t now_ms);
    void update_led_indicators();
    void update_co2_bar(int co2, bool valid);
    void init_ui_defaults();
    void set_visible(lv_obj_t *obj, bool visible);
    void hide_all_sensor_info_containers();

    void safe_label_set_text(lv_obj_t *obj, const char *new_text);
    void safe_label_set_text_static(lv_obj_t *obj, const char *new_text);
    void update_qrcode_if_needed(lv_obj_t *obj, const char *text, char *cache, size_t cache_size);
    void reset_dynamic_url_caches();
    void open_rtc_detection_overlay();
    void close_rtc_detection_overlay();
    bool rtc_detection_overlay_visible() const;
    void set_rtc_detection_pending_mode(Config::RtcMode mode);
    lv_color_t color_inactive();
    lv_color_t color_green();
    lv_color_t color_yellow();
    lv_color_t color_orange();
    lv_color_t color_red();
    lv_color_t color_blue();
    lv_color_t color_card_border();
    lv_color_t getTempColor(float t);
    lv_color_t getHumidityColor(float h);
    lv_color_t getAbsoluteHumidityColor(float ah);
    lv_color_t getDewPointColor(float dew_c);
    lv_color_t getCO2Color(int co2);
    lv_color_t getCOColor(float co_ppm);
    lv_color_t getPM05Color(float pm);
    lv_color_t getPM25Color(float pm);
    lv_color_t getPM4Color(float pm);
    lv_color_t getPM10Color(float pm);
    lv_color_t getPM1Color(float pm);
    lv_color_t getPressureDeltaColor(float delta, bool valid, bool is24h);
    lv_color_t getVOCColor(int voc);
    lv_color_t getNOxColor(int nox);
    lv_color_t getHCHOColor(float hcho_ppb, bool valid);
    AirQuality getAirQuality(const SensorData &data);
    bool has_poor_gas_background_alert();
    bool has_high_co2_background_alert();
    void update_main_screen_background_alert();
    lv_color_t blink_red(lv_color_t color);
    lv_color_t night_alert_color(lv_color_t color);
    lv_color_t alert_color_for_mode(lv_color_t color);
    void compute_header_style(const AirQuality &aq,
                              uint8_t status_severity,
                              bool co_alert_active,
                              lv_color_t &color,
                              lv_opa_t &shadow_opa);
    void compute_status_summary(bool gas_warmup, bool &co_alert_active, uint8_t &max_severity) const;
    lv_color_t active_text_color();

    void sync_wifi_toggle_state();
    void sync_mqtt_toggle_state();
    void sync_alert_blink_toggle_state();
    void sync_auto_dim_button_state();
    void sync_backlight_settings_button_state();
    void sync_night_mode_toggle_ui();

    void apply_toggle_style(lv_obj_t *btn);
    void set_dot_color(lv_obj_t *obj, lv_color_t color);
    void set_chip_color(lv_obj_t *obj, lv_color_t color);
    void set_button_enabled(lv_obj_t *btn, bool enabled);
    void confirm_show(ConfirmAction action);
    void confirm_hide();

    void night_mode_on_enter();
    void night_mode_on_exit();
    void set_night_mode_state(bool enabled, bool save_pref);
    void apply_pending_screen_now_from_web();

    void mqtt_apply_pending();
    void bind_screen_events_once(int screen_id);
    void bind_available_events(int screen_id);
    void apply_toggle_styles_for_available_objects(int screen_id);
    void apply_checked_states_for_available_objects(int screen_id);
    void init_theme_controls_if_available();

    void on_settings_event(lv_event_t *e);
    void on_back_event(lv_event_t *e);
    void on_about_event(lv_event_t *e);
    void on_about_back_event(lv_event_t *e);
    void on_web_page_event(lv_event_t *e);
    void on_web_page_back_event(lv_event_t *e);
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
    void on_time_format_toggle_event(lv_event_t *e);
    void on_units_mdy_event(lv_event_t *e);
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
    void on_backlight_alarm_wake_event(lv_event_t *e);
    void on_backlight_preset_always_on_event(lv_event_t *e);
    void on_backlight_preset_30s_event(lv_event_t *e);
    void on_backlight_preset_1m_event(lv_event_t *e);
    void on_backlight_sleep_hours_minus_event(lv_event_t *e);
    void on_backlight_sleep_hours_plus_event(lv_event_t *e);
    void on_backlight_sleep_minutes_minus_event(lv_event_t *e);
    void on_backlight_sleep_minutes_plus_event(lv_event_t *e);
    void on_backlight_wake_hours_minus_event(lv_event_t *e);
    void on_backlight_wake_hours_plus_event(lv_event_t *e);
    void on_backlight_wake_minutes_minus_event(lv_event_t *e);
    void on_backlight_wake_minutes_plus_event(lv_event_t *e);
    void on_language_event(lv_event_t *e);
    void on_datetime_back_event(lv_event_t *e);
    void on_datetime_apply_event(lv_event_t *e);
    void on_rtc_status_event(lv_event_t *e);
    void on_rtc_detection_auto_event(lv_event_t *e);
    void on_rtc_detection_pcf8523_event(lv_event_t *e);
    void on_rtc_detection_ds3231_event(lv_event_t *e);
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
    void on_card_temp_event(lv_event_t *e);
    void on_card_voc_event(lv_event_t *e);
    void on_card_nox_event(lv_event_t *e);
    void on_card_hcho_event(lv_event_t *e);
    void on_card_co2_event(lv_event_t *e);
    void on_card_hum_event(lv_event_t *e);
    void on_rh_info_event(lv_event_t *e);
    void on_ah_info_event(lv_event_t *e);
    void on_mr_info_event(lv_event_t *e);
    void on_dp_info_event(lv_event_t *e);
    void on_info_graph_event(lv_event_t *e);
    void on_temp_range_1h_event(lv_event_t *e);
    void on_temp_range_3h_event(lv_event_t *e);
    void on_temp_range_24h_event(lv_event_t *e);
    void on_rh_range_1h_event(lv_event_t *e);
    void on_rh_range_3h_event(lv_event_t *e);
    void on_rh_range_24h_event(lv_event_t *e);
    void on_voc_range_1h_event(lv_event_t *e);
    void on_voc_range_3h_event(lv_event_t *e);
    void on_voc_range_24h_event(lv_event_t *e);
    void on_nox_range_1h_event(lv_event_t *e);
    void on_nox_range_3h_event(lv_event_t *e);
    void on_nox_range_24h_event(lv_event_t *e);
    void on_hcho_range_1h_event(lv_event_t *e);
    void on_hcho_range_3h_event(lv_event_t *e);
    void on_hcho_range_24h_event(lv_event_t *e);
    void on_co2_range_1h_event(lv_event_t *e);
    void on_co2_range_3h_event(lv_event_t *e);
    void on_co2_range_24h_event(lv_event_t *e);
    void on_pm05_range_1h_event(lv_event_t *e);
    void on_pm05_range_3h_event(lv_event_t *e);
    void on_pm05_range_24h_event(lv_event_t *e);
    void on_pm25_4_range_1h_event(lv_event_t *e);
    void on_pm25_4_range_3h_event(lv_event_t *e);
    void on_pm25_4_range_24h_event(lv_event_t *e);
    void on_pm1_10_range_1h_event(lv_event_t *e);
    void on_pm1_10_range_3h_event(lv_event_t *e);
    void on_pm1_10_range_24h_event(lv_event_t *e);
    void on_co_range_1h_event(lv_event_t *e);
    void on_co_range_3h_event(lv_event_t *e);
    void on_co_range_24h_event(lv_event_t *e);
    void on_pressure_range_1h_event(lv_event_t *e);
    void on_pressure_range_3h_event(lv_event_t *e);
    void on_pressure_range_24h_event(lv_event_t *e);
    void on_pm25_info_event(lv_event_t *e);
    void on_pm4_info_event(lv_event_t *e);
    void on_pm10_info_event(lv_event_t *e);
    void on_pm1_info_event(lv_event_t *e);
    void on_card_pm05_event(lv_event_t *e);
    void on_card_pm25_event(lv_event_t *e);
    void on_card_pm10_event(lv_event_t *e);
    void on_card_co_event(lv_event_t *e);
    void on_card_pressure_event(lv_event_t *e);
    void on_pressure_3h_info_event(lv_event_t *e);
    void on_pressure_24h_info_event(lv_event_t *e);
    void on_sensors_info_back_event(lv_event_t *e);
    void on_temp_offset_minus(lv_event_t *e);
    void on_temp_offset_plus(lv_event_t *e);
    void on_hum_offset_minus(lv_event_t *e);
    void on_hum_offset_plus(lv_event_t *e);
    void on_diag_event(lv_event_t *e);
    void on_diag_back_event(lv_event_t *e);
    void on_boot_diag_continue(lv_event_t *e);
    void on_boot_diag_errors(lv_event_t *e);
    void on_dac_settings_event(lv_event_t *e);
    void on_dac_settings_back_event(lv_event_t *e);
    void on_dac_manual_on_event(lv_event_t *e);
    void on_dac_auto_on_event(lv_event_t *e);
    void on_dac_manual_level_event(lv_event_t *e);
    void on_dac_manual_timer_event(lv_event_t *e);
    void on_dac_manual_start_event(lv_event_t *e);
    void on_dac_manual_stop_event(lv_event_t *e);
    void on_dac_manual_auto_event(lv_event_t *e);
    void on_dac_auto_start_event(lv_event_t *e);
    void on_dac_auto_stop_event(lv_event_t *e);

    static void on_settings_event_cb(lv_event_t *e);
    static void on_back_event_cb(lv_event_t *e);
    static void on_about_event_cb(lv_event_t *e);
    static void on_about_back_event_cb(lv_event_t *e);
    static void on_web_page_event_cb(lv_event_t *e);
    static void on_web_page_back_event_cb(lv_event_t *e);
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
    static void on_time_format_toggle_event_cb(lv_event_t *e);
    static void on_units_mdy_event_cb(lv_event_t *e);
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
    static void on_backlight_alarm_wake_event_cb(lv_event_t *e);
    static void on_backlight_preset_always_on_event_cb(lv_event_t *e);
    static void on_backlight_preset_30s_event_cb(lv_event_t *e);
    static void on_backlight_preset_1m_event_cb(lv_event_t *e);
    static void on_backlight_sleep_hours_minus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_hours_plus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_minutes_minus_event_cb(lv_event_t *e);
    static void on_backlight_sleep_minutes_plus_event_cb(lv_event_t *e);
    static void on_backlight_wake_hours_minus_event_cb(lv_event_t *e);
    static void on_backlight_wake_hours_plus_event_cb(lv_event_t *e);
    static void on_backlight_wake_minutes_minus_event_cb(lv_event_t *e);
    static void on_backlight_wake_minutes_plus_event_cb(lv_event_t *e);
    static void on_language_event_cb(lv_event_t *e);
    static void on_datetime_back_event_cb(lv_event_t *e);
    static void on_datetime_apply_event_cb(lv_event_t *e);
    static void on_rtc_status_event_cb(lv_event_t *e);
    static void on_rtc_detection_auto_event_cb(lv_event_t *e);
    static void on_rtc_detection_pcf8523_event_cb(lv_event_t *e);
    static void on_rtc_detection_ds3231_event_cb(lv_event_t *e);
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
    static void on_card_temp_event_cb(lv_event_t *e);
    static void on_card_voc_event_cb(lv_event_t *e);
    static void on_card_nox_event_cb(lv_event_t *e);
    static void on_card_hcho_event_cb(lv_event_t *e);
    static void on_card_co2_event_cb(lv_event_t *e);
    static void on_card_hum_event_cb(lv_event_t *e);
    static void on_rh_info_event_cb(lv_event_t *e);
    static void on_ah_info_event_cb(lv_event_t *e);
    static void on_mr_info_event_cb(lv_event_t *e);
    static void on_dp_info_event_cb(lv_event_t *e);
    static void on_info_graph_event_cb(lv_event_t *e);
    static void on_temp_range_1h_event_cb(lv_event_t *e);
    static void on_temp_range_3h_event_cb(lv_event_t *e);
    static void on_temp_range_24h_event_cb(lv_event_t *e);
    static void on_rh_range_1h_event_cb(lv_event_t *e);
    static void on_rh_range_3h_event_cb(lv_event_t *e);
    static void on_rh_range_24h_event_cb(lv_event_t *e);
    static void on_voc_range_1h_event_cb(lv_event_t *e);
    static void on_voc_range_3h_event_cb(lv_event_t *e);
    static void on_voc_range_24h_event_cb(lv_event_t *e);
    static void on_nox_range_1h_event_cb(lv_event_t *e);
    static void on_nox_range_3h_event_cb(lv_event_t *e);
    static void on_nox_range_24h_event_cb(lv_event_t *e);
    static void on_hcho_range_1h_event_cb(lv_event_t *e);
    static void on_hcho_range_3h_event_cb(lv_event_t *e);
    static void on_hcho_range_24h_event_cb(lv_event_t *e);
    static void on_co2_range_1h_event_cb(lv_event_t *e);
    static void on_co2_range_3h_event_cb(lv_event_t *e);
    static void on_co2_range_24h_event_cb(lv_event_t *e);
    static void on_pm05_range_1h_event_cb(lv_event_t *e);
    static void on_pm05_range_3h_event_cb(lv_event_t *e);
    static void on_pm05_range_24h_event_cb(lv_event_t *e);
    static void on_pm25_4_range_1h_event_cb(lv_event_t *e);
    static void on_pm25_4_range_3h_event_cb(lv_event_t *e);
    static void on_pm25_4_range_24h_event_cb(lv_event_t *e);
    static void on_pm1_10_range_1h_event_cb(lv_event_t *e);
    static void on_pm1_10_range_3h_event_cb(lv_event_t *e);
    static void on_pm1_10_range_24h_event_cb(lv_event_t *e);
    static void on_co_range_1h_event_cb(lv_event_t *e);
    static void on_co_range_3h_event_cb(lv_event_t *e);
    static void on_co_range_24h_event_cb(lv_event_t *e);
    static void on_pressure_range_1h_event_cb(lv_event_t *e);
    static void on_pressure_range_3h_event_cb(lv_event_t *e);
    static void on_pressure_range_24h_event_cb(lv_event_t *e);
    static void on_pm25_info_event_cb(lv_event_t *e);
    static void on_pm4_info_event_cb(lv_event_t *e);
    static void on_pm10_info_event_cb(lv_event_t *e);
    static void on_pm1_info_event_cb(lv_event_t *e);
    static void on_card_pm05_event_cb(lv_event_t *e);
    static void on_card_pm25_event_cb(lv_event_t *e);
    static void on_card_pm10_event_cb(lv_event_t *e);
    static void on_card_co_event_cb(lv_event_t *e);
    static void on_card_pressure_event_cb(lv_event_t *e);
    static void on_pressure_3h_info_event_cb(lv_event_t *e);
    static void on_pressure_24h_info_event_cb(lv_event_t *e);
    static void on_sensors_info_back_event_cb(lv_event_t *e);
    static void on_temp_offset_minus_cb(lv_event_t *e);
    static void on_temp_offset_plus_cb(lv_event_t *e);
    static void on_hum_offset_minus_cb(lv_event_t *e);
    static void on_hum_offset_plus_cb(lv_event_t *e);
    static void on_diag_event_cb(lv_event_t *e);
    static void on_diag_back_event_cb(lv_event_t *e);
    static void on_boot_diag_continue_cb(lv_event_t *e);
    static void on_boot_diag_errors_cb(lv_event_t *e);
    static void on_dac_settings_event_cb(lv_event_t *e);
    static void on_dac_settings_back_event_cb(lv_event_t *e);
    static void on_dac_manual_on_event_cb(lv_event_t *e);
    static void on_dac_auto_on_event_cb(lv_event_t *e);
    static void on_dac_manual_level_event_cb(lv_event_t *e);
    static void on_dac_manual_timer_event_cb(lv_event_t *e);
    static void on_dac_manual_start_event_cb(lv_event_t *e);
    static void on_dac_manual_stop_event_cb(lv_event_t *e);
    static void on_dac_manual_auto_event_cb(lv_event_t *e);
    static void on_dac_auto_start_event_cb(lv_event_t *e);
    static void on_dac_auto_stop_event_cb(lv_event_t *e);
    static void apply_toggle_style_cb(lv_obj_t *btn);
    static void mqtt_sync_with_wifi_cb();
    static WebUiBridge::ApplyResult applyWebUiSettingsBridge(const WebUiBridge::SettingsUpdate &update,
                                                             void *ctx);
    static WebUiBridge::ApplyResult applyThemePreviewBridge(const WebUiBridge::ThemeUpdate &update,
                                                            void *ctx);
    static WebUiBridge::ApplyResult applyDacActionBridge(const WebUiBridge::DacActionUpdate &update,
                                                         void *ctx);
    static WebUiBridge::ApplyResult applyDacAutoBridge(const WebUiBridge::DacAutoUpdate &update,
                                                       void *ctx);
    static WebUiBridge::ApplyResult applyWifiSaveBridge(const WebUiBridge::WifiSaveUpdate &update,
                                                        void *ctx);
    static WebUiBridge::ApplyResult applyMqttSaveBridge(const WebUiBridge::MqttSaveUpdate &update,
                                                        void *ctx);

    static UiController *instance_;

    StorageManager &storage;
    AuraNetworkManager &networkManager;
    MqttManager &mqttManager;
    ConnectivityRuntime &connectivityRuntime;
    MqttRuntimeState &mqttRuntimeState;
    WebUiBridge &webUiBridge;
    NetworkCommandQueue &networkCommandQueue;
    SensorManager &sensorManager;
    ChartsHistory &chartsHistory;
    TimeManager &timeManager;
    ThemeManager &themeManager;
    BacklightManager &backlightManager;
    NightModeManager &nightModeManager;
    FanControl &fanControl;
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
    static constexpr size_t kScreenSlotCount = 16; // screen ids are 1..15
    bool screen_events_bound_[kScreenSlotCount] = {};
    bool theme_events_bound_ = false;
    int pending_screen_id = 0;
    int current_screen_id = 0;
    bool header_status_enabled = true;
    int wifi_icon_state = -1;
    int mqtt_icon_state = -1;
    int wifi_icon_state_main = -1;
    int mqtt_icon_state_main = -1;
    static constexpr size_t kQrUrlCacheSize = 96;
    char web_page_qr_cache_[kQrUrlCacheSize] = {};
    char wifi_portal_qr_cache_[kQrUrlCacheSize] = {};
    char mqtt_portal_qr_cache_[kQrUrlCacheSize] = {};
    char theme_custom_qr_cache_[kQrUrlCacheSize] = {};
    char dac_portal_qr_cache_[kQrUrlCacheSize] = {};
    uint32_t dac_network_ui_signature_ = UINT32_MAX;
    bool clock_ui_dirty = true;
    bool datetime_ui_dirty = true;
    bool web_page_panel_dirty = false;
    bool dac_auto_tab_selected_ = false;
    bool ntp_toggle_syncing = false;
    ConfirmAction confirm_action = CONFIRM_NONE;
    uint32_t last_clock_tick_ms = 0;
    int set_hour = 0;
    int set_minute = 0;
    int set_day = 1;
    int set_month = 1;
    int set_year = 2024;
    bool datetime_changed = false;
    Config::RtcMode rtc_detection_saved_mode_ = Config::RtcMode::Auto;
    Config::RtcMode rtc_detection_pending_mode_ = Config::RtcMode::Auto;
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
    Config::Language ui_language = Config::Language::EN;
    bool date_units_mdy = false;
    bool time_format_24h_ = true;
    bool language_dirty = false;
    bool blink_state = true;
    uint32_t last_blink_ms = 0;
    uint32_t last_ui_update_ms = 0;
    uint32_t last_dac_ui_update_ms = 0;
    uint32_t last_diag_log_update_ms = 0;
    uint32_t last_settings_header_update_ms = 0;
    uint32_t last_ui_tick_ms = 0;
    uint32_t status_msg_last_ms = 0;
    uint32_t status_msg_signature = 0;
    uint8_t status_msg_index = 0;
    uint8_t status_msg_count = 0;
    uint8_t status_max_severity = 0;
    bool co_status_alert_active = false;
    bool poor_gas_background_alert_active_ = false;
    bool high_co2_background_alert_active_ = false;
    ConnectivityRuntimeSnapshot connectivity_;
    bool wifi_toggle_syncing_ = false;
    bool mqtt_toggle_syncing_ = false;
    bool wifi_override_active_ = false;
    bool wifi_override_enabled_ = false;
    uint32_t last_lvgl_lock_warn_ms = 0;
    uint16_t lvgl_lock_fail_streak = 0;
    uint32_t lvgl_diag_last_heartbeat_ms = 0;
    uint32_t lvgl_diag_prev_heartbeat_lock_fail_count = 0;
    uint32_t lvgl_diag_prev_heartbeat_touch_err_count = 0;
    uint32_t lvgl_diag_last_stall_warn_ms = 0;
    bool lvgl_diag_stall_active = false;
    uint32_t lvgl_diag_stall_since_ms = 0;
    bool firmware_update_screen_active_ = false;
    int firmware_update_return_screen_id_ = 0;
    UiDeferredUnload deferred_unload_;
    bool boot_logo_active = false;
    uint32_t boot_logo_start_ms = 0;
    bool boot_diag_active = false;
    bool boot_diag_has_error = false;
    uint32_t boot_diag_start_ms = 0;
    uint32_t last_boot_diag_update_ms = 0;
    uint32_t boot_release_at_ms = 0;
    bool boot_ui_released = false;
    InfoSensor info_sensor = INFO_NONE;
    TempGraphRange temp_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool temp_graph_mode_ = false;
    bool rh_graph_mode_ = false;
    TempGraphRange rh_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool voc_graph_mode_ = false;
    TempGraphRange voc_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool nox_graph_mode_ = false;
    TempGraphRange nox_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool hcho_graph_mode_ = false;
    TempGraphRange hcho_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool co2_graph_mode_ = false;
    TempGraphRange co2_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool pm05_graph_mode_ = false;
    TempGraphRange pm05_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool pm25_4_graph_mode_ = false;
    TempGraphRange pm25_4_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool pm1_10_graph_mode_ = false;
    TempGraphRange pm1_10_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool co_graph_mode_ = false;
    TempGraphRange co_graph_range_ = TEMP_GRAPH_RANGE_3H;
    bool pressure_graph_mode_ = false;
    TempGraphRange pressure_graph_range_ = TEMP_GRAPH_RANGE_3H;
    InfoSensor graph_refresh_sensor_ = INFO_NONE;
    TempGraphRange graph_refresh_range_ = TEMP_GRAPH_RANGE_3H;
    uint16_t graph_refresh_points_ = 0;
    uint16_t graph_refresh_history_count_ = 0;
    uint32_t graph_refresh_epoch_ = 0;
    bool graph_refresh_units_c_ = true;
    bool graph_refresh_night_mode_ = false;
    uint32_t graph_refresh_theme_sig_ = 0;
    uint32_t graph_refresh_last_ms_ = 0;
    lv_obj_t *temp_graph_label_min_ = nullptr;
    lv_obj_t *temp_graph_label_now_ = nullptr;
    lv_obj_t *temp_graph_label_max_ = nullptr;
    lv_obj_t *temp_graph_zone_overlay_ = nullptr;
    lv_obj_t *temp_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *temp_graph_time_labels_[7] = {};
    lv_obj_t *rh_graph_label_min_ = nullptr;
    lv_obj_t *rh_graph_label_now_ = nullptr;
    lv_obj_t *rh_graph_label_max_ = nullptr;
    lv_obj_t *rh_graph_zone_overlay_ = nullptr;
    lv_obj_t *rh_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *rh_graph_time_labels_[7] = {};
    lv_obj_t *voc_graph_label_min_ = nullptr;
    lv_obj_t *voc_graph_label_now_ = nullptr;
    lv_obj_t *voc_graph_label_max_ = nullptr;
    lv_obj_t *voc_graph_zone_overlay_ = nullptr;
    lv_obj_t *voc_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *voc_graph_time_labels_[7] = {};
    lv_obj_t *nox_graph_label_min_ = nullptr;
    lv_obj_t *nox_graph_label_now_ = nullptr;
    lv_obj_t *nox_graph_label_max_ = nullptr;
    lv_obj_t *nox_graph_zone_overlay_ = nullptr;
    lv_obj_t *nox_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *nox_graph_time_labels_[7] = {};
    lv_obj_t *hcho_graph_label_min_ = nullptr;
    lv_obj_t *hcho_graph_label_now_ = nullptr;
    lv_obj_t *hcho_graph_label_max_ = nullptr;
    lv_obj_t *hcho_graph_zone_overlay_ = nullptr;
    lv_obj_t *hcho_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *hcho_graph_time_labels_[7] = {};
    lv_obj_t *co2_graph_label_min_ = nullptr;
    lv_obj_t *co2_graph_label_now_ = nullptr;
    lv_obj_t *co2_graph_label_max_ = nullptr;
    lv_obj_t *co2_graph_zone_overlay_ = nullptr;
    lv_obj_t *co2_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *co2_graph_time_labels_[7] = {};
    lv_obj_t *co_graph_label_min_ = nullptr;
    lv_obj_t *co_graph_label_now_ = nullptr;
    lv_obj_t *co_graph_label_max_ = nullptr;
    lv_obj_t *co_graph_zone_overlay_ = nullptr;
    lv_obj_t *co_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *co_graph_time_labels_[7] = {};
    lv_obj_t *pm05_graph_label_min_ = nullptr;
    lv_obj_t *pm05_graph_label_now_ = nullptr;
    lv_obj_t *pm05_graph_label_max_ = nullptr;
    lv_obj_t *pm05_graph_zone_overlay_ = nullptr;
    lv_obj_t *pm05_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *pm05_graph_time_labels_[7] = {};
    lv_obj_t *pm25_4_graph_label_min_ = nullptr;
    lv_obj_t *pm25_4_graph_label_now_ = nullptr;
    lv_obj_t *pm25_4_graph_label_max_ = nullptr;
    lv_obj_t *pm25_4_graph_zone_overlay_ = nullptr;
    lv_obj_t *pm25_4_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *pm25_4_graph_time_labels_[7] = {};
    lv_obj_t *pm1_10_graph_label_min_ = nullptr;
    lv_obj_t *pm1_10_graph_label_now_ = nullptr;
    lv_obj_t *pm1_10_graph_label_max_ = nullptr;
    lv_obj_t *pm1_10_graph_zone_overlay_ = nullptr;
    lv_obj_t *pm1_10_graph_zone_bands_[kMaxGraphZoneBands] = {};
    lv_obj_t *pm1_10_graph_time_labels_[7] = {};
    lv_obj_t *pressure_graph_label_min_ = nullptr;
    lv_obj_t *pressure_graph_label_now_ = nullptr;
    lv_obj_t *pressure_graph_label_max_ = nullptr;
    lv_obj_t *pressure_graph_time_labels_[7] = {};
};
