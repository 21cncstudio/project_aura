#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "config/AppConfig.h"
#include "config/AppData.h"

class StorageManager;

class ThemeManager {
public:
    ThemeManager();

    void loadFromPrefs(StorageManager &storage);
    void initAfterUi(StorageManager &storage, bool night_mode, bool &datetime_ui_dirty);
    void registerEvents(void (*apply_toggle_style)(lv_obj_t *),
                        lv_event_cb_t swatch_cb,
                        lv_event_cb_t tab_cb);
    void syncPreviewWithCurrent();
    void applyPreviewFromSwatch(const ThemeSwatch &swatch);
    void applyPreviewCustom(const ThemeColors &colors);
    void applyPreviewAsCurrent(StorageManager &storage, bool night_mode, bool &datetime_ui_dirty);
    void applyActive(bool night_mode, bool &datetime_ui_dirty);
    void selectSwatchByCurrent();
    bool hasPreview() const { return preview_valid_; }
    bool hasUnsavedPreview() const;
    bool isCurrentPreset() const { return selected_index_ >= 0; }
    void setThemeScreenOpen(bool open) { theme_screen_open_ = open; }
    void setCustomTabSelected(bool selected) { custom_tab_selected_ = selected; }
    bool isCustomScreenOpen() const { return theme_screen_open_ && custom_tab_selected_; }
    ThemeColors previewOrCurrent() const;
    lv_color_t activeTextColor(bool night_mode) const;

private:
    void applyPreview(const ThemeColors &colors);
    void applyMain(const ThemeColors &colors);
    void saveToPrefs(StorageManager &storage, const ThemeColors &colors);
    bool readFromSwatch(const ThemeSwatch &swatch, ThemeColors &out) const;
    bool readFromUi(ThemeColors &out) const;
    void selectSwatchByColors(const ThemeColors &colors);
    void setSelectedSwatch(const ThemeSwatch *selected);
    bool colorsEqual(const ThemeColors &a, const ThemeColors &b) const;
    void initSwatches();

    ThemeColors current_ = {};
    ThemeColors preview_ = {};
    ThemeColors saved_ = {};
    ThemeColors night_ = {};
    ThemeSwatch swatches_[Config::THEME_SWATCH_COUNT] = {};
    bool preview_valid_ = false;
    bool saved_valid_ = false;
    int selected_index_ = -1;
    bool theme_screen_open_ = false;
    bool custom_tab_selected_ = false;
};
