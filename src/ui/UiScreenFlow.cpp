// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiScreenFlow.h"

#include "core/Logger.h"
#include "modules/NetworkManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/UiBootFlow.h"
#include "ui/UiController.h"
#include "ui/UiEventBinder.h"
#include "ui/ThemeManager.h"
#include "ui/ui.h"

#if !defined(EEZ_FOR_LVGL)
extern "C" void loadScreen(enum ScreensEnum screenId);
extern "C" void unloadScreen(enum ScreensEnum screenId);
#endif

void UiScreenFlow::processPendingScreen(UiController &owner, uint32_t now_ms) {
    bool refresh_status_icons_after_switch = false;
    if (owner.pending_screen_id != 0) {
        int next_screen = owner.pending_screen_id;
        int previous_screen = owner.current_screen_id;
        ScreensEnum next_screen_enum = static_cast<ScreensEnum>(next_screen);
        loadScreen(next_screen_enum);
        if (!UiEventBinder::screenRootById(next_screen)) {
            if (next_screen == SCREEN_ID_PAGE_MQTT) {
                owner.setMqttScreenOpenState(false);
            } else if (next_screen == SCREEN_ID_PAGE_THEME) {
                owner.setThemeScreenOpenState(false);
            }
            LOGW("UI", "screen %d is unavailable after load request", next_screen);
            owner.pending_screen_id = 0;
        } else {
            bool was_bound = (next_screen > 0 && next_screen < static_cast<int>(owner.kScreenSlotCount))
                ? owner.screen_events_bound_[next_screen]
                : true;
            owner.bind_screen_events_once(next_screen);
            refresh_status_icons_after_switch = !was_bound;
            owner.current_screen_id = next_screen;
            owner.pending_screen_id = 0;
            owner.reset_dynamic_url_caches();

            // Lazily rebuilt screens can be released on exit.
            // Delay unload slightly to avoid racing with screen activation.
            owner.deferred_unload_.scheduleOnSwitch(previous_screen, owner.current_screen_id, now_ms);

            if (previous_screen == SCREEN_ID_PAGE_SENSORS_INFO &&
                owner.current_screen_id != SCREEN_ID_PAGE_SENSORS_INFO) {
                owner.release_all_sensor_graph_runtime_objects();
            }

            if (owner.current_screen_id == SCREEN_ID_PAGE_SETTINGS) {
                owner.temp_offset_ui_dirty = true;
                owner.hum_offset_ui_dirty = true;
                owner.data_dirty = true;
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_MAIN_PRO) {
                owner.data_dirty = true;
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_SENSORS_INFO) {
                owner.data_dirty = true;
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_CLOCK) {
                owner.datetime_ui_dirty = true;
                owner.clock_ui_dirty = true;
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_WIFI) {
                owner.markWifiUiDirty();
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_BACKLIGHT) {
                owner.backlightManager.markUiDirty();
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_AUTO_NIGHT_MODE) {
                owner.nightModeManager.markUiDirty();
            } else if (owner.current_screen_id == SCREEN_ID_PAGE_DAC_SETTINGS) {
                owner.dac_auto_tab_selected_ = false;
                owner.last_dac_ui_update_ms = 0;
            }

            if (owner.current_screen_id == SCREEN_ID_PAGE_MAIN_PRO &&
                !owner.boot_ui_released &&
                (objects.page_boot_logo || objects.page_boot_diag)) {
                owner.boot_release_at_ms = now_ms + 500;
            }
        }
    }

    if (refresh_status_icons_after_switch) {
        owner.wifi_icon_state = -1;
        owner.mqtt_icon_state = -1;
        owner.wifi_icon_state_main = -1;
        owner.mqtt_icon_state_main = -1;
        owner.update_status_icons();
    }
}

void UiScreenFlow::processBootRelease(UiController &owner, uint32_t now_ms) {
    if (!owner.boot_ui_released &&
        owner.boot_release_at_ms != 0 &&
        owner.pending_screen_id == 0 &&
        owner.current_screen_id == SCREEN_ID_PAGE_MAIN_PRO &&
        now_ms >= owner.boot_release_at_ms) {
        UiBootFlow::releaseBootScreens(owner);
    }
}

void UiScreenFlow::processDeferredUnloads(UiController &owner, uint32_t now_ms) {
    for (size_t i = 0; i < owner.deferred_unload_.count(); ++i) {
        if (!owner.deferred_unload_.ready(i, now_ms, owner.pending_screen_id, owner.current_screen_id)) {
            continue;
        }

        const int unload_screen_id = owner.deferred_unload_.screenId(i);
        unloadScreen(static_cast<ScreensEnum>(unload_screen_id));
        if (!UiEventBinder::screenRootById(unload_screen_id)) {
            if (unload_screen_id > 0 &&
                unload_screen_id < static_cast<int>(owner.kScreenSlotCount)) {
                owner.screen_events_bound_[unload_screen_id] = false;
            }
            if (unload_screen_id == SCREEN_ID_PAGE_THEME) {
                // Theme screen is rebuilt lazily, so its swatch callbacks must be rebound.
                owner.theme_events_bound_ = false;
            }
            owner.deferred_unload_.clear(i);
        } else {
            // Screen switch may still be settling; retry shortly.
            owner.deferred_unload_.retry(i, now_ms);
        }
    }
}
