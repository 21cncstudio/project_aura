// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "BacklightManager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_display_panel.hpp>
#include "modules/StorageManager.h"
#include "ui/ui.h"

namespace {

void safe_label_set_text(lv_obj_t *obj, const char *new_text) {
    if (!obj) {
        return;
    }
    const char *current = lv_label_get_text(obj);
    if (current && strcmp(current, new_text) == 0) {
        return;
    }
    lv_label_set_text(obj, new_text);
}

int wrap_value(int value, int modulo) {
    if (modulo == 0) {
        return value;
    }
    value %= modulo;
    if (value < 0) {
        value += modulo;
    }
    return value;
}

bool is_sleep_window(int sleep_hour, int sleep_minute, int wake_hour, int wake_minute,
                     const tm &local_tm) {
    int now_min = local_tm.tm_hour * 60 + local_tm.tm_min;
    int sleep_min = sleep_hour * 60 + sleep_minute;
    int wake_min = wake_hour * 60 + wake_minute;
    if (sleep_min == wake_min) {
        return false;
    }
    if (sleep_min < wake_min) {
        return now_min >= sleep_min && now_min < wake_min;
    }
    return now_min >= sleep_min || now_min < wake_min;
}

} // namespace

void BacklightManager::loadFromPrefs(StorageManager &storage) {
    const auto &cfg = storage.config();
    uint32_t timeout_s = cfg.backlight_timeout_s;
    backlight_timeout_ms_ = normalizeTimeoutMs(timeout_s * 1000UL);
    schedule_enabled_ = cfg.backlight_schedule_enabled;
    sleep_hour_ = cfg.backlight_sleep_hour;
    sleep_minute_ = cfg.backlight_sleep_minute;
    wake_hour_ = cfg.backlight_wake_hour;
    wake_minute_ = cfg.backlight_wake_minute;

    if (sleep_hour_ < 0 || sleep_hour_ > 23) sleep_hour_ = 23;
    if (wake_hour_ < 0 || wake_hour_ > 23) wake_hour_ = 6;
    if (sleep_minute_ < 0 || sleep_minute_ > 59) sleep_minute_ = 0;
    if (wake_minute_ < 0 || wake_minute_ > 59) wake_minute_ = 0;

    prefs_dirty_ = false;
    ui_dirty_ = true;
}

void BacklightManager::attachBacklight(esp_panel::drivers::Backlight *backlight) {
    panel_backlight_ = backlight;
    backlight_on_ = panel_backlight_ != nullptr;
}

uint32_t BacklightManager::normalizeTimeoutMs(uint32_t timeout_ms) const {
    if (timeout_ms == Config::BACKLIGHT_TIMEOUT_30S ||
        timeout_ms == Config::BACKLIGHT_TIMEOUT_1M ||
        timeout_ms == Config::BACKLIGHT_TIMEOUT_5M) {
        return timeout_ms;
    }
    return 0;
}

