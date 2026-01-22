#pragma once
#include <Arduino.h>
#include "config/AppConfig.h"

class StorageManager;

class NightModeManager {
public:
    void loadFromPrefs(StorageManager &storage);
    void savePrefs(StorageManager &storage);

    void setAutoEnabled(bool enabled);
    void adjustStartHour(int delta);
    void adjustStartMinute(int delta);
    void adjustEndHour(int delta);
    void adjustEndMinute(int delta);

    bool applyNow(bool night_mode, bool &desired_night_mode);
    bool poll(bool night_mode, bool &desired_night_mode);

    void updateUi();

    void markUiDirty() { ui_dirty_ = true; }
    bool isUiDirty() const { return ui_dirty_; }
    bool isToggleSyncing() const { return toggle_syncing_; }
    bool isAutoEnabled() const { return auto_enabled_; }

private:
    bool applyInternal(bool night_mode, bool &desired_night_mode);
    bool isActive(const tm &local_tm) const;
    void clampTimes();

    bool auto_enabled_ = false;
    bool auto_active_ = false;
    int start_hour_ = 21;
    int start_minute_ = 0;
    int end_hour_ = 7;
    int end_minute_ = 0;
    uint32_t last_check_ms_ = 0;
    bool ui_dirty_ = true;
    bool toggle_syncing_ = false;
    bool prefs_dirty_ = false;
};
