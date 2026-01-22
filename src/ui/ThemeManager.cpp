// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ThemeManager.h"
#include "modules/StorageManager.h"
#include "ui/ui.h"
#include "ui/styles.h"

namespace {

uint32_t themeColorToU32(lv_color_t color) {
    return static_cast<uint32_t>(color.full);
}

lv_color_t themeColorFromU32(uint32_t value) {
    lv_color_t color;
    color.full = value;
    return color;
}

} // namespace

ThemeManager::ThemeManager() {
    night_.screen_bg = lv_color_hex(0x000000);
    night_.card_bg = lv_color_hex(0x000000);
    night_.card_border = lv_color_hex(0x3f3f3f);
    night_.text_primary = lv_color_hex(0x8f8f8f);
    night_.shadow_color = night_.card_border;
    night_.shadow_enabled = true;
    night_.gradient_enabled = false;
    night_.gradient_color = night_.card_bg;
    night_.gradient_direction = LV_GRAD_DIR_NONE;
    night_.screen_gradient_enabled = false;
    night_.screen_gradient_color = night_.screen_bg;
    night_.screen_gradient_direction = LV_GRAD_DIR_NONE;
}

void ThemeManager::loadFromPrefs(StorageManager &storage) {
    const auto &cfg = storage.config();
    if (!cfg.theme.valid) {
        saved_valid_ = false;
        return;
    }
    saved_.screen_bg = themeColorFromU32(cfg.theme.screen_bg);
    saved_.card_bg = themeColorFromU32(cfg.theme.card_bg);
    saved_.card_border = themeColorFromU32(cfg.theme.card_border);
    saved_.text_primary = themeColorFromU32(cfg.theme.text_primary);
    saved_.shadow_color = themeColorFromU32(cfg.theme.shadow_color);
    saved_.shadow_enabled = cfg.theme.shadow_enabled;
    saved_.gradient_enabled = cfg.theme.gradient_enabled;
    saved_.gradient_color = themeColorFromU32(cfg.theme.gradient_color);
    saved_.gradient_direction = static_cast<lv_grad_dir_t>(cfg.theme.gradient_direction);
    saved_.screen_gradient_enabled = cfg.theme.screen_gradient_enabled;
    saved_.screen_gradient_color = themeColorFromU32(cfg.theme.screen_gradient_color);
    saved_.screen_gradient_direction =
        static_cast<lv_grad_dir_t>(cfg.theme.screen_gradient_direction);
    saved_valid_ = true;
}

void ThemeManager::initAfterUi(StorageManager &storage, bool night_mode, bool &datetime_ui_dirty) {
    ThemeColors detected = {};
    if (readFromUi(detected)) {
        current_ = detected;
    }
    if (saved_valid_) {
        current_ = saved_;
        applyMain(current_);
    } else {
        ThemeSwatch amber = {
            objects.btn_theme_industrial_amber,
            objects.card_theme_industrial_amber,
            objects.label_btn_theme_industrial_amber
        };
        ThemeColors amber_colors = {};
        if (readFromSwatch(amber, amber_colors)) {
            current_ = amber_colors;
            saved_ = amber_colors;
            saved_valid_ = true;
            applyMain(current_);
            saveToPrefs(storage, current_);
        }
    }
    syncPreviewWithCurrent();
    selectSwatchByColors(current_);
    applyActive(night_mode, datetime_ui_dirty);
}

