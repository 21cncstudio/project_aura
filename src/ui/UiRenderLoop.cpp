// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiRenderLoop.h"

#include "config/AppConfig.h"
#include "modules/NetworkManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/UiBootFlow.h"
#include "ui/UiController.h"
#include "ui/ui.h"

using namespace Config;

namespace {

constexpr uint32_t DIAG_LOG_UPDATE_MS = 750;
constexpr uint32_t SETTINGS_HEADER_UPDATE_MS = 750;

} // namespace

void UiRenderLoop::process(UiController &owner, uint32_t now_ms) {
    bool allow_ui_update = true;
    if (owner.connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG) &&
        (now_ms - owner.last_ui_update_ms) < WIFI_UI_UPDATE_MS) {
        allow_ui_update = false;
    }

    if (allow_ui_update &&
        owner.current_screen_id == SCREEN_ID_PAGE_BOOT_DIAG &&
        (now_ms - owner.last_boot_diag_update_ms) >= 200) {
        UiBootFlow::updateBootDiag(owner, now_ms);
        owner.last_boot_diag_update_ms = now_ms;
    }

    if (!allow_ui_update) {
        return;
    }

    bool did_update = false;
    if (owner.temp_offset_ui_dirty) {
        owner.update_temp_offset_label();
        owner.temp_offset_ui_dirty = false;
        did_update = true;
    }
    if (owner.hum_offset_ui_dirty) {
        owner.update_hum_offset_label();
        owner.hum_offset_ui_dirty = false;
        did_update = true;
    }
    if (owner.consumeNetworkUiDirty()) {
        owner.update_wifi_ui();
        did_update = true;
    }
    if (owner.consumeMqttUiDirty()) {
        owner.update_mqtt_ui();
        did_update = true;
    }
    if (owner.clock_ui_dirty) {
        owner.update_clock_labels();
        owner.clock_ui_dirty = false;
        did_update = true;
    }
    if (owner.datetime_ui_dirty && owner.current_screen_id == SCREEN_ID_PAGE_CLOCK) {
        owner.update_datetime_ui();
        owner.datetime_ui_dirty = false;
        did_update = true;
    }
    if (owner.web_page_panel_dirty &&
        owner.current_screen_id == SCREEN_ID_PAGE_SETTINGS &&
        objects.container_web_page &&
        !lv_obj_has_flag(objects.container_web_page, LV_OBJ_FLAG_HIDDEN)) {
        owner.update_web_page_panel();
        did_update = true;
    }
    if (owner.backlightManager.isUiDirty() && owner.current_screen_id == SCREEN_ID_PAGE_BACKLIGHT) {
        owner.backlightManager.updateUi();
        did_update = true;
    }
    if (owner.nightModeManager.isUiDirty() && owner.current_screen_id == SCREEN_ID_PAGE_AUTO_NIGHT_MODE) {
        owner.nightModeManager.updateUi();
        did_update = true;
    }
    if ((owner.current_screen_id == SCREEN_ID_PAGE_SETTINGS ||
         owner.current_screen_id == SCREEN_ID_PAGE_DAC_SETTINGS) &&
        (now_ms - owner.last_dac_ui_update_ms) >= 200) {
        owner.update_dac_ui(now_ms);
        owner.last_dac_ui_update_ms = now_ms;
        did_update = true;
    }
    if (owner.current_screen_id == SCREEN_ID_PAGE_SETTINGS &&
        (now_ms - owner.last_settings_header_update_ms) >= SETTINGS_HEADER_UPDATE_MS) {
        owner.update_settings_header();
        owner.last_settings_header_update_ms = now_ms;
        did_update = true;
    }
    if (owner.current_screen_id == SCREEN_ID_PAGE_DIAG &&
        (now_ms - owner.last_diag_log_update_ms) >= DIAG_LOG_UPDATE_MS) {
        owner.update_diag_log_ui();
        owner.last_diag_log_update_ms = now_ms;
        did_update = true;
    }
    if (owner.data_dirty) {
        if (owner.current_screen_id == SCREEN_ID_PAGE_MAIN_PRO) {
            owner.update_ui();
        } else if (owner.current_screen_id == SCREEN_ID_PAGE_SETTINGS) {
            owner.update_settings_header();
        } else if (owner.current_screen_id == SCREEN_ID_PAGE_SENSORS_INFO) {
            owner.update_sensor_info_ui();
        }
        owner.data_dirty = false;
        did_update = true;
    }
    if (did_update) {
        owner.last_ui_update_ms = now_ms;
    }
}
