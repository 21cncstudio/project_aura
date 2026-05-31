#if defined(EEZ_FOR_LVGL)
#include <eez/core/vars.h>
#endif

#include "ui.h"
#include "screens.h"
#include "images.h"
#include "actions.h"
#include "vars.h"

#if defined(EEZ_FOR_LVGL)

void ui_init() {
    eez_flow_init(assets, sizeof(assets), (lv_obj_t **)&objects, sizeof(objects), images, sizeof(images), actions);
}

void ui_tick() {
    eez_flow_tick();
    tick_screen(g_currentScreen);
}

#else

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum { UI_KNOWN_SCREEN_COUNT = SCREEN_ID_PAGE_AURA_AQ_LINK };
enum { UI_PAGE_SLOT_COUNT = (int)(offsetof(objects_t, label_boot_ver) / sizeof(lv_obj_t *)) };
enum { UI_OBJECT_SLOT_COUNT = (int)(sizeof(objects_t) / sizeof(lv_obj_t *)) };

#if defined(__cplusplus)
#define UI_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define UI_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

UI_STATIC_ASSERT(UI_PAGE_SLOT_COUNT == UI_KNOWN_SCREEN_COUNT,
                 "EEZ page layout changed: update ui_runtime screen tables.");
UI_STATIC_ASSERT(SCREEN_ID_PAGE_AURA_AQ_LINK == UI_KNOWN_SCREEN_COUNT,
                 "Expected Aura AQ Link to be last screen id; update ui_runtime mapping.");
UI_STATIC_ASSERT(offsetof(objects_t, page_boot_logo) == 0,
                 "objects_t must start with page_boot_logo.");
UI_STATIC_ASSERT((offsetof(objects_t, page_aura_aq_link) + sizeof(lv_obj_t *)) ==
                    (UI_PAGE_SLOT_COUNT * sizeof(lv_obj_t *)),
                 "Page roots must stay contiguous; update ui_runtime mapping.");
UI_STATIC_ASSERT((sizeof(objects_t) % sizeof(lv_obj_t *)) == 0,
                 "objects_t must consist of lv_obj_t* slots only.");

enum { UI_MAX_SCREEN_ID = UI_KNOWN_SCREEN_COUNT };

static int16_t currentScreen = -1;
static uint8_t createdScreens[UI_MAX_SCREEN_ID + 1];

static bool isScreenIdValid(enum ScreensEnum screenId) {
    return screenId >= SCREEN_ID_PAGE_BOOT_LOGO && screenId <= UI_MAX_SCREEN_ID;
}

static lv_obj_t *getLvglObjectFromScreenId(enum ScreensEnum screenId) {
    switch (screenId) {
        case SCREEN_ID_PAGE_BOOT_LOGO:
            return objects.page_boot_logo;
        case SCREEN_ID_PAGE_BOOT_DIAG:
            return objects.page_boot_diag;
        case SCREEN_ID_PAGE_SETTINGS:
            return objects.page_settings;
        case SCREEN_ID_PAGE_WIFI:
            return objects.page_wifi;
        case SCREEN_ID_PAGE_THEME:
            return objects.page_theme;
        case SCREEN_ID_PAGE_CLOCK:
            return objects.page_clock;
        case SCREEN_ID_PAGE_CO2_CALIB:
            return objects.page_co2_calib;
        case SCREEN_ID_PAGE_AUTO_NIGHT_MODE:
            return objects.page_auto_night_mode;
        case SCREEN_ID_PAGE_BACKLIGHT:
            return objects.page_backlight;
        case SCREEN_ID_PAGE_MQTT:
            return objects.page_mqtt;
        case SCREEN_ID_PAGE_SENSORS_INFO:
            return objects.page_sensors_info;
        case SCREEN_ID_PAGE_DAC_SETTINGS:
            return objects.page_dac_settings;
        case SCREEN_ID_PAGE_FW_UPDATE:
            return objects.page_fw_update;
        case SCREEN_ID_PAGE_DIAG:
            return objects.page_diag;
        case SCREEN_ID_PAGE_AURA_AQ_LINK:
            return objects.page_aura_aq_link;
        case SCREEN_ID_PAGE_MAIN_PRO:
            return objects.page_main_pro;
        default:
            return 0;
    }
}

static void clearObjectRefsForScreen(lv_obj_t *screen) {
    if (!screen) {
        return;
    }
    bool screen_valid = lv_obj_is_valid(screen);
    lv_obj_t **slots = (lv_obj_t **)&objects;
    for (size_t i = 0; i < UI_OBJECT_SLOT_COUNT; ++i) {
        lv_obj_t *obj = slots[i];
        if (!obj) {
            continue;
        }
        if (obj == screen) {
            slots[i] = NULL;
            continue;
        }
        if (!lv_obj_is_valid(obj)) {
            // Drop stale references left after async deletion.
            slots[i] = NULL;
            continue;
        }
        if (screen_valid && lv_obj_get_screen(obj) == screen) {
            slots[i] = NULL;
        }
    }
}

