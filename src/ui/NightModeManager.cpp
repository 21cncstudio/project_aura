// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "NightModeManager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
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

} // namespace

void NightModeManager::loadFromPrefs(StorageManager &storage) {
    const auto &cfg = storage.config();
    auto_enabled_ = cfg.auto_night_enabled;
    start_hour_ = cfg.auto_night_start_hour;
    start_minute_ = cfg.auto_night_start_minute;
    end_hour_ = cfg.auto_night_end_hour;
    end_minute_ = cfg.auto_night_end_minute;
    clampTimes();
    prefs_dirty_ = false;
    ui_dirty_ = true;
}

void NightModeManager::savePrefs(StorageManager &storage) {
    if (!prefs_dirty_) {
        return;
    }
    auto &cfg = storage.config();
    cfg.auto_night_enabled = auto_enabled_;
    cfg.auto_night_start_hour = start_hour_;
    cfg.auto_night_start_minute = start_minute_;
    cfg.auto_night_end_hour = end_hour_;
    cfg.auto_night_end_minute = end_minute_;
    storage.saveConfig(true);
    prefs_dirty_ = false;
}

void NightModeManager::setAutoEnabled(bool enabled) {
    if (enabled == auto_enabled_) {
        return;
    }
    auto_enabled_ = enabled;
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void NightModeManager::adjustStartHour(int delta) {
    start_hour_ = wrap_value(start_hour_ + delta, 24);
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void NightModeManager::adjustStartMinute(int delta) {
    start_minute_ = wrap_value(start_minute_ + delta, 60);
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void NightModeManager::adjustEndHour(int delta) {
    end_hour_ = wrap_value(end_hour_ + delta, 24);
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void NightModeManager::adjustEndMinute(int delta) {
    end_minute_ = wrap_value(end_minute_ + delta, 60);
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

bool NightModeManager::applyNow(bool night_mode, bool &desired_night_mode) {
    return applyInternal(night_mode, desired_night_mode);
}

bool NightModeManager::poll(bool night_mode, bool &desired_night_mode) {
    if (!auto_enabled_) {
        return false;
    }
    uint32_t now_ms = millis();
    if (now_ms - last_check_ms_ < Config::AUTO_NIGHT_POLL_MS) {
        return false;
    }
    last_check_ms_ = now_ms;
    return applyInternal(night_mode, desired_night_mode);
}

bool NightModeManager::applyInternal(bool night_mode, bool &desired_night_mode) {
    if (!auto_enabled_) {
        auto_active_ = false;
        return false;
    }
    time_t now = time(nullptr);
    if (now <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    tm local_tm = {};
    localtime_r(&now, &local_tm);
    bool active = isActive(local_tm);
    if (active != auto_active_ || active != night_mode) {
        auto_active_ = active;
        desired_night_mode = active;
        return true;
    }
    return false;
}

bool NightModeManager::isActive(const tm &local_tm) const {
    int now_min = local_tm.tm_hour * 60 + local_tm.tm_min;
    int start_min = start_hour_ * 60 + start_minute_;
    int end_min = end_hour_ * 60 + end_minute_;
    if (start_min == end_min) {
        return false;
    }
    if (start_min < end_min) {
        return now_min >= start_min && now_min < end_min;
    }
    return now_min >= start_min || now_min < end_min;
}

void NightModeManager::updateUi() {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", start_hour_);
    if (objects.label_auto_night_start_hours_value) {
        safe_label_set_text(objects.label_auto_night_start_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", start_minute_);
    if (objects.label_auto_night_start_minutes_value) {
        safe_label_set_text(objects.label_auto_night_start_minutes_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", end_hour_);
    if (objects.label_auto_night_end_hours_value) {
        safe_label_set_text(objects.label_auto_night_end_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", end_minute_);
    if (objects.label_auto_night_end_minutes_value) {
        safe_label_set_text(objects.label_auto_night_end_minutes_value, buf);
    }
    toggle_syncing_ = true;
    if (objects.btn_auto_night_toggle) {
        if (auto_enabled_) {
            lv_obj_add_state(objects.btn_auto_night_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_auto_night_toggle, LV_STATE_CHECKED);
        }
    }
    toggle_syncing_ = false;
    ui_dirty_ = false;
}

void NightModeManager::clampTimes() {
    if (start_hour_ < 0 || start_hour_ > 23) start_hour_ = 21;
    if (end_hour_ < 0 || end_hour_ > 23) end_hour_ = 7;
    if (start_minute_ < 0 || start_minute_ > 59) start_minute_ = 0;
    if (end_minute_ < 0 || end_minute_ > 59) end_minute_ = 0;
}
