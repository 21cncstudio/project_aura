// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"
#include "ui/UiText.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include <WiFi.h>
#include <esp_heap_caps.h>

#include "lvgl_v8_port.h"
#include "ui/ui.h"
#include "ui/styles.h"
#include "ui/images.h"
#include "ui/StatusMessages.h"
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

const char *language_label(Language lang) {
    switch (lang) {
        case Language::DE: return "DEUTSCH";
        case Language::EN:
        default:
            return "ENGLISH";
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
    struct EventBinding {
        lv_obj_t *obj;
        lv_event_cb_t cb;
        lv_event_code_t code;
    };

    const EventBinding click_bindings[] = {
        {objects.btn_settings, on_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_back, on_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi, on_wifi_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_back, on_wifi_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt, on_mqtt_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt_back, on_mqtt_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_mqtt_reconnect, on_mqtt_reconnect_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_reconnect, on_wifi_reconnect_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_start_ap, on_wifi_start_ap_event_cb, LV_EVENT_CLICKED},
        {objects.btn_time_date, on_time_date_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_dim, on_auto_night_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_head_status_1, on_backlight_settings_event_cb, LV_EVENT_CLICKED},
        {objects.btn_language, on_language_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_back, on_backlight_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_back, on_auto_night_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_hours_minus, on_auto_night_start_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_hours_plus, on_auto_night_start_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_minutes_minus, on_auto_night_start_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_start_minutes_plus, on_auto_night_start_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_hours_minus, on_auto_night_end_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_hours_plus, on_auto_night_end_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_minutes_minus, on_auto_night_end_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_auto_night_end_minutes_plus, on_auto_night_end_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_always_on, on_backlight_preset_always_on_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_30s, on_backlight_preset_30s_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_1m, on_backlight_preset_1m_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_5m, on_backlight_preset_5m_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_hours_minus, on_backlight_sleep_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_hours_plus, on_backlight_sleep_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_minutes_minus, on_backlight_sleep_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_sleep_minutes_plus, on_backlight_sleep_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_hours_minus, on_backlight_wake_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_hours_plus, on_backlight_wake_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_minutes_minus, on_backlight_wake_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_backlight_wake_minutes_plus, on_backlight_wake_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_restart, on_restart_event_cb, LV_EVENT_CLICKED},
        {objects.btn_factory_reset, on_factory_reset_event_cb, LV_EVENT_CLICKED},
        {objects.btn_voc_reset, on_voc_reset_event_cb, LV_EVENT_CLICKED},
        {objects.btn_confirm_ok, on_confirm_ok_event_cb, LV_EVENT_CLICKED},
        {objects.btn_confirm_cancel, on_confirm_cancel_event_cb, LV_EVENT_CLICKED},
        {objects.btn_datetime_back, on_datetime_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_datetime_apply, on_datetime_apply_event_cb, LV_EVENT_CLICKED},
        {objects.btn_tz_plus, on_tz_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_tz_minus, on_tz_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_hours_minus, on_set_time_hours_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_hours_plus, on_set_time_hours_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_minutes_minus, on_set_time_minutes_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_time_minutes_plus, on_set_time_minutes_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_day_minus, on_set_date_day_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_day_plus, on_set_date_day_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_month_minus, on_set_date_month_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_month_plus, on_set_date_month_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_year_minus, on_set_date_year_minus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_set_date_year_plus, on_set_date_year_plus_event_cb, LV_EVENT_CLICKED},
        {objects.btn_wifi_forget, on_wifi_forget_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib, on_co2_calib_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib_back, on_co2_calib_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_co2_calib_start, on_co2_calib_start_event_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_offset_minus, on_temp_offset_minus_cb, LV_EVENT_CLICKED},
        {objects.btn_temp_offset_plus, on_temp_offset_plus_cb, LV_EVENT_CLICKED},
        {objects.btn_hum_offset_minus, on_hum_offset_minus_cb, LV_EVENT_CLICKED},
        {objects.btn_hum_offset_plus, on_hum_offset_plus_cb, LV_EVENT_CLICKED},
        {objects.btn_theme_color, on_theme_color_event_cb, LV_EVENT_CLICKED},
        {objects.btn_theme_back, on_theme_back_event_cb, LV_EVENT_CLICKED},
        {objects.btn_diag_continue, on_boot_diag_continue_cb, LV_EVENT_CLICKED},
    };

    const EventBinding value_bindings[] = {
        {objects.btn_head_status, on_head_status_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_wifi_toggle, on_wifi_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_mqtt_toggle, on_mqtt_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_auto_night_toggle, on_auto_night_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_backlight_schedule_toggle, on_backlight_schedule_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_night_mode, on_night_mode_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_units_c_f, on_units_c_f_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_led_indicators, on_led_indicators_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_alert_blink, on_alert_blink_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_co2_calib_asc, on_co2_calib_asc_event_cb, LV_EVENT_VALUE_CHANGED},
        {objects.btn_ntp_toggle, on_ntp_toggle_event_cb, LV_EVENT_VALUE_CHANGED},
    };

    auto bind_events = [](const EventBinding *bindings, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            const EventBinding &binding = bindings[i];
            if (binding.obj) {
                lv_obj_add_event_cb(binding.obj, binding.cb, binding.code, nullptr);
            }
        }
    };

    lv_obj_t *toggle_buttons[] = {
        objects.btn_night_mode,
        objects.btn_auto_dim,
        objects.btn_wifi,
        objects.btn_mqtt,
        objects.btn_units_c_f,
        objects.btn_led_indicators,
        objects.btn_alert_blink,
        objects.btn_co2_calib_asc,
        objects.btn_head_status,
        objects.btn_wifi_toggle,
        objects.btn_mqtt_toggle,
        objects.btn_ntp_toggle,
        objects.btn_backlight_schedule_toggle,
        objects.btn_backlight_always_on,
        objects.btn_backlight_30s,
        objects.btn_backlight_1m,
        objects.btn_backlight_5m,
        objects.btn_auto_night_toggle,
    };

    for (lv_obj_t *btn : toggle_buttons) {
        apply_toggle_style(btn);
    }

    auto set_checked = [](lv_obj_t *btn, bool checked) {
        if (!btn) {
            return;
        }
        if (checked) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    };

    set_checked(objects.btn_head_status, true);
    set_checked(objects.btn_night_mode, night_mode);
    set_checked(objects.btn_units_c_f, temp_units_c);
    set_checked(objects.btn_led_indicators, led_indicators_enabled);
    set_checked(objects.btn_alert_blink, alert_blink_enabled);
    set_checked(objects.btn_co2_calib_asc, co2_asc_enabled);

    bind_events(click_bindings, sizeof(click_bindings) / sizeof(click_bindings[0]));
    bind_events(value_bindings, sizeof(value_bindings) / sizeof(value_bindings[0]));
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
        aq.status = UiText::StatusInitializing();
        aq.score = 0;
        aq.color = color_blue();
        return aq;
    }

    aq.score = max_score;

    if (aq.score <= 25)      { aq.status = UiText::QualityExcellent(); aq.color = color_green(); }
    else if (aq.score <= 50) { aq.status = UiText::QualityGood();      aq.color = color_green(); }
    else if (aq.score <= 75) { aq.status = UiText::QualityModerate();  aq.color = color_yellow(); }
    else                     { aq.status = UiText::QualityPoor();     aq.color = color_red(); }

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
        if (objects.label_time_value) safe_label_set_text(objects.label_time_value, UiText::TimeMissing());
        if (objects.label_date_value) safe_label_set_text(objects.label_date_value, UiText::DateMissing());
        if (objects.label_time_value_1) safe_label_set_text(objects.label_time_value_1, UiText::TimeMissing());
        if (objects.label_date_value_1) safe_label_set_text(objects.label_date_value_1, UiText::DateMissing());
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

