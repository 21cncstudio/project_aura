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
#include "core/AppVersion.h"
#include "core/BoardInit.h"
#include "core/BoardRecoveryPolicy.h"
#include "core/BacklightWakeBreadcrumbs.h"
#include "core/BootDiagnostics.h"
#include "core/BootHelpers.h"
#include "core/BootPolicy.h"
#include "core/ChartsRuntimeState.h"
#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "Gt911Hardware.h"
#include "Gt911Config.h"
#include "core/MemoryMonitor.h"
#include "core/MqttRuntimeState.h"
#include "core/NetworkCommandQueue.h"
#include "core/NetworkPlane.h"
#include "core/OtaRollback.h"
#include "core/RuntimeI2cRecoveryPolicy.h"
#include "core/RuntimeReadinessPolicy.h"
#include "core/SafeRestart.h"
#include "core/SensorI2cBus.h"
#include "core/I2cFaultDomainPolicy.h"
#include "core/SharedI2cShutdownPolicy.h"
#include "core/I2cBusRecovery.h"
#include "core/LastGoodHealthPolicy.h"
#include "core/WebRuntimeState.h"
#include "core/WakePowerGuard.h"
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

// Daily-history restore reaches storage code while other loop frames are
// active. Keep explicit headroom above Arduino's 8 KiB default.
SET_LOOP_TASK_STACK_SIZE(12U * 1024U);

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
constexpr uint32_t RUNTIME_I2C_MONITOR_START_MS = 10UL * 1000UL;
constexpr uint32_t SHARED_I2C_LVGL_QUIESCE_TIMEOUT_MS = 250U;
constexpr uint32_t SHARED_I2C_OWNER_DRAIN_TIMEOUT_MS = 250U;
constexpr uint32_t SHARED_I2C_SENSOR_DRAIN_TIMEOUT_MS = 6000U;
constexpr uint32_t RESTART_I2C_RELEASE_TIMEOUT_MS = 2000U;
constexpr uint32_t RESTART_I2C_STABLE_IDLE_MS = 200U;
constexpr uint32_t RESTART_I2C_RELEASE_POLL_MS = 5U;

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
bool panel_i2c_ready = false;
bool sensor_i2c_ready = false;
bool lvgl_ready = false;
bool operational_ready = false;
bool restart_task_ready = false;
bool restart_request_pending = false;
bool restart_ota_deferral_logged = false;
RuntimeI2cRecoveryPolicy::State runtime_i2c_recovery_policy;
esp_panel::board::Board *runtime_board = nullptr;
bool gt911_runtime_recovery_attempted = false;

void observe_last_good_health(uint32_t now_ms, bool force_transient_pause) {
    const WakePowerGuard::Phase wake_phase = WakePowerGuard::phase(now_ms);
    const bool wake_fail_closed =
        wake_phase == WakePowerGuard::Phase::FailClosed;
    const bool wake_transient_pause =
        wake_phase != WakePowerGuard::Phase::Idle && !wake_fail_closed;

    LastGoodHealthPolicy::Inputs inputs = {};
    inputs.board_ready = board_ready;
    inputs.lvgl_ready = lvgl_ready && uiController.isLvglReady();
    inputs.display_bus_ready = panel_i2c_ready;
    inputs.sensor_bus_ready = sensor_i2c_ready;
    inputs.critical_runtime_fault =
        runtime_i2c_recovery_policy.sharedBusFaultConfirmed() ||
        wake_fail_closed;
    inputs.recovery_or_restart_pending =
        restart_request_pending ||
        boot_ui_auto_recovery_restart_pending() ||
        boot_board_auto_recovery_restart_pending();
    inputs.transient_pause =
        force_transient_pause ||
        wake_transient_pause ||
        WebHandlersIsOtaBusy() ||
        ota_window_active ||
        ota_resume_pending;

    const bool can_sample_ui =
        inputs.board_ready &&
        inputs.lvgl_ready &&
        inputs.display_bus_ready &&
        inputs.sensor_bus_ready &&
        !inputs.critical_runtime_fault &&
        !inputs.recovery_or_restart_pending &&
        !inputs.transient_pause;
    inputs.ui_runtime_healthy =
        can_sample_ui && uiController.isLvglRuntimeHealthy();

    storage.observeLastGoodHealth(
        now_ms,
        LastGoodHealthPolicy::classify(inputs));
}

void quiesce_network_for_restart() {
    const wifi_mode_t wifi_mode = WiFi.getMode();
    if ((wifi_mode & (WIFI_MODE_STA | WIFI_MODE_AP)) == 0) {
        return;
    }

    LOGI("OTA", "quiescing WiFi before restart");
    WiFi.disconnect(true, false);
    delay(250);
}

bool quiesce_lvgl_for_shared_i2c(const char *operation) {
    const bool lvgl_task_active = lvgl_port_request_quiesce();
    if (!lvgl_task_active || lvgl_port_is_paused()) {
        return true;
    }

    const uint32_t pause_started_ms = millis();
    while (!lvgl_port_is_paused() &&
           static_cast<uint32_t>(millis() - pause_started_ms) <
               SHARED_I2C_LVGL_QUIESCE_TIMEOUT_MS) {
        delay(5);
    }
    if (!lvgl_port_is_paused()) {
        Logger::log(Logger::Warn,
                    "I2C",
                    "LVGL cooperative pause timed out before %s",
                    operation != nullptr ? operation : "shared-I2C write");
        return false;
    }
    return true;
}

