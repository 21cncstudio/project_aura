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
#include "core/BoardInit.h"
#include "core/BoardRecoveryPolicy.h"
#include "core/BootDiagnostics.h"
#include "core/BootPolicy.h"
#include "core/ChartsRuntimeState.h"
#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "core/MemoryMonitor.h"
#include "core/MqttRuntimeState.h"
#include "core/NetworkCommandQueue.h"
#include "core/NetworkPlane.h"
#include "core/OtaRollback.h"
#include "core/RuntimeReadinessPolicy.h"
#include "core/SafeRestart.h"
#include "core/WebRuntimeState.h"
#include "core/Watchdog.h"

#include "modules/StorageManager.h"
#include "modules/DisplayThresholds.h"
#include "modules/PressureHistory.h"
#include "modules/ChartsHistory.h"
#include "modules/DailyExtremaHistory.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/SdCardManager.h"
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
SdCardManager sdCardManager;
DailyExtremaHistory dailyExtremaHistory;
AuraNetworkManager networkManager;
MqttManager mqttManager;
ConnectivityRuntime connectivityRuntime;
MqttRuntimeState mqttRuntimeState;
ChartsRuntimeState chartsRuntimeState;
WebRuntimeState webRuntimeState;
NetworkCommandQueue networkCommandQueue;
WebUiBridge webUiBridge;
DisplayThresholdManager displayThresholds;
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
constexpr uint32_t OTA_UI_QUIESCE_WARN_MS = 2000;
constexpr uint32_t OTA_TOUCH_BLOCK_MS = 15UL * 60UL * 1000UL;

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
    displayThresholds,
    networkCommandQueue,
    sensorManager,
    chartsHistory,
    dailyExtremaHistory,
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
bool ota_pause_requested = false;
uint32_t ota_pause_requested_ms = 0;
bool ota_pause_wait_warned = false;
bool ota_resume_pending = false;
bool network_plane_running = false;
bool board_ready = false;
bool i2c_runtime_ready = false;
bool lvgl_ready = false;
bool operational_ready = false;

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
    const I2cBusRecovery::LineState early_i2c_state = I2cBusRecovery::sample(
        static_cast<gpio_num_t>(I2C_SDA_PIN),
        static_cast<gpio_num_t>(I2C_SCL_PIN));
    delay(3000);
    Serial.begin(115200);
    Logger::begin(Serial, static_cast<Logger::Level>(Config::LOG_LEVEL));
    Logger::setSerialOutputEnabled(Config::LOG_SERIAL_OUTPUT);
    Logger::setSensorsSerialOutputEnabled(Config::LOG_SERIAL_SENSORS_OUTPUT);
    OtaRollback::logCurrentAppState();

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
    BootDiagnostics::state = BootDiagnostics::Snapshot{};
    BootDiagnostics::state.reset_reason = boot_reset_reason;
    BootDiagnostics::state.auto_recovery_boot = boot_board_auto_recovery_reboot;
    const bool restart_task_ready = safe_restart_init();
    if (!restart_task_ready) {
        LOGW("Restart", "Core0 restart task init failed; automatic recovery will be suppressed");
    }

    BoardInit::Result board_result = BoardInit::initBoard(early_i2c_state);
    auto *board = board_result.board;
    board_ready = board_result.ready();
    i2c_runtime_ready = board_ready;
    BootDiagnostics::state.i2c_status = board_result.last_recovery.status;
    BootDiagnostics::state.sda_high = board_result.last_recovery.after.sda_high;
    BootDiagnostics::state.scl_high = board_result.last_recovery.after.scl_high;
    BootDiagnostics::state.board_ready = board_ready;
    BootDiagnostics::state.board_rounds = board_result.rounds;
    BootDiagnostics::state.board_begin_attempts = board_result.begin_attempts;
    BootDiagnostics::state.board_stage = board_result.last_stage;
    BootDiagnostics::state.board_failure = board_result.failure;

    const BoardRecoveryPolicy::Decision recovery_decision = BoardRecoveryPolicy::decide(
        board_ready,
        boot_reset_reason == ESP_RST_POWERON,
        boot_board_auto_recovery_reboot,
        restart_task_ready);
    if (recovery_decision == BoardRecoveryPolicy::Decision::Restart) {
        LOGE("Main",
             "Board unavailable after %u rounds; automatic recovery restart requested",
             static_cast<unsigned>(board_result.rounds));
        boot_mark_board_auto_recovery_reboot();
        delay(100);
        safe_restart_via_core0();
    }
    if (!board_ready) {
        LOGE("Main",
             "Board automatic recovery suppressed: %s; entering headless mode",
             BoardRecoveryPolicy::decisionText(recovery_decision));
        LOGW("Main", "Runtime I2C polling suppressed: board/I2C unavailable");
    } else if (boot_board_auto_recovery_reboot) {
        LOGI("Main", "Board recovered after automatic restart");
    }

    AppInit::Context init_ctx{
        storage,
        networkManager,
        mqttManager,
        connectivityRuntime,
        mqttRuntimeState,
        chartsRuntimeState,
        webRuntimeState,
        webUiBridge,
        displayThresholds,
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
    AppInit::initBoardAndPeripherals(init_ctx, board);
    sdCardManager.begin(board);
    dailyExtremaHistory.begin(sdCardManager, temp_units_c);
    networkManager.attachDailyHistory(sdCardManager, dailyExtremaHistory);
    lvgl_ready = AppInit::initLvglAndUi(init_ctx, board);
    operational_ready = RuntimeReadinessPolicy::operational(board_ready, lvgl_ready);
    BootDiagnostics::state.lvgl_ready = lvgl_ready;
    memoryMonitor.logNow("boot");

    Watchdog::setup(TASK_WDT_TIMEOUT_MS);
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
        // A controlled restart confirms OTA only after display and UI reached
        // the operational state. Headless management access is intentionally
        // insufficient to validate a display firmware image.
        if (RuntimeReadinessPolicy::canConfirmOta(board_ready, lvgl_ready)) {
            OtaRollback::markValidIfPending("controlled_restart");
        } else {
            LOGW("OTA", "rollback validation withheld: controlled restart from headless boot");
        }
        dailyExtremaHistory.flush();
        WebHandlersBeginRestartShutdown();
        if (lvgl_ready) {
            lvgl_port_prepare_restart();
        }
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
        ota_pause_requested = false;
        ota_pause_requested_ms = 0;
        ota_pause_wait_warned = false;
        ota_resume_pending = false;
        if (lvgl_ready) {
            lvgl_port_block_touch_read(OTA_TOUCH_BLOCK_MS);
        }
    } else if (!ota_busy && ota_window_active) {
        ota_window_active = false;
        if (lvgl_ready &&
            (ota_pause_requested || ota_lvgl_quiesced || lvgl_port_is_paused())) {
            lvgl_port_request_resume();
            ota_resume_pending = true;
        } else if (lvgl_ready) {
            lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
        }
        ota_lvgl_quiesced = false;
        ota_pause_requested = false;
        ota_pause_requested_ms = 0;
        ota_pause_wait_warned = false;
    }

    if (lvgl_ready && !ota_busy && ota_resume_pending) {
        lvgl_port_request_resume();
        if (!lvgl_port_is_paused()) {
            ota_resume_pending = false;
            lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
            LOGI("OTA", "LVGL resumed after OTA window");
        } else {
            AppInit::pollDeferredRuntime();
            memoryMonitor.poll(loop_now);
            Watchdog::kick();
            delay(1);
            return;
        }
    }

    if (ota_busy) {
        if (lvgl_ready && !ota_lvgl_quiesced &&
            static_cast<int32_t>(loop_now - ota_quiesce_due_ms) >= 0) {
            if (!ota_pause_requested) {
                lvgl_port_request_pause();
                ota_pause_requested = true;
                ota_pause_requested_ms = loop_now;
            }

            if (lvgl_port_is_paused()) {
                ota_lvgl_quiesced = true;
                LOGI("OTA", "LVGL paused during OTA transfer");
            } else if (!ota_pause_wait_warned &&
                       static_cast<int32_t>(loop_now - ota_pause_requested_ms) >=
                           static_cast<int32_t>(OTA_UI_QUIESCE_WARN_MS)) {
                ota_pause_wait_warned = true;
                LOGW("OTA", "waiting for LVGL pause acknowledgement (%u ms)",
                     static_cast<unsigned>(loop_now - ota_pause_requested_ms));
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
        if (!ota_pause_requested) {
            uiController.poll(loop_now);
        }
        Watchdog::kick();
        delay(1);
        return;
    }

    SensorManager::PollResult sensor_poll{};
    if (i2c_runtime_ready) {
        sensor_poll = sensorManager.poll(currentData, storage, pressureHistory, co2_asc_enabled);
        uiController.onSensorPoll(sensor_poll);
        chartsHistory.update(currentData, storage, sensorManager.isWarmupActive());
    }
    chartsRuntimeState.update(chartsHistory);
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    if (!network_plane_running) {
        networkCommandQueue.processAll(networkManager, mqttManager, connectivityRuntime);
        networkManager.poll();
        connectivityRuntime.update(networkManager, mqttManager);
    }
    AppInit::pollDeferredRuntime();
    const uint32_t now = millis();
    if (BootPolicy::markStable(now,
                               boot_start_ms,
                               Config::SAFE_BOOT_STABLE_MS,
                               boot_stable,
                               boot_count,
                               safe_boot_stage)) {
        if (RuntimeReadinessPolicy::canConfirmOta(board_ready, lvgl_ready)) {
            OtaRollback::markValidIfPending("stable_boot");
        } else {
            LOGW("OTA", "rollback validation withheld: stable timer reached in headless mode");
        }
    }
    TimeManager::PollResult time_poll = timeManager.poll(now);
    mqttManager.setSystemTimeValid(timeManager.isSystemTimeValid());
    uiController.onTimePoll(time_poll);
    if (sensor_poll.data_changed) {
        dailyExtremaHistory.update(currentData, now);
    }
    dailyExtremaHistory.poll(now);
    if (i2c_runtime_ready) {
        fanControl.poll(now, &currentData, sensorManager.isWarmupActive(), displayThresholds.snapshot());
    }
    const FanControl::Snapshot fan_snapshot = fanControl.snapshot();
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    mqttRuntimeState.update(currentData,
                            fan_snapshot,
                            sensorManager.isWarmupActive(),
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
