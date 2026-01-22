#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: style_text_primary
//

void init_style_style_text_primary_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xffffe5a8));
};

lv_style_t *get_style_style_text_primary_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_text_primary_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_text_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_text_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_text_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_text_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: style_card_base
//

void init_style_style_card_base_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0xff160c09));
    lv_style_set_border_color(style, lv_color_hex(0xffe19756));
    lv_style_set_radius(style, 15);
    lv_style_set_shadow_color(style, lv_color_hex(0xffd97706));
    lv_style_set_shadow_opa(style, 230);
    lv_style_set_shadow_width(style, 20);
    lv_style_set_shadow_spread(style, 1);
    lv_style_set_bg_grad_color(style, lv_color_hex(0xff000000));
    lv_style_set_border_width(style, 1);
};

lv_style_t *get_style_style_card_base_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_card_base_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_card_base(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_card_base_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_card_base(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_card_base_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: style_screen_bg
//

void init_style_style_screen_bg_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0xff130b08));
    lv_style_set_bg_grad_color(style, lv_color_hex(0xff000000));
};

lv_style_t *get_style_style_screen_bg_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_screen_bg_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_screen_bg(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_screen_bg_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_screen_bg(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_screen_bg_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: style_preview_screen_bg
//

void init_style_style_preview_screen_bg_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0xff130b08));
    lv_style_set_bg_grad_color(style, lv_color_hex(0xff000000));
};

lv_style_t *get_style_style_preview_screen_bg_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_preview_screen_bg_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_preview_screen_bg(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_preview_screen_bg_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_preview_screen_bg(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_preview_screen_bg_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: style_preview_card_base
//

void init_style_style_preview_card_base_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_border_color(style, lv_color_hex(0xffe19756));
    lv_style_set_bg_color(style, lv_color_hex(0xff160c09));
    lv_style_set_shadow_width(style, 20);
    lv_style_set_shadow_spread(style, 1);
    lv_style_set_bg_grad_color(style, lv_color_hex(0xff000000));
    lv_style_set_shadow_opa(style, 230);
    lv_style_set_shadow_color(style, lv_color_hex(0xffd97706));
};

lv_style_t *get_style_style_preview_card_base_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_preview_card_base_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_preview_card_base(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_preview_card_base_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_preview_card_base(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_preview_card_base_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: style_preview_text_primary
//

void init_style_style_preview_text_primary_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xffffe5a8));
};

lv_style_t *get_style_style_preview_text_primary_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_style_preview_text_primary_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_style_preview_text_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_style_preview_text_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_style_preview_text_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_style_preview_text_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_style_text_primary,
        add_style_style_card_base,
        add_style_style_screen_bg,
        add_style_style_preview_screen_bg,
        add_style_style_preview_card_base,
        add_style_style_preview_text_primary,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_style_text_primary,
        remove_style_style_card_base,
        remove_style_style_screen_bg,
        remove_style_style_preview_screen_bg,
        remove_style_style_preview_card_base,
        remove_style_style_preview_text_primary,
    };
    remove_style_funcs[styleIndex](obj);
}

