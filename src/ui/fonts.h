#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t ui_font_jet_med_72;
extern const lv_font_t ui_font_jet_med_48;
extern const lv_font_t ui_font_jet_reg_14;
extern const lv_font_t ui_font_jet_reg_18;
extern const lv_font_t ui_font_jet_med_28;
extern const lv_font_t ui_font_noto_sans_sc_reg_14;
extern const lv_font_t ui_font_noto_sans_sc_reg_18;
// PROJECT_AURA_MANAGED_BEGIN: external-japanese-fonts
extern const lv_font_t ui_font_noto_sans_jp_reg_14;
extern const lv_font_t ui_font_noto_sans_jp_reg_18;
// PROJECT_AURA_MANAGED_END: external-japanese-fonts

#ifndef EXT_FONT_DESC_T
#define EXT_FONT_DESC_T
typedef struct _ext_font_desc_t {
    const char *name;
    const void *font_ptr;
} ext_font_desc_t;
#endif

extern ext_font_desc_t fonts[];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_FONTS_H*/