void disable_runtime_panel_i2c_owners() {
    panel_i2c_ready = false;
    lvgl_port_disable_touch_i2c();
    backlightManager.disableSharedBus();
}

void disable_runtime_sensor_i2c_owners() {
    sensor_i2c_ready = false;
    timeManager.disableSharedI2cRuntime();
    sensorManager.disableSharedI2c();
}

bool drain_runtime_panel_i2c_owners() {
    const bool touch_idle =
        lvgl_port_wait_touch_i2c_idle(SHARED_I2C_OWNER_DRAIN_TIMEOUT_MS);
    const bool backlight_idle =
        backlightManager.waitForSharedBusIdle(SHARED_I2C_OWNER_DRAIN_TIMEOUT_MS);
    if (!touch_idle || !backlight_idle) {
        LOGE("I2C",
             "panel-bus owner drain timed out (touch=%s backlight=%s)",
             touch_idle ? "idle" : "busy",
             backlight_idle ? "idle" : "busy");
        return false;
    }
    return true;
}

bool drain_runtime_sensor_i2c_owners() {
    const bool rtc_idle =
        timeManager.finalizeSharedI2cRuntimeDisable(SHARED_I2C_OWNER_DRAIN_TIMEOUT_MS);
    const bool sensors_idle =
        sensorManager.waitForSharedI2cIdle(SHARED_I2C_SENSOR_DRAIN_TIMEOUT_MS);
    if (!rtc_idle || !sensors_idle) {
        LOGE("I2C",
             "sensor-bus owner drain timed out (rtc=%s sensors=%s)",
             rtc_idle ? "idle" : "busy",
             sensors_idle ? "idle" : "busy");
        return false;
    }
    return true;
}

void disable_runtime_panel_fault_domain() {
    disable_runtime_panel_i2c_owners();
    if (I2cFaultDomainPolicy::panelFailureDisablesSensorDomain(
            Config::SENSOR_I2C_SEPARATE)) {
        disable_runtime_sensor_i2c_owners();
    }
}

bool drain_runtime_panel_fault_domain() {
    const bool panel_idle = drain_runtime_panel_i2c_owners();
    const bool sensor_idle =
        I2cFaultDomainPolicy::panelFailureDisablesSensorDomain(
            Config::SENSOR_I2C_SEPARATE)
            ? drain_runtime_sensor_i2c_owners()
            : true;
    return panel_idle && sensor_idle;
}

void disable_runtime_shared_i2c_owners() {
    // Controlled shutdown closes both physical domains. On the 4.3-inch
    // profile the calls still converge on the one shared controller.
    disable_runtime_panel_i2c_owners();
    disable_runtime_sensor_i2c_owners();
}

bool wait_for_i2c_idle_window(const char *domain,
                              gpio_num_t sda,
                              gpio_num_t scl) {
    const uint32_t started_ms = millis();
    uint32_t idle_started_ms = 0U;
    bool idle_window_active = false;
    I2cBusRecovery::LineState lines = I2cBusRecovery::sample(sda, scl);

    while (static_cast<uint32_t>(millis() - started_ms) <
           RESTART_I2C_RELEASE_TIMEOUT_MS) {
        lines = I2cBusRecovery::sample(sda, scl);
        if (lines.idle()) {
            if (!idle_window_active) {
                idle_window_active = true;
                idle_started_ms = millis();
            }
            if (static_cast<uint32_t>(millis() - idle_started_ms) >=
                RESTART_I2C_STABLE_IDLE_MS) {
                LOGI("Restart",
                     "%s I2C idle before storage teardown (stable=%lu ms wait=%lu ms)",
                     domain,
                     static_cast<unsigned long>(RESTART_I2C_STABLE_IDLE_MS),
                     static_cast<unsigned long>(millis() - started_ms));
                return true;
            }
        } else {
            idle_window_active = false;
        }
        delay(RESTART_I2C_RELEASE_POLL_MS);
    }

    lines = I2cBusRecovery::sample(sda, scl);
    LOGE("Restart",
         "%s I2C has no idle window before storage teardown (SDA=%u SCL=%u wait=%lu ms)",
         domain,
         lines.sda_high ? 1U : 0U,
         lines.scl_high ? 1U : 0U,
         static_cast<unsigned long>(millis() - started_ms));
    return false;
}

