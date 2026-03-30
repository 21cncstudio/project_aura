// SPDX-FileCopyrightText: 2025-2026 netscout2001
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/UiController.h"
#include "modules/BatteryManager.h"
#include "ui/ui.h"
#include "ui/screens.h"
#include "ui/fonts.h"
#include <lvgl.h>
#include <stdio.h>

// ── Colours ───────────────────────────────────────────────────────────────────
#define BATT_GREEN   lv_color_hex(0x00c853)
#define BATT_CYAN    lv_color_hex(0x00E5FF)
#define BATT_BLUE    lv_color_hex(0x2196F3)
#define BATT_YELLOW  lv_color_hex(0xFFEB3B)
#define BATT_ORANGE  lv_color_hex(0xFF9800)
#define BATT_RED     lv_color_hex(0xF44336)

// ── Widget handles ────────────────────────────────────────────────────────────
static lv_obj_t*   s_batt      = nullptr;
static lv_obj_t*   s_bolt      = nullptr;
static lv_obj_t*   s_pct       = nullptr;
static lv_timer_t* s_blink_tmr = nullptr;
static bool        s_blink_on  = false;
static bool        s_created   = false;

static lv_color_t color_for_level(BattColorLevel lvl) {
    switch (lvl) {
        case BattColorLevel::Full:     return BATT_BLUE;
        case BattColorLevel::High:     return BATT_GREEN;
        case BattColorLevel::Medium:   return BATT_YELLOW;
        case BattColorLevel::Low:      return BATT_ORANGE;
        case BattColorLevel::Critical: return BATT_RED;
    }
    return BATT_GREEN;
}

static const char* symbol_for_level(BattColorLevel lvl) {
    switch (lvl) {
        case BattColorLevel::Full:     return LV_SYMBOL_BATTERY_FULL;
        case BattColorLevel::High:     return LV_SYMBOL_BATTERY_3;
        case BattColorLevel::Medium:   return LV_SYMBOL_BATTERY_2;
        case BattColorLevel::Low:      return LV_SYMBOL_BATTERY_1;
        case BattColorLevel::Critical: return LV_SYMBOL_BATTERY_EMPTY;
    }
    return LV_SYMBOL_BATTERY_FULL;
}

static void blink_cb(lv_timer_t*) {
    s_blink_on = !s_blink_on;
    if (s_bolt)
        lv_obj_set_style_opa(s_bolt, s_blink_on ? LV_OPA_COVER : LV_OPA_20, 0);
}

static void set_blink(bool en) {
    if (en) {
        if (!s_blink_tmr)
            s_blink_tmr = lv_timer_create(blink_cb, 600, nullptr);
    } else {
        if (s_blink_tmr) { lv_timer_del(s_blink_tmr); s_blink_tmr = nullptr; }
        if (s_bolt) lv_obj_set_style_opa(s_bolt, LV_OPA_COVER, 0);
        s_blink_on = false;
    }
}

// ── Lazy widget creation ──────────────────────────────────────────────────────
// Waits until the MQTT icon has been rendered (width > 0) before creating
static void ensure_created() {
    if (s_created) return;
    if (!objects.mqtt_status_icon_4) return;

    // Wait until the MQTT icon has been rendered (width > 0)
    const lv_coord_t ref_w = lv_obj_get_width(objects.mqtt_status_icon_4);
    const lv_coord_t ref_h = lv_obj_get_height(objects.mqtt_status_icon_4);
    if (ref_w == 0 || ref_h == 0) return;

    const lv_coord_t ref_x = lv_obj_get_x(objects.mqtt_status_icon_4);
    const lv_coord_t ref_y = lv_obj_get_y(objects.mqtt_status_icon_4);
    const lv_coord_t cy    = ref_y + ref_h / 2;

    lv_obj_t* parent = lv_obj_get_parent(objects.mqtt_status_icon_4);
    if (!parent) return;

    // Positions (right -> left):
    //   [MQTT] <- 8px <- [72% ~32px] <- 6px <- [BattSymbol ~20px]
    const lv_coord_t pct_x  = ref_x - 8 - 32;
    const lv_coord_t batt_x = pct_x - 6 - 20;

    // ── Battery symbol ────────────────────────────────────────────────────────
    s_batt = lv_label_create(parent);
    lv_label_set_text(s_batt, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(s_batt, BATT_GREEN, 0);
    lv_obj_set_style_text_font(s_batt, &lv_font_montserrat_16, 0);
    lv_obj_add_flag(s_batt, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(s_batt, batt_x, cy - 9);
    lv_obj_add_flag(s_batt, LV_OBJ_FLAG_HIDDEN);

    // ── Lightning bolt (overlaid on battery symbol) ───────────────────────────
    // montserrat_14 keeps it smaller than the battery outline
    s_bolt = lv_label_create(parent);
    lv_label_set_text(s_bolt, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(s_bolt, BATT_CYAN, 0);
    lv_obj_set_style_text_font(s_bolt, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(s_bolt, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(s_bolt, batt_x + 6, cy - 7);
    lv_obj_add_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);

    // ── Percentage label ──────────────────────────────────────────────────────
    s_pct = lv_label_create(parent);
    lv_label_set_text(s_pct, "100%");   // pre-allocate max width
    lv_obj_set_style_text_color(s_pct, BATT_GREEN, 0);
    lv_obj_set_style_text_font(s_pct, &lv_font_montserrat_16, 0);
    lv_obj_add_flag(s_pct, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(s_pct, pct_x, cy - 9);
    lv_obj_add_flag(s_pct, LV_OBJ_FLAG_HIDDEN);

    s_created = true;
}

// ── Public update function ────────────────────────────────────────────────────
void update_battery_icon() {
    ensure_created();
    if (!s_created) return;

    const BatteryState& st = BatteryManager::instance().state();

    // No sensor -> hide everything
    if (!st.detected) {
        lv_obj_add_flag(s_batt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pct,  LV_OBJ_FLAG_HIDDEN);
        set_blink(false);
        return;
    }

    // USB only (no battery) -> battery full green, bolt green static, no %
    if (!st.battery_present) {
        lv_label_set_text(s_batt, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(s_batt, BATT_GREEN, 0);
        lv_obj_clear_flag(s_batt, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_text_color(s_bolt, BATT_GREEN, 0);
        lv_obj_set_style_opa(s_bolt, LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(s_pct, LV_OBJ_FLAG_HIDDEN);
        set_blink(false);
        return;
    }

    // Battery present
    const lv_color_t lvl_col = color_for_level(st.color_level);

    lv_label_set_text(s_batt, symbol_for_level(st.color_level));
    lv_obj_set_style_text_color(s_batt, lvl_col, 0);
    lv_obj_clear_flag(s_batt, LV_OBJ_FLAG_HIDDEN);

    if (st.is_charging) {
        lv_obj_set_style_text_color(s_bolt, BATT_CYAN, 0);
        lv_obj_clear_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);
        set_blink(true);
    } else {
        lv_obj_add_flag(s_bolt, LV_OBJ_FLAG_HIDDEN);
        set_blink(false);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(st.percent));
    lv_label_set_text(s_pct, buf);
    lv_obj_set_style_text_color(s_pct, lvl_col, 0);
    lv_obj_clear_flag(s_pct, LV_OBJ_FLAG_HIDDEN);
}
