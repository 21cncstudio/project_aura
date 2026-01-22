// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>
#include "config/AppConfig.h"

namespace esp_panel {
namespace drivers {
class Backlight;
} // namespace drivers
} // namespace esp_panel

class StorageManager;

class BacklightManager {
public:
    void loadFromPrefs(StorageManager &storage);
    void attachBacklight(esp_panel::drivers::Backlight *backlight);
    void poll(bool lvgl_ready);
    void updateUi();
    void savePrefs(StorageManager &storage);

    void setOn(bool on);
    bool isOn() const { return backlight_on_; }

    void setTimeoutMs(uint32_t timeout_ms);
    void setScheduleEnabled(bool enabled);
    void adjustSleepHour(int delta);
    void adjustSleepMinute(int delta);
    void adjustWakeHour(int delta);
    void adjustWakeMinute(int delta);

    void markUiDirty() { ui_dirty_ = true; }
    bool isUiDirty() const { return ui_dirty_; }
    bool isPresetSyncing() const { return preset_syncing_; }
    bool isScheduleSyncing() const { return schedule_syncing_; }

private:
    uint32_t normalizeTimeoutMs(uint32_t timeout_ms) const;
    void storeSchedulePrefs();
    void refreshSchedule();
    void consumeInput();

    esp_panel::drivers::Backlight *panel_backlight_ = nullptr;
    bool backlight_on_ = true;
    uint32_t backlight_timeout_ms_ = 0;
    bool schedule_enabled_ = false;
    bool schedule_active_ = false;
    int sleep_hour_ = 23;
    int sleep_minute_ = 0;
    int wake_hour_ = 6;
    int wake_minute_ = 0;
    uint32_t last_inactive_ms_ = 0;
    uint32_t block_input_until_ms_ = 0;
    bool ui_dirty_ = true;
    bool preset_syncing_ = false;
    bool schedule_syncing_ = false;
    bool prefs_dirty_ = false;
};