bool try_runtime_gt911_recovery() {
    if (runtime_board == nullptr || !lvgl_ready || !uiController.isLvglReady()) {
        LOGE("GT911", "runtime recovery unavailable: board/LVGL not ready");
        return false;
    }

    LOGW("GT911", "touch offline with idle bus; attempting one address recovery");
    if (!quiesce_lvgl_for_shared_i2c("GT911 address recovery")) {
        return false;
    }

    const bool wake_probe_enabled = !backlightManager.isOn();
    if (!lvgl_port_prepare_touch_hard_recovery()) {
        LOGE("GT911", "runtime recovery could not isolate touch/IRQ state");
        (void)lvgl_port_request_resume();
        return false;
    }

    const bool address_selected = Gt911Hardware::selectConfiguredAddress(runtime_board);
    uint8_t product_id[3] = {};
    const bool product_read =
        address_selected && BootHelpers::readGt911ConfiguredProductId(product_id);
    const bool product_valid =
        product_read && BootHelpers::isExpectedGt911ProductId(product_id);
    const bool touch_restored = lvgl_port_complete_touch_hard_recovery(
        product_valid, wake_probe_enabled);
    (void)lvgl_port_request_resume();

    if (!product_valid || !touch_restored) {
        if (product_read) {
            LOGE("GT911",
                 "runtime recovery verification failed at 0x%02X: %02X,%02X,%02X",
                 AURA_GT911_I2C_ADDRESS,
                 product_id[0],
                 product_id[1],
                 product_id[2]);
        } else {
            LOGE("GT911",
                 "runtime recovery verification found no response at 0x%02X",
                 AURA_GT911_I2C_ADDRESS);
        }
        return false;
    }

    LOGI("GT911",
         "runtime address recovery verified at 0x%02X: %02X,%02X,%02X",
         AURA_GT911_I2C_ADDRESS,
         product_id[0],
         product_id[1],
         product_id[2]);
    return true;
}

