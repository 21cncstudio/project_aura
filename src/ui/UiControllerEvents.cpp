// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <WiFi.h>
#include <math.h>

#include "config/AppConfig.h"
#include "core/Logger.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/StorageManager.h"
#include "modules/TimeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/ThemeManager.h"
#include "ui/UiStrings.h"
#include "ui/ui.h"

using namespace Config;

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
void UiController::on_language_event_cb(lv_event_t *e) { if (instance_) instance_->on_language_event(e); }
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
        storage.saveConfig(true);
        if (offsets_saved) LOGI("UI", "offsets saved");
        if (language_saved) LOGI("UI", "language saved");
    }
    pending_screen_id = SCREEN_ID_PAGE_MAIN;
}

void UiController::on_language_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_language = next_language(ui_language);
    UiStrings::setLanguage(ui_language);
    language_dirty = (ui_language != storage.config().language);
    update_language_label();
    update_settings_texts();
    update_wifi_texts();
    update_mqtt_texts();
    update_datetime_texts();
    update_ui();
    update_wifi_ui();
    update_mqtt_ui();
    update_datetime_ui();
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

