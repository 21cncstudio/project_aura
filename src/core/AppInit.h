// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <driver/gpio.h>

#include "config/AppData.h"
#include "core/ChartsRuntimeState.h"
#include "core/ConnectivityRuntime.h"
#include "core/MqttRuntimeState.h"
#include "core/NetworkCommandQueue.h"
#include "core/WebRuntimeState.h"
#include "modules/StorageManager.h"
#include "modules/DisplayThresholds.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"
#include "modules/PressureHistory.h"
#include "modules/ChartsHistory.h"
#include "modules/FanControl.h"
#include "web/WebUiBridge.h"
#include "ui/ThemeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/UiController.h"

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

namespace AppInit {

struct Context {
    StorageManager &storage;
    AuraNetworkManager &networkManager;
    MqttManager &mqttManager;
    ConnectivityRuntime &connectivityRuntime;
    MqttRuntimeState &mqttRuntimeState;
    ChartsRuntimeState &chartsRuntimeState;
    WebRuntimeState &webRuntimeState;
    WebUiBridge &webUiBridge;
    DisplayThresholdManager &displayThresholds;
    NetworkCommandQueue &networkCommandQueue;
    SensorManager &sensorManager;
    TimeManager &timeManager;
    ThemeManager &themeManager;
    BacklightManager &backlightManager;
    NightModeManager &nightModeManager;
    FanControl &fanControl;
    PressureHistory &pressureHistory;
    ChartsHistory &chartsHistory;
    UiController &uiController;
    SensorData &currentData;
    bool &night_mode;
    bool &temp_units_c;
    bool &led_indicators_enabled;
    bool &alert_blink_enabled;
    bool &co2_asc_enabled;
    float &temp_offset;
    float &hum_offset;
};

StorageManager::BootAction handleBootState();
bool recoverI2cBus(gpio_num_t sda, gpio_num_t scl);
void initManagersAndConfig(Context &ctx, StorageManager::BootAction boot_action);
esp_panel::board::Board *initBoardAndPeripherals(Context &ctx);
bool initLvglAndUi(Context &ctx, esp_panel::board::Board *board);
void pollDeferredRuntime();

} // namespace AppInit
