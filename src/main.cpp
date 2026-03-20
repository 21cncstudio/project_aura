// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include <Arduino.h>
#include <esp_wifi.h>

#include "config/AppConfig.h"
#include "config/AppData.h"

#include "core/AppInit.h"
#include "core/BootPolicy.h"
#include "core/ChartsRuntimeState.h"
#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "core/MemoryMonitor.h"
#include "core/MqttRuntimeState.h"
#include "core/NetworkCommandQueue.h"
#include "core/NetworkPlane.h"
#include "core/SafeRestart.h"
#include "core/WebRuntimeState.h"
#include "core/Watchdog.h"

#include "modules/StorageManager.h"
#include "modules/PressureHistory.h"
#include "modules/ChartsHistory.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/TimeManager.h"
#include "modules/FanControl.h"
#include "web/WebRuntime.h"
#include "web/WebUiBridge.h"

#include "core/BootState.h"

#include "ui/UiController.h"
#include "ui/ThemeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "lvgl_v8_port.h"

namespace {

using namespace Config;

SensorData currentData;
StorageManager storage;
PressureHistory pressureHistory;
ChartsHistory chartsHistory;
AuraNetworkManager networkManager;
MqttManager mqttManager;
ConnectivityRuntime connectivityRuntime;
MqttRuntimeState mqttRuntimeState;
ChartsRuntimeState chartsRuntimeState;
WebRuntimeState webRuntimeState;
NetworkCommandQueue networkCommandQueue;
WebUiBridge webUiBridge;
SensorManager sensorManager;
TimeManager timeManager;
ThemeManager themeManager;
BacklightManager backlightManager;
NightModeManager nightModeManager;
FanControl fanControl;
MemoryMonitor memoryMonitor;
uint32_t boot_start_ms = 0;
bool boot_stable = false;
constexpr uint32_t TASK_WDT_TIMEOUT_MS = 180000;
constexpr uint32_t OTA_UI_QUIESCE_DELAY_MS = 120;

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
    connectivityRuntime,
    mqttRuntimeState,
    webUiBridge,
    networkCommandQueue,
    sensorManager,
    chartsHistory,
    timeManager,
    themeManager,
    backlightManager,
    nightModeManager,
    fanControl,
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
NetworkPlane::Context network_plane_context{
    networkManager,
    mqttManager,
    connectivityRuntime,
    mqttRuntimeState,
    networkCommandQueue,
    webUiBridge
};
bool ota_window_active = false;
bool ota_lvgl_quiesced = false;
uint32_t ota_quiesce_due_ms = 0;
bool network_plane_running = false;

void quiesce_network_for_restart() {
    const wifi_mode_t wifi_mode = WiFi.getMode();
    if ((wifi_mode & (WIFI_MODE_STA | WIFI_MODE_AP)) == 0) {
        return;
    }

    LOGI("OTA", "quiescing WiFi before restart");
    WiFi.disconnect(true, false);
    delay(250);
}
} // namespace

void setup()
{
    delay(3000);
    Serial.begin(115200);
    Logger::begin(Serial, static_cast<Logger::Level>(Config::LOG_LEVEL));
    Logger::setSerialOutputEnabled(Config::LOG_SERIAL_OUTPUT);
    Logger::setSensorsSerialOutputEnabled(Config::LOG_SERIAL_SENSORS_OUTPUT);

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
        connectivityRuntime,
        mqttRuntimeState,
        chartsRuntimeState,
        webRuntimeState,
        webUiBridge,
        networkCommandQueue,
        sensorManager,
        timeManager,
        themeManager,
        backlightManager,
        nightModeManager,
        fanControl,
        pressureHistory,
        chartsHistory,
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
    if (!safe_restart_init()) {
        LOGW("Restart", "Core0 restart task init failed; controlled restart requests will abort");
    }
    webUiBridge.setDispatchMode(WebUiBridge::DispatchMode::DeferredReply);
    network_plane_running = NetworkPlane::start(network_plane_context);
    if (!network_plane_running) {
        LOGW("Main", "network task unavailable, falling back to main-loop networking");
    }
}