void UiController::confirm_show(ConfirmAction action) {
    confirm_action = action;
    if (!objects.container_confirm) {
        return;
    }
    bool show_voc = (action == CONFIRM_VOC_RESET);
    bool show_restart = (action == CONFIRM_RESTART);
    bool show_reset = (action == CONFIRM_FACTORY_RESET);

    set_visible(objects.container_confirm, true);
    set_visible(objects.container_confirm_card, true);
    set_visible(objects.btn_confirm_ok, true);
    set_visible(objects.btn_confirm_cancel, true);
    set_visible(objects.label_btn_confirm_cancel, true);
    lv_obj_add_flag(objects.container_confirm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(objects.container_confirm);

    set_visible(objects.label_btn_confirm_voc, show_voc);
    set_visible(objects.label_confirm_title_voc, show_voc);
    set_visible(objects.container_confirm_voc_text, show_voc);

    set_visible(objects.label_btn_confirm_restart, show_restart);
    set_visible(objects.label_confirm_title_restart, show_restart);
    set_visible(objects.container_confirm_restart_text, show_restart);

    set_visible(objects.label_btn_confirm_reset, show_reset);
    set_visible(objects.label_confirm_title_reset, show_reset);
    set_visible(objects.container_confirm_reset_text, show_reset);
}

void UiController::confirm_hide() {
    confirm_action = CONFIRM_NONE;
    set_visible(objects.container_confirm, false);
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
        strcpy(buf, UiText::ValueZeroPercent());
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

    update_sensor_cards(aq, gas_warmup, show_co2_bar);
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

Config::Language UiController::next_language(Config::Language current) {
    switch (current) {
        case Config::Language::DE: return Config::Language::EN;
        case Config::Language::EN:
        default:
            return Config::Language::DE;
    }
}

void UiController::update_language_label() {
    if (objects.label_language_value) {
        safe_label_set_text(objects.label_language_value, language_label(ui_language));
    }
}

void UiController::update_settings_texts() {
    if (objects.label_settings_title) safe_label_set_text(objects.label_settings_title, UiText::LabelSettingsTitle());
    if (objects.label_btn_back) safe_label_set_text(objects.label_btn_back, UiText::LabelSettingsBack());
    if (objects.label_temp_offset_title) safe_label_set_text(objects.label_temp_offset_title, UiText::LabelTempOffsetTitle());
    if (objects.label_hum_offset_title) safe_label_set_text(objects.label_hum_offset_title, UiText::LabelHumOffsetTitle());
    if (objects.label_btn_night_mode) safe_label_set_text(objects.label_btn_night_mode, UiText::LabelNightMode());
    if (objects.label_btn_units_c_f) safe_label_set_text(objects.label_btn_units_c_f, UiText::LabelUnitsCF());
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
    if (objects.label_btn_units_led_indicators) safe_label_set_text(objects.label_btn_units_led_indicators, UiText::LabelLedIndicators());
    if (objects.label_btn_alert_blink) safe_label_set_text(objects.label_btn_alert_blink, UiText::LabelAlertBlink());
    if (objects.label_voc_reset) safe_label_set_text(objects.label_voc_reset, UiText::LabelVocRelearn());
    if (objects.label_btn_head_status_1) safe_label_set_text(objects.label_btn_head_status_1, UiText::LabelBacklight());
}

void UiController::update_theme_custom_info(bool presets) {
    set_visible(objects.container_theme_custom_info, !presets);
    if (!presets && objects.qrcode_theme_custom) {
        lv_qrcode_update(objects.qrcode_theme_custom, UiText::ThemePortalUrl(), strlen(UiText::ThemePortalUrl()));
    }
}

void UiController::update_status_message(uint32_t now_ms, bool gas_warmup) {
    StatusMessages::StatusMessageResult result = StatusMessages::build_status_messages(currentData, gas_warmup);
    const StatusMessages::StatusMessage *messages = result.messages;
    const size_t count = result.count;
    const bool has_valid = result.has_valid;

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
        status_text = UiText::StatusInitializing();
    } else if (count == 0) {
        status_text = UiText::StatusAllGood();
    } else {
        status_text = messages[status_msg_index].text;
    }

    if (objects.label_status_value) {
        safe_label_set_text(objects.label_status_value, status_text ? status_text : UiText::ValueMissing());
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

    ui_language = storage.config().language;
    language_dirty = false;
    UiStrings::setLanguage(ui_language);
    update_language_label();
    update_settings_texts();

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
