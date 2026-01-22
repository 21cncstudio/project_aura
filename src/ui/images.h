#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_project_aura_logo;
extern const lv_img_dsc_t img_wifi_green;
extern const lv_img_dsc_t img_wifi_red;
extern const lv_img_dsc_t img_wifi_yellow;
extern const lv_img_dsc_t img_wifi_blue;
extern const lv_img_dsc_t img_home_green;
extern const lv_img_dsc_t img_home_red;
extern const lv_img_dsc_t img_home_blue;
extern const lv_img_dsc_t img_home_yellow;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[9];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/