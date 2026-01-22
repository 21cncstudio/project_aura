// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/AppInit.h"

#include <esp_display_panel.hpp>
#include <esp_system.h>

#include "config/AppConfig.h"
#include "core/BootPolicy.h"
#include "core/BootHelpers.h"
#include "core/BootState.h"
#include "core/BoardInit.h"
#include "core/InitConfig.h"
#include "core/Logger.h"
#include "lvgl_v8_port.h"

namespace {

UiController *g_ui_controller = nullptr;

struct WifiStateContext {
    AuraNetworkManager *network = nullptr;
    TimeManager *time_manager = nullptr;
    UiController *ui_controller = nullptr;
};

WifiStateContext g_wifi_state_ctx;

void mqtt_sync_with_wifi_cb() {
    if (g_ui_controller) {
        g_ui_controller->mqtt_sync_with_wifi();
    }
}

void wifi_state_change_cb(AuraNetworkManager::WifiState,
                          AuraNetworkManager::WifiState,
                          bool connected,
                          void *ctx) {
    auto *state = static_cast<WifiStateContext *>(ctx);
    if (!state || !state->network || !state->time_manager || !state->ui_controller) {
        return;
    }
    state->time_manager->updateWifiState(state->network->isEnabled(), connected);
    state->ui_controller->markDatetimeDirty();
    state->ui_controller->mqtt_sync_with_wifi();
}

} // namespace

StorageManager::BootAction AppInit::handleBootState() {
    esp_reset_reason_t reset_reason = esp_reset_reason();
    boot_reset_reason = reset_reason;
    bool crash_reset = BootHelpers::isCrashReset(reset_reason);
    StorageManager::BootAction boot_action =
        BootPolicy::apply(crash_reset,
                          boot_count,
                          safe_boot_stage,
                          Config::SAFE_BOOT_MAX_REBOOTS);
    LOGI("Main", "Reset reason: %d, boot count: %u", reset_reason, boot_count);
    if (boot_action == StorageManager::BootAction::SafeRollback) {
        LOGW("Main", "SAFE BOOT: restoring last known good config");
    } else if (boot_action == StorageManager::BootAction::SafeFactoryReset) {
        LOGE("Main", "SAFE BOOT: factory reset");
    }
    return boot_action;
}

bool AppInit::recoverI2cBus(gpio_num_t sda, gpio_num_t scl) {
    boot_i2c_recovered = BootHelpers::recoverI2CBus(sda, scl);
    if (!boot_i2c_recovered) {
        LOGW("Main", "I2C bus recovery failed");
    } else {
        LOGI("Main", "I2C bus recovered");
    }
    return boot_i2c_recovered;
}

void AppInit::initManagersAndConfig(Context &ctx, StorageManager::BootAction boot_action) {
    ctx.storage.begin(boot_action);
    ctx.networkManager.begin(ctx.storage);
    ctx.mqttManager.begin(ctx.storage, ctx.networkManager);

    g_ui_controller = &ctx.uiController;
    ctx.networkManager.attachMqttContext(
        ctx.mqttManager.client(),
        ctx.mqttManager.userEnabledRef(),
        ctx.mqttManager.connectFailCountRef(),
        ctx.mqttManager.hostRef(),
        ctx.mqttManager.portRef(),
        ctx.mqttManager.userRef(),
        ctx.mqttManager.passRef(),
        ctx.mqttManager.deviceNameRef(),
        ctx.mqttManager.baseTopicRef(),
        ctx.mqttManager.deviceIdRef(),
        ctx.mqttManager.discoveryRef(),
        ctx.mqttManager.anonymousRef(),
        mqtt_sync_with_wifi_cb);
    ctx.networkManager.attachThemeContext(ctx.themeManager);
    g_wifi_state_ctx.network = &ctx.networkManager;
    g_wifi_state_ctx.time_manager = &ctx.timeManager;
    g_wifi_state_ctx.ui_controller = &ctx.uiController;
    ctx.networkManager.setStateChangeCallback(wifi_state_change_cb, &g_wifi_state_ctx);

    const auto &cfg = ctx.storage.config();
    ctx.temp_offset = cfg.temp_offset;
    ctx.hum_offset = cfg.hum_offset;
    InitConfig::normalizeOffsets(ctx.temp_offset, ctx.hum_offset);
    ctx.temp_units_c = cfg.units_c;
    ctx.night_mode = cfg.night_mode;
    ctx.led_indicators_enabled = cfg.led_indicators;
    ctx.alert_blink_enabled = cfg.alert_blink;
    ctx.backlightManager.loadFromPrefs(ctx.storage);
    ctx.timeManager.begin(ctx.storage);
    ctx.nightModeManager.loadFromPrefs(ctx.storage);
    ctx.co2_asc_enabled = cfg.asc_enabled;
    ctx.themeManager.loadFromPrefs(ctx.storage);

    ctx.timeManager.updateWifiState(ctx.networkManager.isEnabled(), ctx.networkManager.isConnected());
    ctx.uiController.mqtt_sync_with_wifi();
    ctx.mqttManager.updateNightModeAvailability(ctx.nightModeManager.isAutoEnabled());
}

esp_panel::board::Board *AppInit::initBoardAndPeripherals(Context &ctx) {
    esp_panel::board::Board *board = BoardInit::initBoard();
    ctx.backlightManager.attachBacklight(board->getBacklight());
    ctx.timeManager.initRtc();
    ctx.pressureHistory.load(ctx.storage, ctx.currentData);
    ctx.uiController.apply_auto_night_now();

    BootHelpers::logGt911Address();
    ctx.sensorManager.begin(ctx.storage, ctx.temp_offset, ctx.hum_offset);

    return board;
}

bool AppInit::initLvglAndUi(Context &ctx, esp_panel::board::Board *board) {
    LOGI("Main", "Initializing LVGL");
    bool lvgl_ready = lvgl_port_init(board->getLCD(), board->getTouch());
    if (!lvgl_ready) {
        LOGE("Main", "LVGL init failed");
    }

    LOGI("Main", "Creating UI");
    ctx.uiController.setLvglReady(lvgl_ready);
    if (lvgl_ready) {
        ctx.uiController.begin();
    }
    return lvgl_ready;
}