void loop()
{
    if (WebHandlersConsumeRestartRequest()) {
        LOGI("OTA", "restarting now (main loop)");
        WebHandlersBeginRestartShutdown();
        lvgl_port_prepare_restart();
        quiesce_network_for_restart();
        delay(50);
        // Delegate restart to a dedicated Core 0 task so Core 0 is the initiator.
        // This avoids using the small IPC task stack and reduces restart races.
        // This eliminates the RUNSTALL timing race that caused "Cache disabled" panics.
        safe_restart_via_core0();
    }

    const bool ota_busy = WebHandlersIsOtaBusy();
    const uint32_t loop_now = millis();
    if (ota_busy && !ota_window_active) {
        ota_window_active = true;
        ota_lvgl_quiesced = false;
        ota_quiesce_due_ms = loop_now + OTA_UI_QUIESCE_DELAY_MS;
    } else if (!ota_busy && ota_window_active) {
        ota_window_active = false;
        if (ota_lvgl_quiesced) {
            if (lvgl_port_resume()) {
                LOGI("OTA", "LVGL resumed after OTA window");
            } else {
                LOGW("OTA", "failed to resume LVGL after OTA window");
            }
            ota_lvgl_quiesced = false;
        }
    }

    if (ota_busy) {
        if (!ota_lvgl_quiesced && static_cast<int32_t>(loop_now - ota_quiesce_due_ms) >= 0) {
            if (lvgl_port_pause()) {
                ota_lvgl_quiesced = true;
                LOGI("OTA", "LVGL paused during OTA transfer");
            } else {
                LOGW("OTA", "failed to pause LVGL during OTA transfer");
            }
        }
        if (!network_plane_running) {
            networkCommandQueue.processAll(networkManager, mqttManager, connectivityRuntime);
            networkManager.poll();
            connectivityRuntime.update(networkManager, mqttManager);
            mqttManager.poll(mqttRuntimeState);
            connectivityRuntime.update(networkManager, mqttManager);
        }
        AppInit::pollDeferredRuntime();
        memoryMonitor.poll(loop_now);
        if (!ota_lvgl_quiesced) {
            uiController.poll(loop_now);
        }
        Watchdog::kick();
        delay(1);
        return;
    }

    SensorManager::PollResult sensor_poll =
        sensorManager.poll(currentData, storage, pressureHistory, co2_asc_enabled);
    uiController.onSensorPoll(sensor_poll);
    chartsHistory.update(currentData, storage);
    chartsRuntimeState.update(chartsHistory);
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    if (!network_plane_running) {
        networkCommandQueue.processAll(networkManager, mqttManager, connectivityRuntime);
        networkManager.poll();
        connectivityRuntime.update(networkManager, mqttManager);
    }
    AppInit::pollDeferredRuntime();
    const uint32_t now = millis();
    BootPolicy::markStable(now,
                           boot_start_ms,
                           Config::SAFE_BOOT_STABLE_MS,
                           boot_stable,
                           boot_count,
                           safe_boot_stage);
    TimeManager::PollResult time_poll = timeManager.poll(now);
    uiController.onTimePoll(time_poll);
    fanControl.poll(now, &currentData, sensorManager.isWarmupActive());
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    mqttRuntimeState.update(currentData,
                            night_mode,
                            alert_blink_enabled,
                            backlightManager.isOn(),
                            nightModeManager.isAutoEnabled());
    if (!network_plane_running) {
        mqttManager.poll(mqttRuntimeState);
        connectivityRuntime.update(networkManager, mqttManager);
    }
    storage.poll(now);
    memoryMonitor.poll(now);
    uiController.poll(now);
    Watchdog::kick();
    delay(10);
}

