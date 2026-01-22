#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: style_text_primary
lv_style_t *get_style_style_text_primary_MAIN_DEFAULT();
void add_style_style_text_primary(lv_obj_t *obj);
void remove_style_style_text_primary(lv_obj_t *obj);

// Style: style_card_base
lv_style_t *get_style_style_card_base_MAIN_DEFAULT();
void add_style_style_card_base(lv_obj_t *obj);
void remove_style_style_card_base(lv_obj_t *obj);

// Style: style_screen_bg
lv_style_t *get_style_style_screen_bg_MAIN_DEFAULT();
void add_style_style_screen_bg(lv_obj_t *obj);
void remove_style_style_screen_bg(lv_obj_t *obj);

// Style: style_preview_screen_bg
lv_style_t *get_style_style_preview_screen_bg_MAIN_DEFAULT();
void add_style_style_preview_screen_bg(lv_obj_t *obj);
void remove_style_style_preview_screen_bg(lv_obj_t *obj);

// Style: style_preview_card_base
lv_style_t *get_style_style_preview_card_base_MAIN_DEFAULT();
void add_style_style_preview_card_base(lv_obj_t *obj);
void remove_style_style_preview_card_base(lv_obj_t *obj);

// Style: style_preview_text_primary
lv_style_t *get_style_style_preview_text_primary_MAIN_DEFAULT();
void add_style_style_preview_text_primary(lv_obj_t *obj);
void remove_style_style_preview_text_primary(lv_obj_t *obj);



#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/