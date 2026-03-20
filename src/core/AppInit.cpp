// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/AppInit.h"

#include <atomic>
#include <esp_display_panel.hpp>
#include <esp_log.h>
#include <esp_system.h>

#include "config/AppConfig.h"
#include "core/BootPolicy.h"
#include "core/BootHelpers.h"
#include "core/BootState.h"
#include "core/BoardInit.h"
#include "core/InitConfig.h"
#include "core/Logger.h"
#include "lvgl_v8_port.h"
#include "ui/UiStrings.h"

namespace {

MqttManager *g_mqtt_manager = nullptr;

struct WifiStateContext {
    AuraNetworkManager *network = nullptr;
    TimeManager *time_manager = nullptr;
    UiController *ui_controller = nullptr;
};

WifiStateContext g_wifi_state_ctx;

struct DeferredWifiRuntimeState {
    std::atomic<bool> pending{false};
    std::atomic<bool> wifi_enabled{false};
    std::atomic<bool> wifi_connected{false};
};

DeferredWifiRuntimeState g_deferred_wifi_runtime;

const char *resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
#ifdef ESP_RST_BROWNOUT
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
#endif
#ifdef ESP_RST_SDIO
        case ESP_RST_SDIO:      return "SDIO";
#endif
        default:                return "UNMAPPED";
    }
}

void mqtt_sync_with_wifi_cb() {
    if (g_mqtt_manager) {
        g_mqtt_manager->syncWithWifi();
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
    if (g_mqtt_manager) {
        g_mqtt_manager->syncWithWifi();
    }
    g_deferred_wifi_runtime.wifi_enabled.store(state->network->isEnabled(), std::memory_order_relaxed);
    g_deferred_wifi_runtime.wifi_connected.store(connected, std::memory_order_relaxed);
    g_deferred_wifi_runtime.pending.store(true, std::memory_order_release);
}

} // namespace

StorageManager::BootAction AppInit::handleBootState() {
    esp_reset_reason_t reset_reason = esp_reset_reason();
    boot_reset_reason = reset_reason;
    boot_ui_auto_recovery_reboot = boot_consume_ui_auto_recovery_reboot();
    bool crash_reset = BootHelpers::isCrashReset(reset_reason);
    StorageManager::BootAction boot_action =
        BootPolicy::apply(crash_reset,
                          boot_count,
                          safe_boot_stage,
                          Config::SAFE_BOOT_MAX_REBOOTS);
    LOGI("Main", "Reset reason: %d (%s), boot count: %u",
         reset_reason,
         resetReasonName(reset_reason),
         boot_count);
    if (boot_ui_auto_recovery_reboot) {
        LOGW("Main", "Previous boot ended with UI auto-recovery reboot");
    }
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
    ctx.mqttManager.begin(ctx.storage, ctx.networkManager, ctx.mqttRuntimeState);

    g_mqtt_manager = &ctx.mqttManager;
    ctx.networkManager.attachMqttContext(
        ctx.mqttManager,
        ctx.mqttManager.userEnabledRef(),
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
    ctx.networkManager.attachChartsRuntime(ctx.chartsRuntimeState);
    ctx.networkManager.attachConnectivityRuntime(ctx.connectivityRuntime);
    ctx.networkManager.attachWebRuntime(ctx.webRuntimeState);
    ctx.networkManager.attachWebUiBridge(ctx.webUiBridge);
    ctx.networkManager.attachCommandQueue(ctx.networkCommandQueue);
    g_wifi_state_ctx.network = &ctx.networkManager;
    g_wifi_state_ctx.time_manager = &ctx.timeManager;
    g_wifi_state_ctx.ui_controller = &ctx.uiController;
    ctx.networkManager.setStateChangeCallback(wifi_state_change_cb, &g_wifi_state_ctx);

    const auto &cfg = ctx.storage.config();
    UiStrings::setLanguage(cfg.language);
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
    ctx.mqttManager.syncWithWifi();
    ctx.mqttRuntimeState.update(ctx.currentData,
                                ctx.night_mode,
                                ctx.alert_blink_enabled,
                                ctx.backlightManager.isOn(),
                                ctx.nightModeManager.isAutoEnabled());
    ctx.chartsRuntimeState.update(ctx.chartsHistory);
    ctx.connectivityRuntime.update(ctx.networkManager, ctx.mqttManager);
}

esp_panel::board::Board *AppInit::initBoardAndPeripherals(Context &ctx) {
    esp_panel::board::Board *board = BoardInit::initBoard();
    if (board == nullptr) {
        LOGE("Main", "Board unavailable, skip display/backlight/touch init");
        return nullptr;
    }
    ctx.backlightManager.attachBacklight(board->getBacklight());
    ctx.timeManager.initRtc();
    ctx.pressureHistory.load(ctx.storage, ctx.currentData);
    ctx.chartsHistory.load(ctx.storage);
    ctx.uiController.apply_auto_night_now();

    BootHelpers::logGt911Address();
    ctx.sensorManager.begin(ctx.storage, ctx.temp_offset, ctx.hum_offset);
    ctx.fanControl.begin(ctx.storage.config().dac_auto_mode,
                         ctx.storage.config().dac_auto_armed);
    String dac_auto_json;
    if (ctx.storage.loadText(StorageManager::kDacAutoPath, dac_auto_json)) {
        DacAutoConfig dac_auto;
        if (DacAutoConfigJson::deserialize(dac_auto_json, dac_auto)) {
            ctx.fanControl.setAutoConfig(dac_auto);
            LOGI("Main", "Loaded DAC auto config");
        } else {
            LOGW("Main", "DAC auto config parse failed, using defaults");
        }
    }
    ctx.webRuntimeState.update(ctx.currentData, ctx.sensorManager.isWarmupActive(), ctx.fanControl);
    ctx.chartsRuntimeState.update(ctx.chartsHistory);

    return board;
}

bool AppInit::initLvglAndUi(Context &ctx, esp_panel::board::Board *board) {
    if (board == nullptr) {
        LOGE("Main", "Skipping LVGL/UI: board init failed");
        ctx.uiController.setLvglReady(false);
        return false;
    }
    LOGI("Main", "Initializing LVGL");
    bool lvgl_ready = lvgl_port_init(board->getLCD(), board->getTouch());
    if (!lvgl_ready) {
        LOGE("Main", "LVGL init failed");
    }

    LOGI("Main", "Creating UI");
    ctx.uiController.setLvglReady(lvgl_ready);
    if (lvgl_ready) {
        ctx.uiController.begin();
        // Keep startup diagnostics, but mute low-level runtime touch/I2C spam.
        esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);
        esp_log_level_set("Panel", ESP_LOG_NONE);
    }
    return lvgl_ready;
}

void AppInit::pollDeferredRuntime() {
    if (!g_deferred_wifi_runtime.pending.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (g_wifi_state_ctx.time_manager) {
        g_wifi_state_ctx.time_manager->updateWifiState(
            g_deferred_wifi_runtime.wifi_enabled.load(std::memory_order_relaxed),
            g_deferred_wifi_runtime.wifi_connected.load(std::memory_order_relaxed));
    }
    if (g_wifi_state_ctx.ui_controller) {
        g_wifi_state_ctx.ui_controller->markDatetimeDirty();
        g_wifi_state_ctx.ui_controller->markWebPagePanelDirty();
    }
}
