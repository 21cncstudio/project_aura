// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include <Arduino.h>

#include "config/AppConfig.h"
#include "config/AppData.h"

#include "core/AppInit.h"
#include "core/BootPolicy.h"
#include "core/Logger.h"
#include "core/MemoryMonitor.h"
#include "core/Watchdog.h"

#include "modules/StorageManager.h"
#include "modules/PressureHistory.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"

#include "core/BootState.h"

#include "ui/UiController.h"
#include "ui/ThemeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"

namespace {

using namespace Config;

SensorData currentData;
StorageManager storage;
PressureHistory pressureHistory;
AuraNetworkManager networkManager;
MqttManager mqttManager;
SensorManager sensorManager;
TimeManager timeManager;
ThemeManager themeManager;
BacklightManager backlightManager;
NightModeManager nightModeManager;
MemoryMonitor memoryMonitor;
uint32_t boot_start_ms = 0;
bool boot_stable = false;
constexpr uint32_t TASK_WDT_TIMEOUT_MS = 180000;

bool night_mode = false;
bool temp_units_c = true;
bool led_indicators_enabled = true;
bool alert_blink_enabled = true;
bool co2_asc_enabled = true;
float temp_offset = 0.0f;
float hum_offset = 0.0f;

UiContext ui_context{
    storage,
    networkManager,
    mqttManager,
    sensorManager,
    timeManager,
    themeManager,
    backlightManager,
    nightModeManager,
    currentData,
    night_mode,
    temp_units_c,
    led_indicators_enabled,
    alert_blink_enabled,
    co2_asc_enabled,
    temp_offset,
    hum_offset
};

UiController uiController(ui_context);

} // namespace

void setup()
{
    delay(3000);
    Serial.begin(115200);
    Logger::begin(Serial, static_cast<Logger::Level>(Config::LOG_LEVEL));

    // Log IPC task stack size to verify CONFIG_IPC_TASK_STACK_SIZE is applied
    #ifdef CONFIG_ESP_IPC_TASK_STACK_SIZE
        LOGI("Main", "IPC task stack size: %d bytes", CONFIG_ESP_IPC_TASK_STACK_SIZE);
        if (CONFIG_ESP_IPC_TASK_STACK_SIZE > 1024) LOGW("Main", "Warning: If using precompiled libs, actual IPC stack might still be 1024!");
    #else
        LOGI("Main", "IPC task stack size: using default (CONFIG_ESP_IPC_TASK_STACK_SIZE not defined)");
    #endif

    memoryMonitor.begin(Config::MEM_LOG_INTERVAL_MS);
    boot_start_ms = millis();

    StorageManager::BootAction boot_action = AppInit::handleBootState();
    AppInit::recoverI2cBus(static_cast<gpio_num_t>(I2C_SDA_PIN),
                           static_cast<gpio_num_t>(I2C_SCL_PIN));

    AppInit::Context init_ctx{
        storage,
        networkManager,
        mqttManager,
        sensorManager,
        timeManager,
        themeManager,
        backlightManager,
        nightModeManager,
        pressureHistory,
        uiController,
        currentData,
        night_mode,
        temp_units_c,
        led_indicators_enabled,
        alert_blink_enabled,
        co2_asc_enabled,
        temp_offset,
        hum_offset
    };

    AppInit::initManagersAndConfig(init_ctx, boot_action);
    auto *board = AppInit::initBoardAndPeripherals(init_ctx);
    AppInit::initLvglAndUi(init_ctx, board);
    memoryMonitor.logNow("boot");

    Watchdog::setup(TASK_WDT_TIMEOUT_MS);
}

void loop()
{
    SensorManager::PollResult sensor_poll =
        sensorManager.poll(currentData, storage, pressureHistory, co2_asc_enabled);
    uiController.onSensorPoll(sensor_poll);
    networkManager.poll();
    uint32_t now = millis();
    BootPolicy::markStable(now,
                           boot_start_ms,
                           Config::SAFE_BOOT_STABLE_MS,
                           boot_stable,
                           boot_count,
                           safe_boot_stage);
    TimeManager::PollResult time_poll = timeManager.poll(now);
    uiController.onTimePoll(time_poll);
    mqttManager.poll(currentData, night_mode, alert_blink_enabled, backlightManager.isOn());
    storage.poll(now);
    memoryMonitor.poll(now);
    uiController.poll(now);
    Watchdog::kick();
    delay(10);
}