typedef void (*create_screen_func_t)(void);

static const create_screen_func_t screen_create_funcs[UI_MAX_SCREEN_ID + 1] = {
    [SCREEN_ID_PAGE_BOOT_LOGO] = create_screen_page_boot_logo,
    [SCREEN_ID_PAGE_BOOT_DIAG] = create_screen_page_boot_diag,
    [SCREEN_ID_PAGE_MAIN_PRO] = create_screen_page_main_pro,
    [SCREEN_ID_PAGE_SETTINGS] = create_screen_page_settings,
    [SCREEN_ID_PAGE_WIFI] = create_screen_page_wifi,
    [SCREEN_ID_PAGE_THEME] = create_screen_page_theme,
    [SCREEN_ID_PAGE_CLOCK] = create_screen_page_clock,
    [SCREEN_ID_PAGE_CO2_CALIB] = create_screen_page_co2_calib,
    [SCREEN_ID_PAGE_AUTO_NIGHT_MODE] = create_screen_page_auto_night_mode,
    [SCREEN_ID_PAGE_BACKLIGHT] = create_screen_page_backlight,
    [SCREEN_ID_PAGE_MQTT] = create_screen_page_mqtt,
    [SCREEN_ID_PAGE_SENSORS_INFO] = create_screen_page_sensors_info,
    [SCREEN_ID_PAGE_DAC_SETTINGS] = create_screen_page_dac_settings,
    [SCREEN_ID_PAGE_FW_UPDATE] = create_screen_page_fw_update,
    [SCREEN_ID_PAGE_DIAG] = create_screen_page_diag,
    [SCREEN_ID_PAGE_AURA_AQ_LINK] = create_screen_page_aura_aq_link,
};

static void createScreenById(enum ScreensEnum screenId) {
    if (!isScreenIdValid(screenId)) {
        return;
    }
    create_screen_func_t create_fn = screen_create_funcs[screenId];
    if (create_fn) {
        create_fn();
    }
}

static bool isScreenEager(enum ScreensEnum screenId) {
    switch (screenId) {
        case SCREEN_ID_PAGE_BOOT_LOGO:
        case SCREEN_ID_PAGE_BOOT_DIAG:
        case SCREEN_ID_PAGE_MAIN_PRO:
        case SCREEN_ID_PAGE_SETTINGS:
            return true;
        default:
            return false;
    }
}

static void markCreatedScreensFromObjects(void) {
    for (int id = SCREEN_ID_PAGE_BOOT_LOGO; id <= UI_MAX_SCREEN_ID; ++id) {
        enum ScreensEnum screenId = (enum ScreensEnum)id;
        if (getLvglObjectFromScreenId(screenId)) {
            createdScreens[id] = 1;
        }
    }
}

static void ensureScreenCreated(enum ScreensEnum screenId) {
    if (!isScreenIdValid(screenId)) {
        return;
    }
    if (createdScreens[screenId]) {
        return;
    }
    if (getLvglObjectFromScreenId(screenId)) {
        createdScreens[screenId] = 1;
        return;
    }
    createScreenById(screenId);
    if (getLvglObjectFromScreenId(screenId)) {
        createdScreens[screenId] = 1;
    }
}

void loadScreen(enum ScreensEnum screenId) {
    if (!isScreenIdValid(screenId)) {
        return;
    }
    ensureScreenCreated(screenId);
    lv_obj_t *screen = getLvglObjectFromScreenId(screenId);
    if (!screen) {
        return;
    }
    currentScreen = screenId - 1;
    lv_scr_load(screen);
}

void unloadScreen(enum ScreensEnum screenId) {
    if (!isScreenIdValid(screenId)) {
        return;
    }
    if (isScreenEager(screenId)) {
        return;
    }
    if (currentScreen == screenId - 1) {
        return;
    }
    lv_obj_t *screen = getLvglObjectFromScreenId(screenId);
    if (!screen) {
        createdScreens[screenId] = 0;
        return;
    }
    if (!lv_obj_is_valid(screen)) {
        clearObjectRefsForScreen(screen);
        createdScreens[screenId] = 0;
        return;
    }
    if (screen == lv_scr_act()) {
        // Active screen cannot be unloaded in-place; retry later from caller side.
        return;
    }
    clearObjectRefsForScreen(screen);
    createdScreens[screenId] = 0;
    lv_obj_del_async(screen);
}

void ui_init() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              false,
                                              LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    memset(createdScreens, 0, sizeof(createdScreens));
    for (int id = SCREEN_ID_PAGE_BOOT_LOGO; id <= UI_MAX_SCREEN_ID; ++id) {
        enum ScreensEnum screenId = (enum ScreensEnum)id;
        if (isScreenEager(screenId)) {
            createScreenById(screenId);
        }
    }
    markCreatedScreensFromObjects();
    loadScreen(SCREEN_ID_PAGE_BOOT_LOGO);
}

void ui_tick() {
    if (currentScreen < 0) {
        return;
    }
    tick_screen(currentScreen);
}

#endif