void BacklightManager::setTimeoutMs(uint32_t timeout_ms) {
    timeout_ms = normalizeTimeoutMs(timeout_ms);
    if (timeout_ms == backlight_timeout_ms_) {
        return;
    }
    backlight_timeout_ms_ = timeout_ms;
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void BacklightManager::setOn(bool on) {
    if (!panel_backlight_) {
        return;
    }
    if (on == backlight_on_) {
        return;
    }
    if (on) {
        panel_backlight_->on();
    } else {
        panel_backlight_->off();
    }
    backlight_on_ = on;
    if (on) {
        lv_disp_trig_activity(nullptr);
        last_inactive_ms_ = 0;
    }
}

void BacklightManager::storeSchedulePrefs() {
    prefs_dirty_ = true;
}

void BacklightManager::savePrefs(StorageManager &storage) {
    if (!prefs_dirty_) {
        return;
    }
    auto &cfg = storage.config();
    cfg.backlight_timeout_s = backlight_timeout_ms_ / 1000;
    cfg.backlight_schedule_enabled = schedule_enabled_;
    cfg.backlight_sleep_hour = sleep_hour_;
    cfg.backlight_sleep_minute = sleep_minute_;
    cfg.backlight_wake_hour = wake_hour_;
    cfg.backlight_wake_minute = wake_minute_;
    storage.saveConfig(true);
    prefs_dirty_ = false;
}

void BacklightManager::setScheduleEnabled(bool enabled) {
    if (enabled == schedule_enabled_) {
        return;
    }
    schedule_enabled_ = enabled;
    prefs_dirty_ = true;
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustSleepHour(int delta) {
    sleep_hour_ = wrap_value(sleep_hour_ + delta, 24);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustSleepMinute(int delta) {
    sleep_minute_ = wrap_value(sleep_minute_ + delta, 60);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustWakeHour(int delta) {
    wake_hour_ = wrap_value(wake_hour_ + delta, 24);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustWakeMinute(int delta) {
    wake_minute_ = wrap_value(wake_minute_ + delta, 60);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::refreshSchedule() {
    bool active = false;
    if (schedule_enabled_) {
        time_t now = time(nullptr);
        if (now > Config::TIME_VALID_EPOCH) {
            tm local_tm = {};
            localtime_r(&now, &local_tm);
            active = is_sleep_window(sleep_hour_, sleep_minute_, wake_hour_, wake_minute_, local_tm);
        }
    }
    if (active != schedule_active_) {
        schedule_active_ = active;
        if (active) {
            setOn(false);
        } else {
            setOn(true);
        }
    }
}

void BacklightManager::updateUi() {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", sleep_hour_);
    if (objects.label_backlight_sleep_hours_value) {
        safe_label_set_text(objects.label_backlight_sleep_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", sleep_minute_);
    if (objects.label_backlight_sleep_minutes_value) {
        safe_label_set_text(objects.label_backlight_sleep_minutes_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", wake_hour_);
    if (objects.label_backlight_wake_hours_value) {
        safe_label_set_text(objects.label_backlight_wake_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", wake_minute_);
    if (objects.label_backlight_wake_minutes_value) {
        safe_label_set_text(objects.label_backlight_wake_minutes_value, buf);
    }

    schedule_syncing_ = true;
    if (objects.btn_backlight_schedule_toggle) {
        if (schedule_enabled_) {
            lv_obj_add_state(objects.btn_backlight_schedule_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_backlight_schedule_toggle, LV_STATE_CHECKED);
        }
    }
    schedule_syncing_ = false;

    preset_syncing_ = true;
    bool always_on = backlight_timeout_ms_ == 0;
    if (objects.btn_backlight_always_on) {
        if (always_on) lv_obj_add_state(objects.btn_backlight_always_on, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_always_on, LV_STATE_CHECKED);
    }
    if (objects.btn_backlight_30s) {
        if (backlight_timeout_ms_ == Config::BACKLIGHT_TIMEOUT_30S) lv_obj_add_state(objects.btn_backlight_30s, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_30s, LV_STATE_CHECKED);
    }
    if (objects.btn_backlight_1m) {
        if (backlight_timeout_ms_ == Config::BACKLIGHT_TIMEOUT_1M) lv_obj_add_state(objects.btn_backlight_1m, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_1m, LV_STATE_CHECKED);
    }
    if (objects.btn_backlight_5m) {
        if (backlight_timeout_ms_ == Config::BACKLIGHT_TIMEOUT_5M) lv_obj_add_state(objects.btn_backlight_5m, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_5m, LV_STATE_CHECKED);
    }
    preset_syncing_ = false;
    ui_dirty_ = false;
}

void BacklightManager::consumeInput() {
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_reset(indev, nullptr);
            lv_indev_wait_release(indev);
        }
        indev = lv_indev_get_next(indev);
    }
}

void BacklightManager::poll(bool lvgl_ready) {
    if (!panel_backlight_ || !lvgl_ready) {
        return;
    }
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }
    uint32_t now_ms = millis();
    uint32_t inactive_ms = lv_disp_get_inactive_time(disp);
    bool activity = inactive_ms < last_inactive_ms_;
    last_inactive_ms_ = inactive_ms;

    refreshSchedule();

    if (!backlight_on_) {
        if (activity) {
            setOn(true);
            block_input_until_ms_ = now_ms + Config::BACKLIGHT_WAKE_BLOCK_MS;
            consumeInput();
        }
        return;
    }

    if (block_input_until_ms_ != 0 && now_ms < block_input_until_ms_) {
        consumeInput();
    } else {
        block_input_until_ms_ = 0;
    }

    uint32_t effective_timeout_ms = backlight_timeout_ms_;
    if (schedule_active_ && effective_timeout_ms == 0) {
        effective_timeout_ms = Config::BACKLIGHT_SCHEDULE_WAKE_MS;
    }
    if (effective_timeout_ms > 0 && inactive_ms >= effective_timeout_ms) {
        setOn(false);
    }
}