void poll_runtime_i2c_recovery(uint32_t now_ms) {
    if (!panel_i2c_ready ||
        static_cast<uint32_t>(now_ms - boot_start_ms) < RUNTIME_I2C_MONITOR_START_MS) {
        return;
    }

    lvgl_port_diagnostics_t diagnostics = {};
    const bool diagnostics_available =
        lvgl_ready && uiController.isLvglReady() &&
        lvgl_port_get_diagnostics(&diagnostics);
    const bool touch_offline =
        diagnostics_available && diagnostics.touch_offline;
    const I2cBusRecovery::LineState lines = I2cBusRecovery::sample(
        static_cast<gpio_num_t>(I2C_SDA_PIN),
        static_cast<gpio_num_t>(I2C_SCL_PIN));

    if (touch_offline && lines.idle() && !gt911_runtime_recovery_attempted) {
        gt911_runtime_recovery_attempted = true;
        if (Config::PANEL_RUNTIME_GT911_HARD_RECOVERY_ENABLED) {
            if (try_runtime_gt911_recovery()) {
                return;
            }
            LOGE("GT911",
                 "runtime address recovery failed; evaluating restart policy");
        } else {
            LOGW("GT911",
                 "touch offline with idle panel bus; automatic CH422G hard recovery disabled");
        }
    }

    const bool actionable_touch_offline =
        Config::PANEL_RUNTIME_TOUCH_AUTO_RESTART_ENABLED && touch_offline;

    const RuntimeI2cRecoveryPolicy::Decision decision =
        runtime_i2c_recovery_policy.poll(now_ms,
                                         Config::PANEL_RUNTIME_STUCK_LINE_CONFIRMATION_QUALIFIED,
                                         lines.idle(),
                                         actionable_touch_offline,
                                         boot_any_auto_recovery_boot(),
                                         restart_task_ready);
    if (runtime_i2c_recovery_policy.sharedBusFaultConfirmed() &&
        panel_i2c_ready &&
        Config::PANEL_RUNTIME_STUCK_BUS_AUTO_RECOVERY_ENABLED) {
        disable_runtime_panel_fault_domain();

        // Quiesce both restart and suppressed paths immediately. A controlled
        // restart can be deferred by an active OTA upload, so leaving LVGL
        // running here would otherwise reopen touch and direct UI I2C paths.
        const bool lvgl_quiesced =
            quiesce_lvgl_for_shared_i2c("runtime shared-I2C shutdown");
        const bool owners_drained = drain_runtime_panel_fault_domain();

        // A suppressed recovery has no later restart shutdown path. Make one
        // bounded best-effort attempt to clear retained DAC output, but only
        // after every gated runtime shared-I2C owner has drained. CH422G SD
        // card-select writes are startup/teardown-only and cannot run here.
        if (decision != RuntimeI2cRecoveryPolicy::Decision::Restart &&
            !Config::SENSOR_I2C_SEPARATE) {
            const SharedI2cShutdownPolicy::SafeOutputDecision safe_output_decision =
                SharedI2cShutdownPolicy::decideSafeOutput(lvgl_quiesced,
                                                          owners_drained);
            if (!SharedI2cShutdownPolicy::shouldAttemptSafeOutput(
                    safe_output_decision)) {
                LOGE("I2C", "DAC safe-output write skipped: shared-I2C owners still active");
            } else {
                if (SharedI2cShutdownPolicy::shouldWarnUnconfirmedLvglPause(
                        safe_output_decision)) {
                    LOGW("I2C",
                         "LVGL pause not acknowledged; shared-I2C owners are drained");
                }
                if (!fanControl.prepareForI2cOffline()) {
                    LOGW("I2C", "DAC safe-output write failed before disabling shared-bus polling");
                }
            }
        }
        LOGE("I2C",
             "shared bus lines remain stuck (SDA=%u, SCL=%u); disabling runtime I2C access",
             lines.sda_high ? 1u : 0u,
             lines.scl_high ? 1u : 0u);
    }
    if (decision == RuntimeI2cRecoveryPolicy::Decision::None) {
        return;
    }
    const bool automatic_recovery_enabled =
        actionable_touch_offline ||
        (runtime_i2c_recovery_policy.sharedBusFaultConfirmed() &&
         Config::PANEL_RUNTIME_STUCK_BUS_AUTO_RECOVERY_ENABLED);
    if (!automatic_recovery_enabled) {
        LOGE("I2C",
             "runtime panel-bus fault observed; automatic shutdown/restart disabled (SDA=%u SCL=%u)",
             lines.sda_high ? 1u : 0u,
             lines.scl_high ? 1u : 0u);
        return;
    }
    if (decision != RuntimeI2cRecoveryPolicy::Decision::Restart) {
        LOGE("I2C",
             "runtime recovery suppressed: %s",
             RuntimeI2cRecoveryPolicy::decisionText(decision));
        return;
    }

    LOGE("I2C",
         "runtime I2C fault detected (touch_offline=%s, shared_bus_stuck=%s, SDA=%u, SCL=%u); scheduling one controlled restart",
         actionable_touch_offline ? "yes" : "no",
         runtime_i2c_recovery_policy.sharedBusFaultConfirmed() ? "yes" : "no",
         lines.sda_high ? 1u : 0u,
         lines.scl_high ? 1u : 0u);
    boot_mark_board_auto_recovery_reboot();
    WebHandlersRequestRestart();
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
    LOGI("Main", "hardware_profile=%s hardware_target=%s build_id=%s console=%s",
         AppVersion::hardwareProfile(), AppVersion::hardwareTarget(),
         AppVersion::buildId(), ARDUINO_USB_CDC_ON_BOOT ? "native_usb_cdc" : "uart");
    OtaRollback::logCurrentAppState();
    LOGI("Main", "Arduino loop task stack size: %u bytes",
         static_cast<unsigned>(getArduinoLoopTaskStackSize()));

    // Log IPC task stack size to verify CONFIG_IPC_TASK_STACK_SIZE is applied
    #ifdef CONFIG_ESP_IPC_TASK_STACK_SIZE
        LOGI("Main", "IPC task stack size: %d bytes", CONFIG_ESP_IPC_TASK_STACK_SIZE);
        if (CONFIG_ESP_IPC_TASK_STACK_SIZE > 1024) LOGW("Main", "Warning: If using precompiled libs, actual IPC stack might still be 1024!");
    #else
        LOGI("Main", "IPC task stack size: using default (CONFIG_ESP_IPC_TASK_STACK_SIZE not defined)");
    #endif

    memoryMonitor.begin(Config::MEM_LOG_INTERVAL_MS);
    boot_start_ms = millis();
    Watchdog::setup(TASK_WDT_TIMEOUT_MS);

    StorageManager::BootAction boot_action = AppInit::handleBootState();
    const bool auto_recovery_boot = boot_any_auto_recovery_boot();
    BacklightWakeBreadcrumbs::initializeAtBoot(
        boot_board_cold_start,
        boot_reset_reason == ESP_RST_BROWNOUT);
    BootDiagnostics::state = BootDiagnostics::Snapshot{};
    BootDiagnostics::state.reset_reason = boot_reset_reason;
    BootDiagnostics::state.auto_recovery_boot = auto_recovery_boot;
    BootDiagnostics::state.previous_backlight_trace =
        BacklightWakeBreadcrumbs::bootSnapshot();
    const BacklightWakeBreadcrumbs::BootSnapshot &previous_backlight_trace =
        BootDiagnostics::state.previous_backlight_trace;
    if (previous_backlight_trace.has_trace) {
        const BacklightWakeBreadcrumbs::Trace &trace = previous_backlight_trace.trace;
        const bool incomplete =
            previous_backlight_trace.status == BacklightWakeBreadcrumbs::CaptureStatus::Active;
        if (incomplete) {
            LOGW("BacklightTrace",
                 "previous boot trace incomplete: event=%s stage=%s seq=%lu uptime=%lu ms epoch=%lu prequiet=%lu ms wait_exceeded=%s wait_active=%lu command=%s driver=%s duration=%lu us retention_uncertain=%s target=%s previous=%s lines(before=%s/%u%u after_driver=%s/%u%u after_probe=%s/%u%u)",
                 BacklightWakeBreadcrumbs::eventText(trace.event),
                 BacklightWakeBreadcrumbs::stageText(trace.stage),
                 static_cast<unsigned long>(trace.sequence),
                 static_cast<unsigned long>(trace.uptime_ms),
                 static_cast<unsigned long>(trace.epoch_s),
                 static_cast<unsigned long>(trace.pre_quiet_elapsed_ms),
                 trace.pre_quiet_wait_exceeded ? "yes" : "no",
                 static_cast<unsigned long>(
                     trace.pre_quiet_wait_exceeded_active_operations),
                 BacklightWakeBreadcrumbs::commandResultText(trace.command_result),
                 BacklightWakeBreadcrumbs::driverResultText(trace.driver_result),
                 static_cast<unsigned long>(trace.driver_duration_us),
                 previous_backlight_trace.retention_uncertain ? "yes" : "no",
                 trace.target_on ? "on" : "off",
                 trace.previous_on ? "on" : "off",
                 trace.before.valid ? "valid" : "invalid",
                 trace.before.sda_high ? 1u : 0u,
                 trace.before.scl_high ? 1u : 0u,
                 trace.after_driver.valid ? "valid" : "invalid",
                 trace.after_driver.sda_high ? 1u : 0u,
                 trace.after_driver.scl_high ? 1u : 0u,
                 trace.after_wake_probe.valid ? "valid" : "invalid",
                 trace.after_wake_probe.sda_high ? 1u : 0u,
                 trace.after_wake_probe.scl_high ? 1u : 0u);
        } else {
            const Logger::Level trace_level =
                previous_backlight_trace.status !=
                        BacklightWakeBreadcrumbs::CaptureStatus::Completed ||
                    previous_backlight_trace.retention_uncertain
                    ? Logger::Warn
                    : Logger::Info;
            Logger::log(
                trace_level,
                "BacklightTrace",
                "previous boot trace: status=%s event=%s stage=%s seq=%lu prequiet=%lu ms wait_exceeded=%s wait_active=%lu command=%s driver=%s duration=%lu us retention_uncertain=%s",
                BacklightWakeBreadcrumbs::statusText(
                    previous_backlight_trace.status),
                BacklightWakeBreadcrumbs::eventText(trace.event),
                BacklightWakeBreadcrumbs::stageText(trace.stage),
                static_cast<unsigned long>(trace.sequence),
                static_cast<unsigned long>(trace.pre_quiet_elapsed_ms),
                trace.pre_quiet_wait_exceeded ? "yes" : "no",
                static_cast<unsigned long>(
                    trace.pre_quiet_wait_exceeded_active_operations),
                BacklightWakeBreadcrumbs::commandResultText(trace.command_result),
                BacklightWakeBreadcrumbs::driverResultText(trace.driver_result),
                static_cast<unsigned long>(trace.driver_duration_us),
                previous_backlight_trace.retention_uncertain ? "yes" : "no");
        }
    } else if (previous_backlight_trace.status ==
               BacklightWakeBreadcrumbs::CaptureStatus::Corrupt) {
        LOGW("BacklightTrace", "previous boot trace corrupt; ignoring retained data");
    }
    restart_task_ready = safe_restart_init();
    if (!restart_task_ready) {
        LOGW("Restart", "Core0 restart task init failed; automatic recovery will be suppressed");
    }

    const I2cBusRecovery::LineState pre_init_i2c_state = I2cBusRecovery::sample(
        static_cast<gpio_num_t>(I2C_SDA_PIN),
        static_cast<gpio_num_t>(I2C_SCL_PIN));
    BoardInit::Result board_result = BoardInit::initBoard(early_i2c_state,
                                                          pre_init_i2c_state,
                                                          auto_recovery_boot);
    auto *board = board_result.board;
    runtime_board = board;
    board_ready = board_result.ready();
    panel_i2c_ready = board_ready;
    const SensorI2cBus::Result sensor_bus_result = SensorI2cBus::begin();
    sensor_i2c_ready = I2cFaultDomainPolicy::sensorRuntimeReady(
        Config::SENSOR_I2C_SEPARATE,
        panel_i2c_ready,
        sensor_bus_result.ready());
    BootDiagnostics::state.i2c_status = board_result.last_recovery.status;
    BootDiagnostics::state.sda_high = board_result.last_recovery.after.sda_high;
    BootDiagnostics::state.scl_high = board_result.last_recovery.after.scl_high;
    BootDiagnostics::state.board_ready = board_ready;
    BootDiagnostics::state.board_rounds = board_result.rounds;
    BootDiagnostics::state.board_begin_attempts = board_result.begin_attempts;
    BootDiagnostics::state.cold_power_start = board_result.cold_power_start;
    BootDiagnostics::state.cold_power_wait_ms = board_result.cold_power_wait_ms;
    BootDiagnostics::state.expander_probe_status = board_result.expander_probe.status;
    BootDiagnostics::state.expander_probe_attempts = board_result.expander_probe.attempts;
    BootDiagnostics::state.expander_probe_wait_ms = board_result.expander_probe.waited_ms;
    BootDiagnostics::state.expander_probe_error = board_result.expander_probe.last_error;
    BootDiagnostics::state.expander_probe_phase = board_result.expander_probe.phase;
    BootDiagnostics::state.expander_probe_failed_address = board_result.expander_probe.failed_address;
    BootDiagnostics::state.expander_probe_failed_value = board_result.expander_probe.failed_value;
    BootDiagnostics::state.expander_probe_bus_recoveries = board_result.expander_probe.bus_recoveries;
    BootDiagnostics::state.expander_probe_failure_lines_valid = board_result.expander_probe.failure_lines_valid;
    BootDiagnostics::state.expander_probe_failure_sda_high = board_result.expander_probe.failure_sda_high;
    BootDiagnostics::state.expander_probe_failure_scl_high = board_result.expander_probe.failure_scl_high;
    BootDiagnostics::state.expander_probe_recovery_sda_high = board_result.expander_probe.recovery_sda_high;
    BootDiagnostics::state.expander_probe_recovery_scl_high = board_result.expander_probe.recovery_scl_high;
    BootDiagnostics::state.expander_probe_recovery_pulses = board_result.expander_probe.recovery_pulses;
    BootDiagnostics::state.board_stage = board_result.last_stage;
    BootDiagnostics::state.board_failure = board_result.failure;

    const bool board_recovery_eligible =
        boot_board_cold_start ||
        board_result.failure == BoardInit::Failure::Begin ||
        board_result.failure == BoardInit::Failure::Timeout;
    if (!board_ready) {
        LOGE("Main",
             "Board unavailable after %u rounds; continuing startup in headless mode",
             static_cast<unsigned>(board_result.rounds));
        LOGW("Main", "Panel I2C runtime suppressed: board unavailable");
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
    AppInit::initBoardAndPeripherals(init_ctx, board, sensor_i2c_ready);
    if (!panel_i2c_ready) {
        // On 7-inch hardware, a failed panel must not take the independent
        // RTC/sensor/DAC domain offline. The 4.3-inch profile still closes all
        // owners because they share the panel controller.
        disable_runtime_panel_fault_domain();
        (void)drain_runtime_panel_fault_domain();
    }
    sdCardManager.begin(board);
    dailyExtremaHistory.begin(sdCardManager, temp_units_c);
    networkManager.attachDailyHistory(sdCardManager, dailyExtremaHistory);
    const AppInit::LvglInitResult lvgl_result = AppInit::initLvglAndUi(init_ctx, board);
    lvgl_ready = lvgl_result.ui_ready;
    operational_ready = RuntimeReadinessPolicy::operational(board_ready, lvgl_ready);
    BootDiagnostics::state.lvgl_ready = lvgl_ready;

    const BoardRecoveryPolicy::Decision recovery_decision = BoardRecoveryPolicy::decide(
        board_ready,
        lvgl_ready,
        board_recovery_eligible,
        auto_recovery_boot,
        restart_task_ready,
        Config::PANEL_STARTUP_AUTO_RESTART_ENABLED);
    if (recovery_decision == BoardRecoveryPolicy::Decision::Restart) {
        if (!board_ready) {
            LOGE("Main", "Board startup failed; controlled recovery restart will be scheduled");
            boot_mark_board_auto_recovery_reboot();
        } else {
            LOGE("Main", "LVGL/UI startup failed; controlled recovery restart will be scheduled");
            boot_mark_ui_auto_recovery_reboot();
        }
    } else if (!operational_ready) {
        LOGE("Main",
             "Startup automatic recovery suppressed: %s; continuing in headless mode",
             BoardRecoveryPolicy::decisionText(recovery_decision));
    } else if (boot_ui_auto_recovery_reboot) {
        LOGI("Main", "LVGL/UI recovered after automatic restart");
    }
    if (!lvgl_ready) {
        // lvgl_port_init() may have completed before UI startup failed. Make
        // every headless path request a cooperative pause. Do not force-stop a
        // task before the controlled shutdown has made the shared-I2C DAC safe.
        (void)lvgl_port_request_quiesce();
    }
    memoryMonitor.logNow("boot");

    webUiBridge.setDispatchMode(WebUiBridge::DispatchMode::DeferredReply);
    network_plane_running = NetworkPlane::start(network_plane_context);
    if (!network_plane_running) {
        LOGW("Main", "network task unavailable, falling back to main-loop networking");
    }
    if (recovery_decision == BoardRecoveryPolicy::Decision::Restart) {
        LOGW("Main",
             "Scheduling one controlled startup recovery restart in %lu ms",
             static_cast<unsigned long>(Config::BOARD_RECOVERY_RESTART_DELAY_MS));
        WebHandlersRequestRestart(Config::BOARD_RECOVERY_RESTART_DELAY_MS);
    } else {
        BacklightWakeBreadcrumbs::acknowledgeBootSnapshot();
    }
}

void loop()
{
    if (WebHandlersConsumeRestartRequest()) {
        restart_request_pending = true;
    }
    const uint32_t health_entry_now = millis();
    observe_last_good_health(health_entry_now, false);
    if (restart_request_pending) {
        // Upload admission and restart shutdown use one atomic gate. Whichever
        // side wins first excludes the other without a cross-task check/use gap.
        if (!WebHandlersTryBeginRestartShutdown()) {
            if (!restart_ota_deferral_logged) {
                LOGW("Restart", "restart deferred until OTA upload completes");
                restart_ota_deferral_logged = true;
            }
        } else {
            restart_request_pending = false;
            restart_ota_deferral_logged = false;
            LOGI("OTA", "restarting now (main loop)");
            // A controlled restart confirms OTA only after the display, UI and
            // required sensor host reached the operational state. Headless
            // management access is intentionally insufficient to validate a
            // hardware-profile firmware image.
            const bool auto_recovery_restart =
                boot_ui_auto_recovery_restart_pending() ||
                boot_board_auto_recovery_restart_pending();
            const bool ui_runtime_healthy = uiController.isLvglRuntimeHealthy();
            if (auto_recovery_restart) {
                LOGW("OTA", "rollback validation withheld: automatic recovery restart");
            } else if (RuntimeReadinessPolicy::canConfirmOta(
                           board_ready,
                           lvgl_ready,
                           sensor_i2c_ready,
                           ui_runtime_healthy)) {
                OtaRollback::markValidIfPending("controlled_restart");
            } else {
                LOGW("OTA", "rollback validation withheld: required runtime not operational");
            }
            // Stop LVGL between handler iterations so touch cannot overlap
            // the bounded DAC write on the shared I2C driver.
            disable_runtime_shared_i2c_owners();
            const bool lvgl_quiesced =
                quiesce_lvgl_for_shared_i2c("DAC safe write for restart");
            const bool panel_owners_drained = drain_runtime_panel_i2c_owners();
            const bool sensor_owners_drained = drain_runtime_sensor_i2c_owners();
            // On 7-inch hardware, panel traffic cannot overlap the independent
            // sensor host. The shared 4.3-inch profile still requires every
            // panel and sensor owner to be idle before another transaction.
            const bool sensor_bus_exclusive =
                I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
                    Config::SENSOR_I2C_SEPARATE,
                    panel_owners_drained,
                    sensor_owners_drained);
            if (sensor_bus_exclusive) {
                if (!sensorManager.stopHchoForRestart()) {
                    LOGW("Restart", "active HCHO measurement did not stop cleanly");
                }
            } else {
                LOGW("Restart", "active HCHO stop skipped: sensor-bus owners still active");
            }
            const bool safe_output_lvgl_quiesced =
                I2cFaultDomainPolicy::lvglPauseSatisfiedForSensorOutput(
                    Config::SENSOR_I2C_SEPARATE,
                    lvgl_quiesced);
            const SharedI2cShutdownPolicy::SafeOutputDecision safe_output_decision =
                SharedI2cShutdownPolicy::decideSafeOutput(
                    safe_output_lvgl_quiesced,
                    sensor_bus_exclusive);
            if (!SharedI2cShutdownPolicy::shouldAttemptSafeOutput(
                    safe_output_decision)) {
                LOGE("Restart", "DAC safe output skipped: sensor bus is not exclusive");
            } else {
                if (SharedI2cShutdownPolicy::shouldWarnUnconfirmedLvglPause(
                        safe_output_decision)) {
                    LOGW("Restart",
                         "LVGL pause not acknowledged; shared-I2C owners are drained");
                }
                if (!fanControl.prepareForRestart()) {
                    LOGW("Restart", "DAC safe output could not be confirmed before restart");
                }
            }
            if (Config::SENSOR_I2C_SEPARATE) {
                (void)wait_for_i2c_idle_window(
                    "sensor",
                    static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
                    static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
            }
            (void)wait_for_i2c_idle_window(
                "panel",
                static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
                static_cast<gpio_num_t>(Config::I2C_SCL_PIN));
            // This remains safe after a failed or only partially completed
            // lvgl_port_init(). Forced suspension is last, after the bounded
            // DAC transaction, so it cannot strand the I2C driver mutex.
            lvgl_port_prepare_restart();
            quiesce_network_for_restart();
            if (!pressureHistory.flush(storage)) {
                LOGW("Restart", "pressure history flush failed; previous snapshot preserved");
            }
            dailyExtremaHistory.flush();
            if (!sdCardManager.end()) {
                LOGW("Restart", "SD card did not unmount cleanly before restart");
            }
            delay(50);
            // Delegate restart to a dedicated Core 0 task so Core 0 is the initiator.
            // This avoids using the small IPC task stack and reduces restart races.
            // This eliminates the RUNSTALL timing race that caused "Cache disabled" panics.
            safe_restart_via_core0();
        }
    }

    const bool ota_busy = WebHandlersIsOtaBusy();
    // Health may be false precisely because the port is paused, but a
    // permanently disabled shared I2C bus must never be followed by an OTA
    // resume that restarts touch traffic.
    const bool lvgl_runtime_available =
        RuntimeReadinessPolicy::canManageLvglRuntime(
            panel_i2c_ready, lvgl_ready, uiController.isLvglReady());
    const uint32_t loop_now = millis();
    if (ota_busy && !ota_window_active) {
        ota_window_active = true;
        ota_lvgl_quiesced = false;
        ota_quiesce_due_ms = loop_now + OTA_UI_QUIESCE_DELAY_MS;
        ota_pause_requested = false;
        ota_pause_requested_ms = 0;
        ota_pause_wait_warned = false;
        ota_resume_pending = false;
        if (lvgl_runtime_available) {
            lvgl_port_block_touch_read(OTA_TOUCH_BLOCK_MS);
        }
    } else if (!ota_busy && ota_window_active) {
        ota_window_active = false;
        if (lvgl_runtime_available &&
            (ota_pause_requested || ota_lvgl_quiesced || lvgl_port_is_paused())) {
            // Publish the release gate before the display task can resume.
            // Otherwise one LVGL handler may run between resume and the block.
            lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
            lvgl_port_request_resume();
            ota_resume_pending = true;
        } else if (lvgl_runtime_available) {
            lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
        }
        ota_lvgl_quiesced = false;
        ota_pause_requested = false;
        ota_pause_requested_ms = 0;
        ota_pause_wait_warned = false;
    }

    if (lvgl_runtime_available && !ota_busy && ota_resume_pending) {
        lvgl_port_request_resume();
        if (!lvgl_port_is_paused()) {
            ota_resume_pending = false;
            lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
            LOGI("OTA", "LVGL resumed after OTA window");
        } else {
            observe_last_good_health(loop_now, true);
            AppInit::pollDeferredRuntime();
            memoryMonitor.poll(loop_now);
            Watchdog::kick();
            delay(1);
            return;
        }
    }

    if (ota_busy) {
        if (lvgl_runtime_available && !ota_lvgl_quiesced &&
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
        observe_last_good_health(loop_now, true);
        Watchdog::kick();
        delay(1);
        return;
    }

    const uint32_t background_now = millis();
    const bool wake_background_paused =
        WakePowerGuard::backgroundPaused(background_now);
    if (wake_background_paused) {
        // During PreQuiet/Settle only the UI owner may advance the guarded
        // backlight transition. RenderWait deliberately lets that same owner
        // perform the first post-wake render while every background producer
        // remains closed. Do not update snapshots, storage, OTA state or
        // diagnostics here: even short owner-side work would otherwise count
        // against a quiet interval that has already started.
        uiController.poll(background_now);
        observe_last_good_health(background_now, true);
        Watchdog::kick();
        delay(1);
        return;
    }
    if (!wake_background_paused) {
        poll_runtime_i2c_recovery(background_now);
    }

    bool system_time_trusted = timeManager.isSystemTimeTrusted();
    SensorManager::PollResult sensor_poll{};
    if (!wake_background_paused && sensor_i2c_ready) {
        sensor_poll = sensorManager.poll(currentData,
                                         storage,
                                         pressureHistory,
                                         co2_asc_enabled,
                                         system_time_trusted);
        uiController.onSensorPoll(sensor_poll);
        chartsHistory.update(currentData,
                             storage,
                             sensorManager.isWarmupActive(),
                             system_time_trusted);
    }
    chartsRuntimeState.update(chartsHistory);
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    if (!wake_background_paused && !network_plane_running) {
        networkCommandQueue.processAll(networkManager, mqttManager, connectivityRuntime);
        networkManager.poll();
        connectivityRuntime.update(networkManager, mqttManager);
    }
    if (!wake_background_paused) {
        AppInit::pollDeferredRuntime();
    }
    const uint32_t now = millis();
    if (BootPolicy::markStable(now,
                               boot_start_ms,
                               Config::SAFE_BOOT_STABLE_MS,
                               boot_stable,
                               boot_count,
                               safe_boot_stage)) {
        if (RuntimeReadinessPolicy::canConfirmOta(
                board_ready,
                lvgl_ready,
                sensor_i2c_ready,
                uiController.isLvglRuntimeHealthy())) {
            OtaRollback::markValidIfPending("stable_boot");
        } else {
            LOGW("OTA", "rollback validation withheld: stable timer reached without healthy required runtime");
        }
    }
    if (!wake_background_paused) {
        TimeManager::PollResult time_poll = timeManager.poll(now, sensor_i2c_ready);
        mqttManager.setSystemTimeValid(timeManager.isSystemTimeValid());
        uiController.onTimePoll(time_poll);
    }
    // RTC/NTP/manual reconciliation may establish trust during poll(). Daily
    // history only updates on data changes, so do not defer this transition to
    // a future sensor sample.
    system_time_trusted = timeManager.isSystemTimeTrusted();
    if (sensor_poll.data_changed) {
        dailyExtremaHistory.update(currentData, now, system_time_trusted);
    }
    if (!wake_background_paused) {
        dailyExtremaHistory.poll(now);
    }
    if (!wake_background_paused && sensor_i2c_ready) {
        fanControl.poll(now, &currentData, sensorManager.isWarmupActive(), displayThresholds.snapshot());
    }
    const FanControl::Snapshot fan_snapshot = fanControl.snapshot();
    const BacklightManager::RuntimeSnapshot backlight_snapshot =
        backlightManager.runtimeSnapshot();
    webRuntimeState.update(currentData, sensorManager.isWarmupActive(), fanControl);
    mqttRuntimeState.update(currentData,
                            fan_snapshot,
                            sensorManager.isWarmupActive(),
                            night_mode,
                            alert_blink_enabled,
                            backlight_snapshot.actual_on,
                            backlight_snapshot.transition_pending,
                            backlight_snapshot.target_on,
                            nightModeManager.isAutoEnabled());
    if (!wake_background_paused && !network_plane_running) {
        mqttManager.poll(mqttRuntimeState);
        connectivityRuntime.update(networkManager, mqttManager);
    }
    if (!wake_background_paused) {
        observe_last_good_health(
            now,
            false);
        storage.poll(now);
    }
    memoryMonitor.poll(now);
    uiController.poll(now);
    Watchdog::kick();
    delay(10);
}