void ThemeManager::registerEvents(void (*apply_toggle_style)(lv_obj_t *),
                                  lv_event_cb_t swatch_cb,
                                  lv_event_cb_t tab_cb) {
    initSwatches();
    for (size_t i = 0; i < Config::THEME_SWATCH_COUNT; i++) {
        if (!swatches_[i].btn) {
            continue;
        }
        lv_obj_add_flag(swatches_[i].btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_width(swatches_[i].btn, 2, LV_PART_MAIN | LV_STATE_CHECKED);
        if (apply_toggle_style) {
            apply_toggle_style(swatches_[i].btn);
        }
        if (swatch_cb) {
            lv_obj_add_event_cb(swatches_[i].btn, swatch_cb, LV_EVENT_CLICKED, &swatches_[i]);
        }
    }
    if (objects.btn_theme_presets) {
        if (apply_toggle_style) {
            apply_toggle_style(objects.btn_theme_presets);
        }
        lv_obj_add_state(objects.btn_theme_presets, LV_STATE_CHECKED);
        if (tab_cb) {
            lv_obj_add_event_cb(objects.btn_theme_presets, tab_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        }
    }
    if (objects.btn_theme_custom) {
        if (apply_toggle_style) {
            apply_toggle_style(objects.btn_theme_custom);
        }
        lv_obj_clear_state(objects.btn_theme_custom, LV_STATE_CHECKED);
        if (tab_cb) {
            lv_obj_add_event_cb(objects.btn_theme_custom, tab_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        }
    }
}

void ThemeManager::syncPreviewWithCurrent() {
    preview_ = current_;
    preview_valid_ = true;
    applyPreview(preview_);
}

void ThemeManager::applyPreviewFromSwatch(const ThemeSwatch &swatch) {
    ThemeColors colors = {};
    if (!readFromSwatch(swatch, colors)) {
        return;
    }
    preview_ = colors;
    preview_valid_ = true;
    applyPreview(preview_);
    setSelectedSwatch(&swatch);
}

void ThemeManager::applyPreviewCustom(const ThemeColors &colors) {
    preview_ = colors;
    preview_valid_ = true;
    applyPreview(preview_);
    setSelectedSwatch(nullptr);
}

void ThemeManager::applyPreviewAsCurrent(StorageManager &storage, bool night_mode,
                                         bool &datetime_ui_dirty) {
    if (!preview_valid_) {
        return;
    }
    current_ = preview_;
    applyActive(night_mode, datetime_ui_dirty);
    saveToPrefs(storage, current_);
}

void ThemeManager::applyActive(bool night_mode, bool &datetime_ui_dirty) {
    if (night_mode) {
        applyMain(night_);
    } else {
        applyMain(current_);
    }
    datetime_ui_dirty = true;
}

void ThemeManager::selectSwatchByCurrent() {
    selectSwatchByColors(current_);
}

ThemeColors ThemeManager::previewOrCurrent() const {
    return preview_valid_ ? preview_ : current_;
}

bool ThemeManager::hasUnsavedPreview() const {
    return preview_valid_ && !colorsEqual(preview_, current_);
}

lv_color_t ThemeManager::activeTextColor(bool night_mode) const {
    return night_mode ? night_.text_primary : current_.text_primary;
}

void ThemeManager::applyPreview(const ThemeColors &colors) {
    lv_style_t *text = get_style_style_preview_text_primary_MAIN_DEFAULT();
    lv_style_t *card = get_style_style_preview_card_base_MAIN_DEFAULT();
    lv_style_t *screen = get_style_style_preview_screen_bg_MAIN_DEFAULT();

    lv_style_set_text_color(text, colors.text_primary);
    lv_style_set_bg_color(card, colors.card_bg);
    lv_style_set_border_color(card, colors.card_border);
    lv_style_set_shadow_color(card, colors.shadow_color);
    lv_style_set_shadow_opa(card, colors.shadow_enabled ? LV_OPA_COVER : LV_OPA_TRANSP);
    lv_style_set_bg_grad_color(card, colors.gradient_color);
    lv_style_set_bg_grad_dir(card,
                             colors.gradient_enabled ? colors.gradient_direction : LV_GRAD_DIR_NONE);
    lv_style_set_bg_color(screen, colors.screen_bg);
    lv_style_set_bg_grad_color(screen, colors.screen_gradient_color);
    lv_style_set_bg_grad_dir(screen, colors.screen_gradient_enabled
                                         ? colors.screen_gradient_direction
                                         : LV_GRAD_DIR_NONE);

    lv_obj_report_style_change(text);
    lv_obj_report_style_change(card);
    lv_obj_report_style_change(screen);
}

void ThemeManager::applyMain(const ThemeColors &colors) {
    lv_style_t *text = get_style_style_text_primary_MAIN_DEFAULT();
    lv_style_t *card = get_style_style_card_base_MAIN_DEFAULT();
    lv_style_t *screen = get_style_style_screen_bg_MAIN_DEFAULT();

    lv_style_set_text_color(text, colors.text_primary);
    lv_style_set_bg_color(card, colors.card_bg);
    lv_style_set_border_color(card, colors.card_border);
    lv_style_set_shadow_color(card, colors.shadow_color);
    lv_style_set_shadow_opa(card, colors.shadow_enabled ? LV_OPA_COVER : LV_OPA_TRANSP);
    lv_style_set_bg_grad_color(card, colors.gradient_color);
    lv_style_set_bg_grad_dir(card,
                             colors.gradient_enabled ? colors.gradient_direction : LV_GRAD_DIR_NONE);
    lv_style_set_bg_color(screen, colors.screen_bg);
    lv_style_set_bg_grad_color(screen, colors.screen_gradient_color);
    lv_style_set_bg_grad_dir(screen, colors.screen_gradient_enabled
                                         ? colors.screen_gradient_direction
                                         : LV_GRAD_DIR_NONE);

    lv_obj_report_style_change(text);
    lv_obj_report_style_change(card);
    lv_obj_report_style_change(screen);
}

void ThemeManager::saveToPrefs(StorageManager &storage, const ThemeColors &colors) {
    auto &cfg = storage.config();
    cfg.theme.valid = true;
    cfg.theme.screen_bg = themeColorToU32(colors.screen_bg);
    cfg.theme.card_bg = themeColorToU32(colors.card_bg);
    cfg.theme.card_border = themeColorToU32(colors.card_border);
    cfg.theme.text_primary = themeColorToU32(colors.text_primary);
    cfg.theme.shadow_color = themeColorToU32(colors.shadow_color);
    cfg.theme.shadow_enabled = colors.shadow_enabled;
    cfg.theme.gradient_enabled = colors.gradient_enabled;
    cfg.theme.gradient_color = themeColorToU32(colors.gradient_color);
    cfg.theme.gradient_direction = static_cast<uint32_t>(colors.gradient_direction);
    cfg.theme.screen_gradient_enabled = colors.screen_gradient_enabled;
    cfg.theme.screen_gradient_color = themeColorToU32(colors.screen_gradient_color);
    cfg.theme.screen_gradient_direction = static_cast<uint32_t>(colors.screen_gradient_direction);
    storage.saveConfig(true);
}

bool ThemeManager::readFromSwatch(const ThemeSwatch &swatch, ThemeColors &out) const {
    if (!swatch.btn || !swatch.card || !swatch.label) {
        return false;
    }
    out.screen_bg = lv_obj_get_style_bg_color(swatch.btn, LV_PART_MAIN);
    out.screen_gradient_color = lv_obj_get_style_bg_grad_color(swatch.btn, LV_PART_MAIN);
    out.screen_gradient_direction = lv_obj_get_style_bg_grad_dir(swatch.btn, LV_PART_MAIN);
    out.screen_gradient_enabled = out.screen_gradient_direction != LV_GRAD_DIR_NONE;
    out.card_bg = lv_obj_get_style_bg_color(swatch.card, LV_PART_MAIN);
    out.card_border = lv_obj_get_style_border_color(swatch.card, LV_PART_MAIN);
    out.text_primary = lv_obj_get_style_text_color(swatch.label, LV_PART_MAIN);
    out.shadow_color = lv_obj_get_style_shadow_color(swatch.card, LV_PART_MAIN);
    out.shadow_enabled = lv_obj_get_style_shadow_opa(swatch.card, LV_PART_MAIN) > 0;
    out.gradient_color = lv_obj_get_style_bg_grad_color(swatch.card, LV_PART_MAIN);
    out.gradient_direction = lv_obj_get_style_bg_grad_dir(swatch.card, LV_PART_MAIN);
    out.gradient_enabled = out.gradient_direction != LV_GRAD_DIR_NONE;
    return true;
}

bool ThemeManager::readFromUi(ThemeColors &out) const {
    if (!objects.background || !objects.card_co2 || !objects.label_co2_value) {
        return false;
    }
    out.screen_bg = lv_obj_get_style_bg_color(objects.background, LV_PART_MAIN);
    out.screen_gradient_color = lv_obj_get_style_bg_grad_color(objects.background, LV_PART_MAIN);
    out.screen_gradient_direction = lv_obj_get_style_bg_grad_dir(objects.background, LV_PART_MAIN);
    out.screen_gradient_enabled = out.screen_gradient_direction != LV_GRAD_DIR_NONE;
    out.card_bg = lv_obj_get_style_bg_color(objects.card_co2, LV_PART_MAIN);
    out.card_border = lv_obj_get_style_border_color(objects.card_co2, LV_PART_MAIN);
    out.text_primary = lv_obj_get_style_text_color(objects.label_co2_value, LV_PART_MAIN);
    out.shadow_color = lv_obj_get_style_shadow_color(objects.card_co2, LV_PART_MAIN);
    out.shadow_enabled = lv_obj_get_style_shadow_opa(objects.card_co2, LV_PART_MAIN) > 0;
    out.gradient_color = lv_obj_get_style_bg_grad_color(objects.card_co2, LV_PART_MAIN);
    out.gradient_direction = lv_obj_get_style_bg_grad_dir(objects.card_co2, LV_PART_MAIN);
    out.gradient_enabled = out.gradient_direction != LV_GRAD_DIR_NONE;
    return true;
}

void ThemeManager::selectSwatchByColors(const ThemeColors &colors) {
    for (size_t i = 0; i < Config::THEME_SWATCH_COUNT; i++) {
        ThemeColors swatch_colors = {};
        if (!readFromSwatch(swatches_[i], swatch_colors)) {
            continue;
        }
        if (colorsEqual(colors, swatch_colors)) {
            setSelectedSwatch(&swatches_[i]);
            return;
        }
    }
    setSelectedSwatch(nullptr);
}

void ThemeManager::setSelectedSwatch(const ThemeSwatch *selected) {
    selected_index_ = -1;
    for (size_t i = 0; i < Config::THEME_SWATCH_COUNT; i++) {
        lv_obj_t *btn = swatches_[i].btn;
        if (!btn) {
            continue;
        }
        if (selected && btn == selected->btn) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            selected_index_ = static_cast<int>(i);
        } else {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }
}

bool ThemeManager::colorsEqual(const ThemeColors &a, const ThemeColors &b) const {
    if (a.screen_bg.full != b.screen_bg.full ||
        a.card_bg.full != b.card_bg.full ||
        a.card_border.full != b.card_border.full ||
        a.text_primary.full != b.text_primary.full ||
        a.shadow_enabled != b.shadow_enabled ||
        a.gradient_enabled != b.gradient_enabled ||
        a.screen_gradient_enabled != b.screen_gradient_enabled) {
        return false;
    }
    if (a.shadow_enabled && a.shadow_color.full != b.shadow_color.full) {
        return false;
    }
    if (a.gradient_enabled) {
        if (a.gradient_color.full != b.gradient_color.full ||
            a.gradient_direction != b.gradient_direction) {
            return false;
        }
    }
    if (a.screen_gradient_enabled) {
        if (a.screen_gradient_color.full != b.screen_gradient_color.full ||
            a.screen_gradient_direction != b.screen_gradient_direction) {
            return false;
        }
    }
    return true;
}

void ThemeManager::initSwatches() {
    swatches_[0] = { objects.btn_theme_industrial_amber, objects.card_theme_industrial_amber, objects.label_btn_theme_industrial_amber };
    swatches_[1] = { objects.btn_theme_nord_frost, objects.card_theme_nord_frost, objects.label_btn_theme_nord_frost };
    swatches_[2] = { objects.btn_theme_orbital_command, objects.card_theme_orbital_command, objects.label_btn_theme_orbital_command };
    swatches_[3] = { objects.btn_theme_vintage_sepia, objects.card_theme_vintage_sepia, objects.label_btn_theme_vintage_sepia };
    swatches_[4] = { objects.btn_theme_cappuccino_mocha, objects.card_theme_cappuccino_mocha, objects.label_btn_theme_cappuccino_mocha };
    swatches_[5] = { objects.btn_theme_eink_contrast, objects.card_theme_eink_contrast, objects.label_btn_theme_eink_contrast };
    swatches_[6] = { objects.btn_theme_nordic_eco, objects.card_theme_nordic_eco, objects.label_btn_theme_nordic_eco };
    swatches_[7] = { objects.btn_theme_dracula_classic, objects.card_theme_dracula_classic, objects.label_btn_theme_dracula_classic };
    swatches_[8] = { objects.btn_theme_everforest_dark, objects.card_theme_everforest_dark, objects.label_btn_theme_everforest_dark };
    swatches_[9] = { objects.btn_theme_matrix_terminal, objects.card_theme_matrix_terminal, objects.label_btn_theme_matrix_terminal };
    swatches_[10] = { objects.btn_theme_mars_pathfinder, objects.card_theme_mars_pathfinder, objects.label_btn_theme_mars_pathfinder };
    swatches_[11] = { objects.btn_theme_lunar_outpost, objects.card_theme_lunar_outpost, objects.label_btn_theme_lunar_outpost };
}
