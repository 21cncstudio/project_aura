// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"
#include "ui/BootDiagPolicy.h"
#include "ui/UiLocalization.h"
#include "ui/UiRenderLoop.h"
#include "ui/UiScreenFlow.h"
#include "ui/UiText.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <esp_wifi.h>

#include "lvgl_v8_port.h"
#include "ui/ui.h"
#include "ui/styles.h"
#include "ui/images.h"
#include "ui/StatusMessages.h"
#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/AppVersion.h"
#include "core/AirQualityEngine.h"
#include "core/Logger.h"
#include "core/SafeRestart.h"
#include "web/WebRuntime.h"
#include "core/SystemLogFilter.h"
#include "modules/StorageManager.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "modules/SensorManager.h"
#include "modules/DailyExtremaHistory.h"
#include "modules/FanControl.h"
#include "modules/TimeManager.h"
#include "ui/ThemeManager.h"
#include "ui/BacklightManager.h"
#include "ui/NightModeManager.h"
#include "ui/UiEventBinder.h"
#include "ui/UiAutoRecoveryPolicy.h"
#include "ui/UiOptionalGasProfile.h"

using namespace Config;

namespace {

constexpr uint32_t STATUS_ROTATE_MS = 5000;
constexpr int UI_LVGL_LOCK_TIMEOUT_MS = 500;
constexpr int UI_LVGL_STARTUP_LOCK_TIMEOUT_MS = 2000;
constexpr uint32_t UI_LVGL_LOCK_WARN_MS = 60000;
constexpr uint16_t UI_LVGL_LOCK_WARN_FAIL_STREAK = 3;
constexpr uint32_t UI_LVGL_DIAG_HEARTBEAT_MS = 60000;
constexpr uint32_t UI_LVGL_DIAG_STALL_MS = 15000;
constexpr uint32_t UI_LVGL_DIAG_VSYNC_STALL_MS = 5000;
constexpr uint32_t UI_LVGL_DIAG_FLUSH_STALL_MS = 15000;
constexpr uint32_t UI_LVGL_DIAG_STALL_LOG_COOLDOWN_MS = 30000;
constexpr uint32_t UI_LVGL_DIAG_RECOVER_MS = 3000;
constexpr uint32_t UI_LVGL_DIAG_REBOOT_STALL_MS = 45000;
constexpr uint32_t UI_LVGL_DIAG_AGE_UNKNOWN_MS = 0xFFFFFFFFu;
constexpr uint32_t UI_LVGL_DIAG_TOUCH_WARN_DELTA = 3;
constexpr uint32_t UI_POOR_GAS_BG_HEX = 0xEB0000;
constexpr uint32_t UI_HIGH_CO2_BG_HEX = 0xB36B00;
constexpr float UI_POOR_GAS_BG_HYSTERESIS_RATIO = 0.05f;
constexpr size_t UI_DIAG_LOG_MAX_LINES = 17;
constexpr size_t UI_DIAG_LOG_RECENT_MAX = 32;
constexpr size_t UI_DIAG_LOG_TEXT_CAPACITY = 2048;
constexpr size_t UI_DIAG_LOG_LINE_CAPACITY = 128;
constexpr size_t UI_DIAG_LOG_MESSAGE_MAX_CHARS = 54;

String trim_copy(const String &value) {
    const char *begin = value.c_str();
    if (!begin) {
        return String();
    }

    const char *end = begin;
    while (*end != '\0') {
        ++end;
    }
    while (begin < end && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    String out;
    while (begin < end) {
        out += *begin++;
    }
    return out;
}

Logger::RecentEntry g_diag_log_snapshot[UI_DIAG_LOG_RECENT_MAX];

enum class SettingsLogSeverity : uint8_t {
    Ok = 0,
    Warn,
    Error,
};

bool should_wake_backlight_on_alert(const SensorData &data,
                                    bool gas_warmup,
                                    const DisplayThresholds::Config &thresholds) {
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, gas_warmup, thresholds);
    for (size_t i = 0; i < result.count; ++i) {
        const StatusMessages::StatusMessage &msg = result.messages[i];
        if (msg.sensor == StatusMessages::STATUS_SENSOR_CO &&
            msg.severity >= StatusMessages::STATUS_YELLOW) {
            return true;
        }
        if (msg.severity == StatusMessages::STATUS_RED) {
            return true;
        }
    }
    return false;
}

float poor_gas_background_exit_threshold(float threshold) {
    const float hysteresis = max(1.0f, threshold * UI_POOR_GAS_BG_HYSTERESIS_RATIO);
    return threshold - hysteresis;
}

lv_style_t *get_poor_gas_background_style() {
    static lv_style_t style;
    static bool initialized = false;
    if (!initialized) {
        const lv_color_t alert_color = lv_color_hex(UI_POOR_GAS_BG_HEX);
        lv_style_init(&style);
        lv_style_set_bg_opa(&style, LV_OPA_COVER);
        lv_style_set_bg_color(&style, alert_color);
        lv_style_set_bg_grad_color(&style, alert_color);
        lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_NONE);
        initialized = true;
    }
    return &style;
}

lv_style_t *get_high_co2_background_style() {
    static lv_style_t style;
    static bool initialized = false;
    if (!initialized) {
        const lv_color_t alert_color = lv_color_hex(UI_HIGH_CO2_BG_HEX);
        lv_style_init(&style);
        lv_style_set_bg_opa(&style, LV_OPA_COVER);
        lv_style_set_bg_color(&style, alert_color);
        lv_style_set_bg_grad_color(&style, alert_color);
        lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_NONE);
        initialized = true;
    }
    return &style;
}

char diag_log_level_char(Logger::Level level) {
    switch (level) {
        case Logger::Error: return 'E';
        case Logger::Warn: return 'W';
        case Logger::Info: return 'I';
        case Logger::Debug: return 'D';
        default: return '?';
    }
}

void set_label_hidden(lv_obj_t *label, bool hidden) {
    if (!label) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

void format_clock_time_label(const tm &local_tm,
                             bool time_format_24h,
                             char *time_buf,
                             size_t time_buf_size,
                             char *ampm_buf,
                             size_t ampm_buf_size) {
    if (!time_buf || time_buf_size == 0) {
        return;
    }
    if (time_format_24h) {
        snprintf(time_buf, time_buf_size, "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
        if (ampm_buf && ampm_buf_size > 0) {
            ampm_buf[0] = '\0';
        }
        return;
    }

    int display_hour = local_tm.tm_hour % 12;
    if (display_hour == 0) {
        display_hour = 12;
    }
    snprintf(time_buf, time_buf_size, "%d:%02d", display_hour, local_tm.tm_min);
    if (ampm_buf && ampm_buf_size > 0) {
        snprintf(ampm_buf, ampm_buf_size, "%s", local_tm.tm_hour >= 12 ? "PM" : "AM");
    }
}

bool should_show_diag_log_entry(const Logger::RecentEntry &entry) {
    if (entry.level != Logger::Error && entry.level != Logger::Warn) {
        return false;
    }
    if (SystemLogFilter::isSoftWarning(entry.level, entry.tag, entry.message)) {
        return false;
    }
    return true;
}

bool should_show_unacknowledged_diag_log_entry(const Logger::RecentEntry &entry,
                                               uint32_t acknowledged_alert_seq) {
    if (!should_show_diag_log_entry(entry)) {
        return false;
    }
    return entry.seq > acknowledged_alert_seq;
}

void compact_diag_log_message(const char *src, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src || src[0] == '\0') {
        return;
    }

    size_t out = 0;
    bool last_space = false;
    while (*src && out + 1 < dst_size) {
        char c = *src++;
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
        if (c == ' ') {
            if (out == 0 || last_space) {
                continue;
            }
            last_space = true;
        } else {
            last_space = false;
        }
        dst[out++] = c;
    }

    while (out > 0 && dst[out - 1] == ' ') {
        --out;
    }
    dst[out] = '\0';

    if (*src && dst_size > 4) {
        dst[dst_size - 4] = '.';
        dst[dst_size - 3] = '.';
        dst[dst_size - 2] = '.';
        dst[dst_size - 1] = '\0';
    }
}

bool append_diag_log_line(char *text,
                          size_t text_capacity,
                          size_t &used,
                          char level_char,
                          const char *tag,
                          const char *message) {
    if (!text || text_capacity == 0 || used >= text_capacity) {
        return false;
    }

    char compact_message[UI_DIAG_LOG_MESSAGE_MAX_CHARS + 1];
    compact_diag_log_message(message, compact_message, sizeof(compact_message));

    char line[UI_DIAG_LOG_LINE_CAPACITY];
    const int written = snprintf(line,
                                 sizeof(line),
                                 "%c %-8.8s %s",
                                 level_char,
                                 (tag && tag[0] != '\0') ? tag : "SYSTEM",
                                 compact_message[0] ? compact_message : "Event");
    if (written <= 0) {
        return false;
    }

    const size_t line_len = strnlen(line, sizeof(line));
    const size_t needed = line_len + (used > 0 ? 1 : 0);
    if (needed >= (text_capacity - used)) {
        return false;
    }
    if (used > 0) {
        text[used++] = '\n';
    }
    memcpy(text + used, line, line_len);
    used += line_len;
    text[used] = '\0';
    return true;
}

SettingsLogSeverity summarize_settings_log_severity(uint32_t acknowledged_alert_seq) {
    Logger::RecentEntry recent[8];
    const size_t count = Logger::copyRecentAlerts(recent, sizeof(recent) / sizeof(recent[0]));
    bool has_warn = false;
    for (size_t i = 0; i < count; ++i) {
        if (!should_show_unacknowledged_diag_log_entry(recent[i], acknowledged_alert_seq)) {
            continue;
        }
        if (recent[i].level == Logger::Error) {
            return SettingsLogSeverity::Error;
        }
        if (recent[i].level == Logger::Warn) {
            has_warn = true;
        }
    }
    return has_warn ? SettingsLogSeverity::Warn : SettingsLogSeverity::Ok;
}

} // namespace

UiController *UiController::instance_ = nullptr;

UiController::UiController(const UiContext &context)
    : storage(context.storage),
      networkManager(context.networkManager),
      mqttManager(context.mqttManager),
      connectivityRuntime(context.connectivityRuntime),
      mqttRuntimeState(context.mqttRuntimeState),
      webUiBridge(context.webUiBridge),
      displayThresholds(context.displayThresholds),
      networkCommandQueue(context.networkCommandQueue),
      sensorManager(context.sensorManager),
      chartsHistory(context.chartsHistory),
      dailyExtremaHistory(context.dailyExtremaHistory),
      timeManager(context.timeManager),
      themeManager(context.themeManager),
      backlightManager(context.backlightManager),
      nightModeManager(context.nightModeManager),
      fanControl(context.fanControl),
      currentData(context.currentData),
      night_mode(context.night_mode),
      temp_units_c(context.temp_units_c),
      led_indicators_enabled(context.led_indicators_enabled),
      alert_blink_enabled(context.alert_blink_enabled),
      co2_asc_enabled(context.co2_asc_enabled),
      temp_offset(context.temp_offset),
      hum_offset(context.hum_offset) {
    instance_ = this;
    webUiBridge.bindSettingsApplier(this, &UiController::applyWebUiSettingsBridge);
    webUiBridge.bindThemeApplier(this, &UiController::applyThemePreviewBridge);
    webUiBridge.bindDacActionApplier(this, &UiController::applyDacActionBridge);
    webUiBridge.bindDacAutoApplier(this, &UiController::applyDacAutoBridge);
    webUiBridge.bindWifiSaveApplier(this, &UiController::applyWifiSaveBridge);
    webUiBridge.bindMqttSaveApplier(this, &UiController::applyMqttSaveBridge);
    refreshConnectivitySnapshot();
    publishWebUiSnapshot();
}

void UiController::setLvglReady(bool ready) {
    lvgl_ready = ready;
}

bool UiController::isLvglRuntimeHealthy() const {
    if (!lvgl_ready ||
        lvgl_diag_stall_active ||
        lvgl_diag_auto_recovery_restart_requested_ ||
        lvgl_diag_auto_recovery_suppressed_) {
        return false;
    }
    lvgl_port_diagnostics_t diagnostics = {};
    return lvgl_port_get_diagnostics(&diagnostics) &&
           !diagnostics.display_sync_fault;
}

void UiController::refreshConnectivitySnapshot() {
    connectivity_ = connectivityRuntime.snapshot();
    if (!wifi_override_active_) {
        return;
    }
    if (connectivity_.wifi_enabled == wifi_override_enabled_) {
        wifi_override_active_ = false;
        return;
    }
    connectivity_.wifi_enabled = wifi_override_enabled_;
    connectivity_.wifi_connected = false;
    connectivity_.sta_ip.clear();
    if (wifi_override_enabled_) {
        connectivity_.wifi_state = connectivity_.wifi_ssid.isEmpty()
                                       ? static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)
                                       : static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTING);
    } else {
        connectivity_.wifi_state = static_cast<int>(AuraNetworkManager::WIFI_STATE_OFF);
        connectivity_.wifi_retry_count = 0;
    }
}

void UiController::syncConnectivityRuntime() {
    refreshConnectivitySnapshot();
}

WebUiBridge::Snapshot UiController::buildWebUiSnapshot() const {
    WebUiBridge::Snapshot snapshot;
    snapshot.available = lvgl_ready;
    snapshot.night_mode = night_mode;
    snapshot.night_mode_locked = nightModeManager.isAutoEnabled();
    snapshot.backlight_on = backlightManager.isOn();
    snapshot.ntp_enabled = timeManager.isNtpEnabledPref();
    snapshot.ntp_active = timeManager.isNtpEnabled();
    snapshot.ntp_syncing = timeManager.isNtpSyncing();
    snapshot.ntp_error = timeManager.isNtpError();
    snapshot.units_c = temp_units_c;
    snapshot.time_format_24h = time_format_24h_;
    snapshot.temp_offset = temp_offset;
    snapshot.hum_offset = hum_offset;
    snapshot.pressure_altitude_set = storage.config().pressure_altitude_set;
    snapshot.pressure_altitude_m = storage.config().pressure_altitude_m;
    snapshot.ntp_last_sync_ms = timeManager.lastNtpSyncMs();
    snapshot.ntp_server = timeManager.ntpServerPref();
    snapshot.display_name = storage.config().web_display_name;
    snapshot.theme_preview_colors = themeManager.previewOrCurrent();
    if (!lvgl_ready) {
        snapshot.mqtt_screen_open = false;
        snapshot.theme_screen_open = false;
        snapshot.theme_custom_screen_open = false;
        return snapshot;
    }

    snapshot.mqtt_screen_open = current_screen_id == SCREEN_ID_PAGE_MQTT ||
                                pending_screen_id == SCREEN_ID_PAGE_MQTT;
    const bool theme_screen_open = current_screen_id == SCREEN_ID_PAGE_THEME ||
                                   pending_screen_id == SCREEN_ID_PAGE_THEME;
    const bool custom_tab_selected =
        objects.btn_theme_custom && lv_obj_is_valid(objects.btn_theme_custom) &&
        lv_obj_has_state(objects.btn_theme_custom, LV_STATE_CHECKED);
    snapshot.theme_screen_open = theme_screen_open;
    snapshot.theme_custom_screen_open = theme_screen_open && custom_tab_selected;
    return snapshot;
}

void UiController::publishWebUiSnapshot() {
    webUiBridge.publishSnapshot(buildWebUiSnapshot());
}

void UiController::processWebUiBridgeCommands() {
    WebUiBridge::SettingsUpdate settings_update;
    uint32_t settings_request_id = 0;
    if (webUiBridge.consumePendingSettingsRequest(settings_update, settings_request_id)) {
        webUiBridge.completePendingSettingsRequest(
            settings_request_id,
            applyWebUiSettingsBridge(settings_update, this));
    }

    WebUiBridge::ThemeUpdate theme_update;
    uint32_t theme_request_id = 0;
    if (webUiBridge.consumePendingThemeRequest(theme_update, theme_request_id)) {
        webUiBridge.completePendingThemeRequest(
            theme_request_id,
            applyThemePreviewBridge(theme_update, this));
    }

    WebUiBridge::DacActionUpdate dac_action_update;
    uint32_t dac_action_request_id = 0;
    if (webUiBridge.consumePendingDacActionRequest(dac_action_update, dac_action_request_id)) {
        webUiBridge.completePendingDacActionRequest(
            dac_action_request_id,
            applyDacActionBridge(dac_action_update, this));
    }

    WebUiBridge::DacAutoUpdate dac_auto_update;
    uint32_t dac_auto_request_id = 0;
    if (webUiBridge.consumePendingDacAutoRequest(dac_auto_update, dac_auto_request_id)) {
        webUiBridge.completePendingDacAutoRequest(
            dac_auto_request_id,
            applyDacAutoBridge(dac_auto_update, this));
    }

    WebUiBridge::WifiSaveUpdate wifi_save_update;
    uint32_t wifi_save_request_id = 0;
    if (webUiBridge.consumePendingWifiSaveRequest(wifi_save_update, wifi_save_request_id)) {
        webUiBridge.completePendingWifiSaveRequest(
            wifi_save_request_id,
            applyWifiSaveBridge(wifi_save_update, this));
    }

    WebUiBridge::MqttSaveUpdate mqtt_save_update;
    uint32_t mqtt_save_request_id = 0;
    if (webUiBridge.consumePendingMqttSaveRequest(mqtt_save_update, mqtt_save_request_id)) {
        webUiBridge.completePendingMqttSaveRequest(
            mqtt_save_request_id,
            applyMqttSaveBridge(mqtt_save_update, this));
    }

    WebUiBridge::FirmwareUpdateScreenMode firmware_update_screen_mode =
        WebUiBridge::FirmwareUpdateScreenMode::Hidden;
    if (!webUiBridge.consumePendingFirmwareUpdateScreen(firmware_update_screen_mode)) {
        return;
    }
    webSetFirmwareUpdateScreen(firmware_update_screen_mode);
    publishWebUiSnapshot();
}

WebUiBridge::ApplyResult UiController::applyWebUiSettingsBridge(
    const WebUiBridge::SettingsUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    WebUiBridge::ApplyResult result{};
    result.restart_requested = update.restart_requested;
    if (!controller) {
        result.success = false;
        result.status_code = 503;
        result.error_message = "UI bridge unavailable";
        return result;
    }

    if (update.has_night_mode && controller->nightModeManager.isAutoEnabled()) {
        result.success = false;
        result.status_code = 409;
        result.error_message = "night_mode is locked by auto mode";
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    }

    const bool previous_backlight = controller->webBacklightOn();
    const bool previous_night_mode = controller->webNightModeEnabled();
    const bool previous_units_c = controller->webUnitsC();
    const float previous_temp_offset = controller->webTempOffset();
    const float previous_hum_offset = controller->webHumOffset();
    const bool previous_ntp_enabled = controller->timeManager.isNtpEnabledPref();
    const String previous_ntp_server =
        update.has_ntp_server ? controller->timeManager.ntpServerPref() : String();
    const String previous_display_name =
        update.has_display_name ? controller->storage.config().web_display_name : String();

    bool applied_backlight = false;
    bool applied_night_mode = false;
    bool applied_ntp_enabled = false;
    bool applied_ntp_server = false;
    bool applied_units = false;
    bool applied_offsets = false;
    bool applied_display_name = false;

    auto finalize = [&](bool success, uint16_t status_code, const char *message) {
        result.success = success;
        result.status_code = status_code;
        result.error_message = message ? message : "";
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    };

    auto rollback = [&]() -> bool {
        bool rollback_failed = false;

        if (applied_backlight && controller->webBacklightOn() != previous_backlight) {
            if (!controller->webSetBacklight(previous_backlight)) {
                rollback_failed = true;
            }
        }
        if (applied_night_mode && controller->webNightModeEnabled() != previous_night_mode) {
            if (!controller->webSetNightMode(previous_night_mode)) {
                rollback_failed = true;
            }
        }
        if (applied_ntp_enabled && controller->timeManager.isNtpEnabledPref() != previous_ntp_enabled) {
            if (!controller->webSetNtpEnabled(previous_ntp_enabled)) {
                rollback_failed = true;
            }
        }
        if (applied_ntp_server && controller->timeManager.ntpServerPref() != previous_ntp_server) {
            if (!controller->webSetNtpServer(previous_ntp_server)) {
                rollback_failed = true;
            }
        }
        if (applied_units && controller->webUnitsC() != previous_units_c) {
            if (!controller->webSetUnitsC(previous_units_c)) {
                rollback_failed = true;
            }
        }
        if (applied_offsets) {
            const bool temp_changed =
                fabsf(controller->webTempOffset() - previous_temp_offset) > 0.0001f;
            const bool hum_changed =
                fabsf(controller->webHumOffset() - previous_hum_offset) > 0.0001f;
            if ((temp_changed || hum_changed) &&
                !controller->webSetOffsets(previous_temp_offset, previous_hum_offset)) {
                rollback_failed = true;
            }
        }
        if (applied_display_name) {
            controller->storage.config().web_display_name = previous_display_name;
            if (!controller->storage.saveConfig(true)) {
                controller->storage.requestSave();
                rollback_failed = true;
            }
        }

        return !rollback_failed;
    };

    auto fail = [&](uint16_t status_code, const char *message) {
        if (!rollback()) {
            LOGE("UI", "web settings rollback failed");
            return finalize(false, 500, "Failed to apply settings atomically");
        }
        return finalize(false, status_code, message);
    };

    if (update.has_ntp_enabled && !update.ntp_enabled) {
        if (!controller->webSetNtpEnabled(false)) {
            return fail(500, "Failed to persist NTP setting");
        }
        applied_ntp_enabled = true;
    }

    if (update.has_ntp_server) {
        if (!controller->webSetNtpServer(update.ntp_server)) {
            return fail(500, "Failed to persist NTP server");
        }
        applied_ntp_server = true;
    }

    if (update.has_ntp_enabled && update.ntp_enabled) {
        if (!controller->webSetNtpEnabled(true)) {
            return fail(500, "Failed to persist NTP setting");
        }
        applied_ntp_enabled = true;
    }

    if (update.has_units_c) {
        if (!controller->webSetUnitsC(update.units_c)) {
            return fail(500, "Failed to persist units setting");
        }
        applied_units = true;
    }

    if (update.has_temp_offset || update.has_hum_offset) {
        const float requested_temp_offset =
            update.has_temp_offset ? update.temp_offset : previous_temp_offset;
        const float requested_hum_offset =
            update.has_hum_offset ? update.hum_offset : previous_hum_offset;
        if (!controller->webSetOffsets(requested_temp_offset, requested_hum_offset)) {
            return fail(500, "Failed to persist offsets");
        }
        applied_offsets = true;
    }

    if (update.has_display_name) {
        controller->storage.config().web_display_name = update.display_name;
        if (!controller->storage.saveConfig(true)) {
            controller->storage.config().web_display_name = previous_display_name;
            controller->storage.requestSave();
            return fail(500, "Failed to persist display_name");
        }
        applied_display_name = true;
    }

    if (update.has_night_mode) {
        if (!controller->webSetNightMode(update.night_mode)) {
            return fail(409, "night_mode is locked by auto mode");
        }
        applied_night_mode = true;
    }

    if (update.has_backlight) {
        if (!controller->webSetBacklight(update.backlight_on)) {
            return fail(409, "backlight state could not be applied");
        }
        applied_backlight = true;
    }

    return finalize(true, 200, nullptr);
}

WebUiBridge::ApplyResult UiController::applyThemePreviewBridge(
    const WebUiBridge::ThemeUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    WebUiBridge::ApplyResult result{};
    if (!controller) {
        result.success = false;
        result.status_code = 503;
        result.error_message = "Theme bridge unavailable";
        return result;
    }

    auto finalize = [&](bool success, uint16_t status_code, const char *message) {
        result.success = success;
        result.status_code = status_code;
        result.error_message = message ? message : "";
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    };

    if (!controller->lvgl_ready) {
        return finalize(false, 503, "LVGL unavailable");
    }
    if (!lvgl_port_lock(UI_LVGL_LOCK_TIMEOUT_MS)) {
        return finalize(false, 503, "LVGL unavailable");
    }

    controller->themeManager.applyPreviewCustom(update.colors);
    const bool unlock_ok = lvgl_port_unlock();
    if (!unlock_ok) {
        return finalize(false, 500, "LVGL unlock failed");
    }
    controller->data_dirty = true;
    return finalize(true, 200, nullptr);
}

bool UiController::consumeNetworkUiDirty() {
    if (!networkManager.isUiDirty()) {
        return false;
    }
    networkManager.clearUiDirty();
    return true;
}

bool UiController::consumeMqttUiDirty() {
    if (!mqttManager.isUiDirty()) {
        return false;
    }
    mqttManager.clearUiDirty();
    return true;
}

void UiController::applyPendingWifiEnabled() {
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::ApplyPendingWifiEnabled)) {
        LOGW("UI", "network command queue full: ApplyPendingWifiEnabled");
    }
}

void UiController::setMqttScreenOpenState(bool open) {
    if (open) {
        mqttManager.markUiDirty();
    }
    webUiBridge.setMqttScreenOpen(open);
    publishWebUiSnapshot();
}

void UiController::setThemeScreenOpenState(bool open) {
    themeManager.setThemeScreenOpen(open);
    if (!open) {
        themeManager.setCustomTabSelected(false);
    }
    webUiBridge.setThemeScreenOpen(open, open && themeManager.isCustomScreenOpen());
    publishWebUiSnapshot();
}

void UiController::setWifiEnabledFromUi(bool enabled) {
    refreshConnectivitySnapshot();
    if (enabled == connectivity_.wifi_enabled) {
        return;
    }
    wifi_override_active_ = true;
    wifi_override_enabled_ = enabled;
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::SetWifiEnabled, enabled)) {
        LOGW("UI", "network command queue full: SetWifiEnabled");
        wifi_override_active_ = false;
    } else {
        refreshConnectivitySnapshot();
        update_wifi_ui();
        markWebPagePanelDirty();
    }
    datetime_ui_dirty = true;
}

void UiController::setMqttUserEnabledFromUi(bool enabled) {
    refreshConnectivitySnapshot();
    if (enabled == connectivity_.mqtt_user_enabled) {
        return;
    }
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::SetMqttUserEnabled, enabled)) {
        LOGW("UI", "network command queue full: SetMqttUserEnabled");
    }
}

void UiController::requestMqttReconnectFromUi() {
    refreshConnectivitySnapshot();
    if (!connectivity_.mqtt_enabled || !connectivity_.wifi_enabled || !connectivity_.wifi_connected) {
        return;
    }
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::RequestMqttReconnect)) {
        LOGW("UI", "network command queue full: RequestMqttReconnect");
    }
}

void UiController::requestWifiReconnectFromUi() {
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::RequestWifiReconnect)) {
        LOGW("UI", "network command queue full: RequestWifiReconnect");
    }
    datetime_ui_dirty = true;
}

void UiController::toggleWifiApModeFromUi() {
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::ToggleWifiApMode)) {
        LOGW("UI", "network command queue full: ToggleWifiApMode");
    }
    datetime_ui_dirty = true;
}

void UiController::clearWifiCredentialsFromUi() {
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::ClearWifiCredentials)) {
        LOGW("UI", "network command queue full: ClearWifiCredentials");
    }
    datetime_ui_dirty = true;
}

void UiController::markWifiUiDirty() {
    networkManager.markUiDirty();
}

void UiController::bind_available_events(int screen_id) {
    UiEventBinder::bindAvailableEvents(*this, screen_id);
}

void UiController::apply_toggle_styles_for_available_objects(int screen_id) {
    UiEventBinder::applyToggleStylesForAvailableObjects(*this, screen_id);
}

void UiController::apply_checked_states_for_available_objects(int screen_id) {
    UiEventBinder::applyCheckedStatesForAvailableObjects(*this, screen_id);
}

void UiController::init_theme_controls_if_available() {
    UiEventBinder::initThemeControlsIfAvailable(*this);
}

void UiController::bind_screen_events_once(int screen_id) {
    if (screen_id <= 0 || screen_id >= static_cast<int>(kScreenSlotCount)) {
        return;
    }
    if (!UiEventBinder::screenRootById(screen_id)) {
        return;
    }
    if (screen_events_bound_[screen_id]) {
        return;
    }

    bind_available_events(screen_id);
    apply_toggle_styles_for_available_objects(screen_id);
    apply_checked_states_for_available_objects(screen_id);
    refresh_texts_for_screen(screen_id);
    init_theme_controls_if_available();
    if (screen_id == SCREEN_ID_PAGE_SENSORS_INFO) {
        restore_sensor_info_selection();
    }

    screen_events_bound_[screen_id] = true;
}

void UiController::refresh_texts_for_screen(int screen_id) {
    UiLocalization::refreshTextsForScreen(*this, screen_id);
}

bool UiController::begin() {
    instance_ = this;
    if (!lvgl_ready) {
        return false;
    }
    if (!lvgl_port_lock(UI_LVGL_STARTUP_LOCK_TIMEOUT_MS)) {
        LOGE("UI", "LVGL lock timed out in begin after %d ms", UI_LVGL_STARTUP_LOCK_TIMEOUT_MS);
        return false;
    }
    ui_init();
    themeManager.initAfterUi(storage, night_mode, datetime_ui_dirty);
    if (night_mode) {
        night_mode_on_enter();
    }
    init_ui_defaults();
    display_thresholds_revision_ = displayThresholds.revision();
    sync_display_threshold_labels();
    if (objects.label_boot_ver) {
        char version_text[32];
        snprintf(version_text, sizeof(version_text), "v%s", AppVersion::fullVersion());
        safe_label_set_text(objects.label_boot_ver, version_text);
    }
    current_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
    pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
    memset(screen_events_bound_, 0, sizeof(screen_events_bound_));
    theme_events_bound_ = false;
    lvgl_lock_fail_streak = 0;
    last_lvgl_lock_warn_ms = 0;
    lvgl_diag_last_heartbeat_ms = millis();
    lvgl_diag_prev_heartbeat_lock_fail_count = 0;
    lvgl_diag_prev_heartbeat_touch_err_count = 0;
    lvgl_diag_prev_heartbeat_vsync_timeout_count = 0;
    lvgl_diag_last_stall_warn_ms = 0;
    lvgl_diag_stall_active = false;
    lvgl_diag_stall_since_ms = 0;
    lvgl_diag_auto_recovery_restart_requested_ = false;
    lvgl_diag_auto_recovery_suppressed_ = false;
    boot_release_at_ms = 0;
    boot_ui_released = false;
    deferred_unload_.reset();
    reset_dynamic_url_caches();
    wifi_icon_state = -1;
    mqtt_icon_state = -1;
    wifi_icon_state_main = -1;
    mqtt_icon_state_main = -1;
    last_dac_ui_update_ms = 0;
    if (objects.page_boot_logo) {
        loadScreen(SCREEN_ID_PAGE_BOOT_LOGO);
        bind_screen_events_once(SCREEN_ID_PAGE_BOOT_LOGO);
        current_screen_id = SCREEN_ID_PAGE_BOOT_LOGO;
        pending_screen_id = 0;
        boot_logo_active = true;
        boot_logo_start_ms = millis();
    }
    LOGI("UI", "LVGL diagnostics enabled (heartbeat=%lu ms, stall=%lu ms, auto-reboot=%lu ms)",
         static_cast<unsigned long>(UI_LVGL_DIAG_HEARTBEAT_MS),
         static_cast<unsigned long>(UI_LVGL_DIAG_STALL_MS),
         static_cast<unsigned long>(UI_LVGL_DIAG_REBOOT_STALL_MS));
    if (!lvgl_port_unlock()) {
        LOGE("UI", "LVGL unlock failed in begin");
        return false;
    }
    last_clock_tick_ms = millis();
    publishWebUiSnapshot();
    return true;
}

void UiController::onSensorPoll(const SensorManager::PollResult &poll) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    backlightManager.setAlarmWakeActive(
        should_wake_backlight_on_alert(currentData, sensorManager.isWarmupActive(), thresholds));
    if (poll.data_changed || poll.warmup_changed) {
        data_dirty = true;
    }
}

void UiController::onTimePoll(const TimeManager::PollResult &poll) {
    if (poll.state_changed) {
        datetime_ui_dirty = true;
        data_dirty = true;
    }
    if (poll.time_updated) {
        apply_auto_night_now();
        clock_ui_dirty = true;
        datetime_ui_dirty = true;
    }
}

void UiController::markDatetimeDirty() {
    datetime_ui_dirty = true;
}

void UiController::markWebPagePanelDirty() {
    web_page_panel_dirty = true;
}

void UiController::mqtt_sync_with_wifi() {
    if (!networkCommandQueue.enqueue(NetworkCommandQueue::Type::SyncMqttWithWifi)) {
        LOGW("UI", "network command queue full: SyncMqttWithWifi");
    }
}

bool UiController::webNightModeLocked() const {
    return nightModeManager.isAutoEnabled();
}

bool UiController::webBacklightOn() const {
    return backlightManager.isOn();
}

bool UiController::webSetNightMode(bool enabled) {
    if (nightModeManager.isAutoEnabled()) {
        sync_night_mode_toggle_ui();
        return false;
    }
    bool previous = night_mode;
    set_night_mode_state(enabled, true);
    sync_night_mode_toggle_ui();
    if (night_mode != previous) {
        data_dirty = true;
        mqttRuntimeState.requestPublish();
    }
    return night_mode == enabled;
}

bool UiController::webSetBacklight(bool enabled) {
    if (!lvgl_ready) {
        // Do not re-arm wake-touch probing or issue display I/O after the
        // LVGL runtime has been permanently quiesced. A no-op request can
        // still succeed as part of an otherwise non-LVGL settings update.
        return backlightManager.isOn() == enabled;
    }

    const bool lock_required = lvgl_ready;
    if (lock_required && !lvgl_port_lock(UI_LVGL_LOCK_TIMEOUT_MS)) {
        LOGW("UI", "LVGL lock timed out while applying web backlight command");
        return false;
    }

    bool previous = backlightManager.isOn();
    const bool applied = backlightManager.setOn(enabled);
    bool changed = backlightManager.isOn() != previous;
    if (changed) {
        data_dirty = true;
        mqttRuntimeState.requestPublish();
    }
    const bool target_reached = backlightManager.isOn() == enabled;
    const bool unlock_ok = !lock_required || lvgl_port_unlock();
    if (!unlock_ok) {
        LOGE("UI", "LVGL unlock failed after web backlight command");
    }
    return applied && target_reached && unlock_ok;
}

bool UiController::webSetNtpEnabled(bool enabled) {
    timeManager.setNtpEnabledPref(enabled);
    return timeManager.isNtpEnabledPref() == enabled;
}

bool UiController::webSetNtpServer(const String &server) {
    const String normalized = trim_copy(server);
    timeManager.setNtpServerPref(normalized);
    return timeManager.ntpServerPref() == normalized;
}

bool UiController::webSetUnitsC(bool units_c) {
    if (units_c == temp_units_c) {
        return true;
    }
    const bool previous_units_c = temp_units_c;
    const Config::DateFormat previous_date_format = date_format_;
    temp_units_c = units_c;
    storage.config().units_c = temp_units_c;
    apply_contextual_date_format_default(ui_language, previous_units_c);
    if (!storage.saveConfig(true)) {
        temp_units_c = previous_units_c;
        date_format_ = previous_date_format;
        storage.config().units_c = previous_units_c;
        storage.config().date_format = previous_date_format;
        clock_ui_dirty = true;
        data_dirty = true;
        LOGE("UI", "failed to persist temperature unit change");
        return false;
    }
    clock_ui_dirty = true;
    if (lvgl_ready) {
        sync_display_threshold_labels();
    }
    dailyExtremaHistory.setPreferredUnitsC(temp_units_c);
    data_dirty = true;
    mqttRuntimeState.requestPublish();
    return true;
}

void UiController::apply_contextual_date_format_default(Config::Language previous_language,
                                                        bool previous_units_c) {
    const Config::DateFormat previous_default =
        Config::defaultDateFormat(previous_language, previous_units_c);
    const Config::DateFormat next_default =
        Config::defaultDateFormat(ui_language, temp_units_c);
    if (date_format_ != previous_default || date_format_ == next_default) {
        return;
    }
    date_format_ = next_default;
    storage.config().date_format = date_format_;
    clock_ui_dirty = true;
    datetime_ui_dirty = true;
    if (lvgl_ready) {
        update_date_format_button();
    }
}

bool UiController::webSetOffsets(float temp_offset_c, float hum_offset_pct) {
    float temp_next = lroundf(temp_offset_c * 10.0f) / 10.0f;
    if (temp_next < -5.0f) {
        temp_next = -5.0f;
    } else if (temp_next > 5.0f) {
        temp_next = 5.0f;
    }

    float hum_next = lroundf(hum_offset_pct);
    if (hum_next < HUM_OFFSET_MIN) {
        hum_next = HUM_OFFSET_MIN;
    } else if (hum_next > HUM_OFFSET_MAX) {
        hum_next = HUM_OFFSET_MAX;
    }

    const float prev_temp_offset = temp_offset;
    const float prev_hum_offset = hum_offset;
    const float prev_temp_offset_saved = temp_offset_saved;
    const float prev_hum_offset_saved = hum_offset_saved;
    const bool prev_temp_offset_dirty = temp_offset_dirty;
    const bool prev_hum_offset_dirty = hum_offset_dirty;
    const float prev_cfg_temp_offset = storage.config().temp_offset;
    const float prev_cfg_hum_offset = storage.config().hum_offset;

    bool changed = (temp_next != temp_offset) || (hum_next != hum_offset);
    temp_offset = temp_next;
    hum_offset = hum_next;
    sensorManager.setOffsets(temp_offset, hum_offset);
    temp_offset_ui_dirty = true;
    hum_offset_ui_dirty = true;

    if (!changed) {
        return true;
    }

    temp_offset_saved = temp_offset;
    hum_offset_saved = hum_offset;
    temp_offset_dirty = false;
    hum_offset_dirty = false;
    storage.config().temp_offset = temp_offset;
    storage.config().hum_offset = hum_offset;
    if (!storage.saveConfig(true)) {
        temp_offset = prev_temp_offset;
        hum_offset = prev_hum_offset;
        temp_offset_saved = prev_temp_offset_saved;
        hum_offset_saved = prev_hum_offset_saved;
        temp_offset_dirty = prev_temp_offset_dirty;
        hum_offset_dirty = prev_hum_offset_dirty;
        storage.config().temp_offset = prev_cfg_temp_offset;
        storage.config().hum_offset = prev_cfg_hum_offset;
        sensorManager.setOffsets(temp_offset, hum_offset);
        temp_offset_ui_dirty = true;
        hum_offset_ui_dirty = true;
        LOGE("UI", "failed to persist sensor offsets");
        return false;
    }
    data_dirty = true;
    mqttRuntimeState.requestPublish();
    return true;
}

void UiController::apply_pending_screen_now_from_web() {
    if (!lvgl_ready || pending_screen_id == 0) {
        return;
    }
    if (!lvgl_port_lock(UI_LVGL_LOCK_TIMEOUT_MS)) {
        LOGW("UI", "LVGL lock timeout while switching web-requested screen (pending=%d)",
             pending_screen_id);
        return;
    }
    const uint32_t now = millis();
    UiScreenFlow::processPendingScreen(*this, now);
    UiScreenFlow::processDeferredUnloads(*this, now);
    lvgl_port_unlock();
}

void UiController::webSetFirmwareUpdateScreen(WebUiBridge::FirmwareUpdateScreenMode mode) {
    if (!lvgl_ready) {
        // Firmware-screen commands may still arrive while OTA/network
        // management remains available in permanent headless mode. Consume
        // them without touching stale LVGL objects.
        firmware_update_screen_active_ = false;
        firmware_update_screen_mode_ = WebUiBridge::FirmwareUpdateScreenMode::Hidden;
        firmware_update_autoclose_due_ms_ = 0;
        return;
    }

    auto is_restore_target = [](int screen_id) {
        return screen_id >= SCREEN_ID_PAGE_MAIN_PRO &&
               screen_id <= SCREEN_ID_PAGE_DAC_SETTINGS;
    };

    if (mode != WebUiBridge::FirmwareUpdateScreenMode::Hidden) {
        if (!firmware_update_screen_active_) {
            int restore_screen = current_screen_id;
            if (!is_restore_target(restore_screen) &&
                pending_screen_id > 0 &&
                pending_screen_id != SCREEN_ID_PAGE_FW_UPDATE) {
                restore_screen = pending_screen_id;
            }
            if (!is_restore_target(restore_screen)) {
                restore_screen = SCREEN_ID_PAGE_MAIN_PRO;
            }
            firmware_update_return_screen_id_ = restore_screen;
        }
        firmware_update_screen_active_ = true;
        firmware_update_screen_mode_ = mode;
        firmware_update_autoclose_due_ms_ =
            mode == WebUiBridge::FirmwareUpdateScreenMode::ConfirmDenied
                ? millis() + 1500UL
                : 0;
        update_fw_update_ui();
        if (current_screen_id != SCREEN_ID_PAGE_FW_UPDATE) {
            pending_screen_id = SCREEN_ID_PAGE_FW_UPDATE;
            apply_pending_screen_now_from_web();
        }
        data_dirty = true;
        return;
    }

    const bool fw_current = current_screen_id == SCREEN_ID_PAGE_FW_UPDATE;
    const bool fw_pending = pending_screen_id == SCREEN_ID_PAGE_FW_UPDATE;
    firmware_update_screen_active_ = false;
    firmware_update_screen_mode_ = WebUiBridge::FirmwareUpdateScreenMode::Hidden;
    firmware_update_autoclose_due_ms_ = 0;

    if (!fw_current && !fw_pending) {
        return;
    }

    int restore_screen = firmware_update_return_screen_id_;
    if (!is_restore_target(restore_screen)) {
        restore_screen = SCREEN_ID_PAGE_MAIN_PRO;
    }
    pending_screen_id = restore_screen;
    apply_pending_screen_now_from_web();
    data_dirty = true;
}

void UiController::webRequestRestart() {
    LOGW("UI", "web restart requested");
    WebHandlersRequestRestart();
}

void UiController::sync_display_threshold_revision() {
    const uint32_t revision = displayThresholds.revision();
    if (revision == display_thresholds_revision_) {
        return;
    }
    display_thresholds_revision_ = revision;
    poor_gas_background_alert_active_ = false;
    high_co2_background_alert_active_ = false;
    invalidate_active_graph_refresh_cache();
    sync_display_threshold_labels();
    data_dirty = true;
}

void UiController::poll(uint32_t now) {
    lvgl_port_diagnostics_t early_lvgl_diag = {};
    if (lvgl_ready &&
        lvgl_port_get_diagnostics(&early_lvgl_diag) &&
        early_lvgl_diag.display_sync_fault) {
        lvgl_diag_stall_active = true;
        lvgl_diag_stall_since_ms = now;

        const UiAutoRecoveryPolicy::Decision recovery_decision =
            UiAutoRecoveryPolicy::decide(
                boot_ui_auto_recovery_reboot,
                lvgl_diag_auto_recovery_restart_requested_);
        if (recovery_decision == UiAutoRecoveryPolicy::Decision::RequestRestart) {
            LOGE("UI",
                 "Display sync fault detected (vsync_timeout=%lu); requesting controlled restart",
                 static_cast<unsigned long>(early_lvgl_diag.vsync_wait_timeout_count));
            lvgl_diag_auto_recovery_restart_requested_ = true;
            boot_mark_ui_auto_recovery_reboot();
            WebHandlersRequestRestart();
        } else if (recovery_decision ==
                   UiAutoRecoveryPolicy::Decision::SuppressRecoveryBoot) {
            lvgl_diag_auto_recovery_suppressed_ = true;
            LOGE("UI",
                 "Display sync fault persisted after automatic UI recovery; restart loop suppressed");
        }

        // The LVGL task intentionally fail-stops while owning the framebuffer.
        // Detach its timers and callbacks, then continue as a web-managed
        // headless runtime until restart or power cycle.
        lvgl_port_prepare_restart();
        lvgl_ready = false;
        publishWebUiSnapshot();
        return;
    }

    refreshConnectivitySnapshot();
    processWebUiBridgeCommands();
    if (!lvgl_ready) {
        publishWebUiSnapshot();
        return;
    }

    if (firmware_update_autoclose_due_ms_ != 0 &&
        static_cast<int32_t>(now - firmware_update_autoclose_due_ms_) >= 0) {
        webSetFirmwareUpdateScreen(WebUiBridge::FirmwareUpdateScreenMode::Hidden);
    }
    sync_display_threshold_revision();
    const bool co2_overlay_progress_active =
        co2_calib_overlay_mode_ == Co2CalibOverlayMode::CalibrationProgress ||
        co2_calib_overlay_mode_ == Co2CalibOverlayMode::AscProgress;
    backlightManager.setAlarmWakeActive(
        should_wake_backlight_on_alert(currentData,
                                       sensorManager.isWarmupActive(),
                                       displayThresholds.snapshot()));
    bool desired = false;
    if (nightModeManager.poll(night_mode, desired)) {
        set_night_mode_state(desired, true);
    }
    if (now - last_clock_tick_ms >= CLOCK_TICK_MS) {
        last_clock_tick_ms = now;
        clock_ui_dirty = true;
        if (current_screen_id == SCREEN_ID_PAGE_CLOCK && !datetime_changed) {
            datetime_ui_dirty = true;
        }
    }
    if (now - last_blink_ms >= BLINK_PERIOD_MS) {
        last_blink_ms = now;
            if (alert_blink_enabled) {
                blink_state = !blink_state;
                if (current_screen_id == SCREEN_ID_PAGE_MAIN_PRO ||
                    current_screen_id == SCREEN_ID_PAGE_SETTINGS ||
                    current_screen_id == SCREEN_ID_PAGE_SENSORS_INFO) {
                    data_dirty = true;
                }
            }
    }
    if (current_screen_id == SCREEN_ID_PAGE_MAIN_PRO &&
        status_msg_count > 1 &&
        (now - status_msg_last_ms) >= STATUS_ROTATE_MS) {
        data_dirty = true;
    }

    lvgl_port_diagnostics_t lvgl_diag = {};
    if (lvgl_port_get_diagnostics(&lvgl_diag)) {
        const uint32_t vsync_timeout_delta =
            (lvgl_diag.vsync_wait_timeout_count >=
             lvgl_diag_prev_heartbeat_vsync_timeout_count)
                ? (lvgl_diag.vsync_wait_timeout_count -
                   lvgl_diag_prev_heartbeat_vsync_timeout_count)
                : lvgl_diag.vsync_wait_timeout_count;
        if ((now - lvgl_diag_last_heartbeat_ms) >= UI_LVGL_DIAG_HEARTBEAT_MS) {
            lvgl_diag_last_heartbeat_ms = now;
            const uint32_t lock_fail_delta =
                (lvgl_diag.lock_fail_count >= lvgl_diag_prev_heartbeat_lock_fail_count)
                    ? (lvgl_diag.lock_fail_count - lvgl_diag_prev_heartbeat_lock_fail_count)
                    : lvgl_diag.lock_fail_count;
            const uint32_t touch_err_delta =
                (lvgl_diag.touch_read_error_count >= lvgl_diag_prev_heartbeat_touch_err_count)
                    ? (lvgl_diag.touch_read_error_count - lvgl_diag_prev_heartbeat_touch_err_count)
                    : lvgl_diag.touch_read_error_count;
            lvgl_diag_prev_heartbeat_lock_fail_count = lvgl_diag.lock_fail_count;
            lvgl_diag_prev_heartbeat_touch_err_count = lvgl_diag.touch_read_error_count;
            lvgl_diag_prev_heartbeat_vsync_timeout_count =
                lvgl_diag.vsync_wait_timeout_count;

            const bool suppress_expected_lock_fail_warning =
                co2_overlay_progress_active &&
                !lvgl_diag.paused &&
                lock_fail_delta > 0 &&
                touch_err_delta < UI_LVGL_DIAG_TOUCH_WARN_DELTA;
            if (lvgl_diag.paused ||
                (!suppress_expected_lock_fail_warning && lock_fail_delta > 0) ||
                vsync_timeout_delta > 0 ||
                touch_err_delta >= UI_LVGL_DIAG_TOUCH_WARN_DELTA) {
                LOGW("UI",
                     "LVGL heartbeat: handler=%lu(age=%lu ms), flush=%lu(age=%lu ms), vsync=%lu(age=%lu ms), vsync_timeout=%lu(+%lu), lock_fail=%lu(+%lu), touch_err=%lu(+%lu), paused=%s",
                     static_cast<unsigned long>(lvgl_diag.timer_handler_count),
                     static_cast<unsigned long>(lvgl_diag.timer_handler_age_ms),
                     static_cast<unsigned long>(lvgl_diag.flush_count),
                     static_cast<unsigned long>(lvgl_diag.flush_age_ms),
                     static_cast<unsigned long>(lvgl_diag.vsync_count),
                     static_cast<unsigned long>(lvgl_diag.vsync_age_ms),
                     static_cast<unsigned long>(lvgl_diag.vsync_wait_timeout_count),
                     static_cast<unsigned long>(vsync_timeout_delta),
                     static_cast<unsigned long>(lvgl_diag.lock_fail_count),
                     static_cast<unsigned long>(lock_fail_delta),
                     static_cast<unsigned long>(lvgl_diag.touch_read_error_count),
                     static_cast<unsigned long>(touch_err_delta),
                     lvgl_diag.paused ? "YES" : "NO");
            }
        }

        const bool handler_age_known = lvgl_diag.timer_handler_age_ms != UI_LVGL_DIAG_AGE_UNKNOWN_MS;
        const bool vsync_age_known = lvgl_diag.vsync_age_ms != UI_LVGL_DIAG_AGE_UNKNOWN_MS;
        const bool flush_age_known = lvgl_diag.flush_age_ms != UI_LVGL_DIAG_AGE_UNKNOWN_MS;
        const bool handler_stall = handler_age_known &&
                                   lvgl_diag.timer_handler_age_ms >= UI_LVGL_DIAG_STALL_MS;
        const bool vsync_stall = backlightManager.isOn() &&
                                 vsync_age_known &&
                                 lvgl_diag.vsync_age_ms >= UI_LVGL_DIAG_VSYNC_STALL_MS;
        const bool flush_stall = backlightManager.isOn() &&
                                 flush_age_known &&
                                 lvgl_diag.flush_age_ms >= UI_LVGL_DIAG_FLUSH_STALL_MS;
        // Flush may stay idle on static screens; do not treat flush-only inactivity as a fatal stall.
        const bool stall_suspected = !lvgl_diag.paused &&
                                     (handler_stall || vsync_stall);
        const bool firmware_update_watchdog_exempt =
            WebHandlersIsOtaUploadActive() ||
            firmware_update_screen_active_ ||
            current_screen_id == SCREEN_ID_PAGE_FW_UPDATE ||
            pending_screen_id == SCREEN_ID_PAGE_FW_UPDATE;
        if (stall_suspected) {
            if (!lvgl_diag_stall_active) {
                lvgl_diag_stall_since_ms = now;
            }
            const bool can_log = !lvgl_diag_stall_active ||
                                 (now - lvgl_diag_last_stall_warn_ms) >= UI_LVGL_DIAG_STALL_LOG_COOLDOWN_MS;
            if (can_log) {
                lvgl_diag_last_stall_warn_ms = now;
                LOGW("UI",
                     "LVGL/display stall suspected (screen=%d, backlight=%s, handler=%s, vsync=%s, flush=%s, handler_age=%lu ms, flush_age=%lu ms, vsync_age=%lu ms, vsync_timeout=%lu(+%lu), lock_fail=%lu, touch_err=%lu)",
                     current_screen_id,
                     backlightManager.isOn() ? "ON" : "OFF",
                     handler_stall ? "STALL" : "OK",
                     vsync_stall ? "STALL" : "OK",
                     flush_stall ? "STALL" : "OK",
                     static_cast<unsigned long>(lvgl_diag.timer_handler_age_ms),
                     static_cast<unsigned long>(lvgl_diag.flush_age_ms),
                     static_cast<unsigned long>(lvgl_diag.vsync_age_ms),
                     static_cast<unsigned long>(lvgl_diag.vsync_wait_timeout_count),
                     static_cast<unsigned long>(vsync_timeout_delta),
                     static_cast<unsigned long>(lvgl_diag.lock_fail_count),
                     static_cast<unsigned long>(lvgl_diag.touch_read_error_count));
            }
            lvgl_diag_stall_active = true;

            if ((now - lvgl_diag_stall_since_ms) >= UI_LVGL_DIAG_REBOOT_STALL_MS &&
                !lvgl_diag_auto_recovery_suppressed_) {
                if (firmware_update_watchdog_exempt) {
                    if (can_log) {
                        LOGW("UI", "LVGL stall persisted during firmware update screen, auto-reboot suppressed");
                    }
                    lvgl_diag_stall_since_ms = now;
                } else {
                    const UiAutoRecoveryPolicy::Decision recovery_decision =
                        UiAutoRecoveryPolicy::decide(
                            boot_ui_auto_recovery_reboot,
                            lvgl_diag_auto_recovery_restart_requested_);
                    if (recovery_decision ==
                        UiAutoRecoveryPolicy::Decision::RequestRestart) {
                        const uint32_t stall_for_ms = now - lvgl_diag_stall_since_ms;
                        LOGE("UI",
                             "LVGL stall persisted %lu ms, requesting controlled restart",
                             static_cast<unsigned long>(stall_for_ms));
                        lvgl_diag_auto_recovery_restart_requested_ = true;
                        boot_mark_ui_auto_recovery_reboot();
                        WebHandlersRequestRestart();
                        // Restart may be deferred while OTA owns the restart
                        // gate. Quiesce the stalled display runtime now so
                        // touch, callbacks and I2C cannot continue meanwhile.
                        lvgl_port_prepare_restart();
                        lvgl_ready = false;
                        publishWebUiSnapshot();
                        return;
                    }
                    if (recovery_decision ==
                        UiAutoRecoveryPolicy::Decision::SuppressRecoveryBoot) {
                        lvgl_diag_auto_recovery_suppressed_ = true;
                        LOGE("UI",
                             "LVGL stall persisted after an automatic UI recovery boot; restart loop suppressed");
                        // No restart will follow on the guarded recovery boot.
                        // Quiesce the stalled LVGL task, display callbacks and
                        // touch IRQ before entering permanent headless mode.
                        lvgl_port_prepare_restart();
                        lvgl_ready = false;
                        publishWebUiSnapshot();
                        return;
                    }
                }
            }
        } else if (lvgl_diag_stall_active &&
                   handler_age_known &&
                   lvgl_diag.timer_handler_age_ms <= UI_LVGL_DIAG_RECOVER_MS) {
            lvgl_diag_stall_active = false;
            lvgl_diag_stall_since_ms = 0;
            LOGI("UI",
                 "LVGL heartbeat recovered (handler_age=%lu ms, flush_age=%lu ms, vsync_age=%lu ms)",
                 static_cast<unsigned long>(lvgl_diag.timer_handler_age_ms),
                 static_cast<unsigned long>(lvgl_diag.flush_age_ms),
                 static_cast<unsigned long>(lvgl_diag.vsync_age_ms));
        } else if (!lvgl_diag_stall_active) {
            lvgl_diag_stall_since_ms = 0;
        }
    }

    if (boot_logo_active &&
        (now - boot_logo_start_ms) >= Config::BOOT_LOGO_MS &&
        current_screen_id == SCREEN_ID_PAGE_BOOT_LOGO &&
        pending_screen_id == 0) {
        if (objects.page_boot_diag) {
            pending_screen_id = SCREEN_ID_PAGE_BOOT_DIAG;
            boot_diag_active = true;
            boot_diag_has_error = false;
            boot_diag_start_ms = now;
            last_boot_diag_update_ms = 0;
        } else {
            pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
        }
        boot_logo_active = false;
        data_dirty = true;
    }

    if (boot_diag_active &&
        current_screen_id == SCREEN_ID_PAGE_BOOT_DIAG &&
        pending_screen_id == 0 &&
        BootDiagPolicy::shouldAutoAdvance(boot_diag_has_error,
                                          BootDiagPolicy::sen66Pending(sensorManager.isOk(),
                                                                       sensorManager.isBusy(),
                                                                       sensorManager.retryAtMs(),
                                                                       now),
                                          now - boot_diag_start_ms,
                                          Config::BOOT_DIAG_MS)) {
        pending_screen_id = SCREEN_ID_PAGE_MAIN_PRO;
        boot_diag_active = false;
        data_dirty = true;
    }

    if (!lvgl_port_lock(UI_LVGL_LOCK_TIMEOUT_MS)) {
        if (lvgl_lock_fail_streak != 0xFFFFu) {
            ++lvgl_lock_fail_streak;
        }
        if (lvgl_lock_fail_streak >= UI_LVGL_LOCK_WARN_FAIL_STREAK &&
            !co2_overlay_progress_active &&
            (now - last_lvgl_lock_warn_ms) >= UI_LVGL_LOCK_WARN_MS) {
            last_lvgl_lock_warn_ms = now;
            LOGW("UI", "LVGL lock timeout in poll (screen=%d, backlight=%s, streak=%u)",
                 current_screen_id,
                 backlightManager.isOn() ? "ON" : "OFF",
                 static_cast<unsigned>(lvgl_lock_fail_streak));
        }
        publishWebUiSnapshot();
        return;
    }
    lvgl_lock_fail_streak = 0;
    mqtt_apply_pending();
    if ((now - last_ui_tick_ms) >= UI_TICK_MS) {
        ui_tick();
        last_ui_tick_ms = now;
    }
    backlightManager.poll(lvgl_ready);
    update_status_icons();
    UiScreenFlow::processPendingScreen(*this, now);
    UiScreenFlow::processBootRelease(*this, now);
    UiScreenFlow::processDeferredUnloads(*this, now);

    UiRenderLoop::process(*this, now);
    lvgl_port_unlock();
    publishWebUiSnapshot();
}

void UiController::safe_label_set_text(lv_obj_t *obj, const char *new_text) {
    if (!obj) return;
    const char *current = lv_label_get_text(obj);
    if (current && strcmp(current, new_text) == 0) return;
    lv_label_set_text(obj, new_text);
}

void UiController::safe_label_set_text_static(lv_obj_t *obj, const char *new_text) {
    if (!obj) return;
    lv_label_set_text_static(obj, new_text);
}

void UiController::sync_display_threshold_labels() {
    if (!lvgl_ready) {
        return;
    }
    update_sensor_info_texts();
}

bool UiController::settings_has_unsaved_changes() const {
    return temp_offset_dirty || hum_offset_dirty || language_dirty;
}

bool UiController::datetime_has_unsaved_changes() const {
    return datetime_changed ||
           (rtc_detection_overlay_visible() &&
            rtc_detection_pending_mode_ != rtc_detection_saved_mode_);
}

const char *UiController::back_button_label(bool has_unsaved_changes) const {
    return has_unsaved_changes ? UiText::LabelSettingsBack() : UiText::LabelDiagBack();
}

void UiController::sync_back_button_label(lv_obj_t *label, bool has_unsaved_changes) {
    if (!label) {
        return;
    }
    safe_label_set_text_static(label, back_button_label(has_unsaved_changes));
}

void UiController::update_qrcode_if_needed(lv_obj_t *obj, const char *text, char *cache, size_t cache_size) {
    if (!obj || !text || !cache || cache_size == 0) {
        return;
    }
    if (cache[0] != '\0' && strcmp(cache, text) == 0) {
        return;
    }
    lv_qrcode_update(obj, text, strlen(text));
    snprintf(cache, cache_size, "%s", text);
}

void UiController::reset_dynamic_url_caches() {
    web_page_qr_cache_[0] = '\0';
    wifi_portal_qr_cache_[0] = '\0';
    mqtt_portal_qr_cache_[0] = '\0';
    theme_custom_qr_cache_[0] = '\0';
    dac_portal_qr_cache_[0] = '\0';
    dac_network_ui_signature_ = UINT32_MAX;
}

void UiController::update_diag_log_ui() {
    if (!objects.system_logs) {
        return;
    }

    update_diag_texts();

    lv_label_set_long_mode(objects.system_logs, LV_LABEL_LONG_CLIP);

    const size_t count = Logger::copyRecentAlerts(g_diag_log_snapshot, UI_DIAG_LOG_RECENT_MAX);

    char text[UI_DIAG_LOG_TEXT_CAPACITY];
    text[0] = '\0';
    size_t used = 0;
    size_t emitted = 0;

    for (size_t i = count; i > 0 && emitted < UI_DIAG_LOG_MAX_LINES; --i) {
        const Logger::RecentEntry &entry = g_diag_log_snapshot[i - 1];
        if (!should_show_unacknowledged_diag_log_entry(entry, diag_ack_alert_seq_)) {
            continue;
        }
        if (!append_diag_log_line(text,
                                  sizeof(text),
                                  used,
                                  diag_log_level_char(entry.level),
                                  entry.tag,
                                  entry.message)) {
            break;
        }
        emitted++;
    }

    if (emitted == 0) {
        safe_label_set_text_static(objects.system_logs, UiText::DiagNoWarningsOrErrors());
        return;
    }

    safe_label_set_text(objects.system_logs, text);
}

lv_color_t UiController::color_inactive() { return lv_color_hex(0x3a3a3a); }

lv_color_t UiController::color_green() { return lv_color_hex(0x00c853); }
lv_color_t UiController::color_yellow() { return lv_color_hex(0xffeb3b); }
lv_color_t UiController::color_orange() { return lv_color_hex(0xff9800); }
lv_color_t UiController::color_red() { return lv_color_hex(0xff1100); }
lv_color_t UiController::color_blue() { return lv_color_hex(0x2196f3); }
lv_color_t UiController::color_card_border() {
    if (objects.card_co2_pro) {
        return lv_obj_get_style_border_color(objects.card_co2_pro, LV_PART_MAIN);
    }
    return lv_color_hex(0xffe19756);
}

lv_color_t UiController::color_card_background() {
    if (objects.card_co2_pro) {
        return lv_obj_get_style_bg_color(objects.card_co2_pro, LV_PART_MAIN);
    }
    return lv_color_hex(0xff160c09);
}

lv_color_t UiController::color_for_display_band(DisplayThresholds::Band band) {
    switch (band) {
        case DisplayThresholds::Band::Green:
            return color_green();
        case DisplayThresholds::Band::Yellow:
            return color_yellow();
        case DisplayThresholds::Band::Orange:
            return color_orange();
        case DisplayThresholds::Band::Red:
            return color_red();
        case DisplayThresholds::Band::Invalid:
        default:
            return color_inactive();
    }
}

lv_color_t UiController::getTempColor(float t) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyRange(t, thresholds.temp));
}

lv_color_t UiController::getHumidityColor(float h) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyRange(h, thresholds.rh));
}

lv_color_t UiController::getAbsoluteHumidityColor(float ah) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyRange(ah, thresholds.ah));
}

lv_color_t UiController::getDewPointColor(float dew_c) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyRange(dew_c, thresholds.dew_point));
}

lv_color_t UiController::getCO2Color(int co2) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyHigh(static_cast<float>(co2), thresholds.co2));
}

lv_color_t UiController::getCOColor(float co_ppm) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyHigh(co_ppm, thresholds.co));
}

lv_color_t UiController::getPM25Color(float pm) {
    if (pm <= AQ_PM25_GREEN_MAX_UGM3) return color_green();
    if (pm <= AQ_PM25_YELLOW_MAX_UGM3) return color_yellow();
    if (pm <= AQ_PM25_ORANGE_MAX_UGM3) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM4Color(float pm) {
    if (pm <= AQ_PM4_GREEN_MAX_UGM3) return color_green();
    if (pm <= AQ_PM4_YELLOW_MAX_UGM3) return color_yellow();
    if (pm <= AQ_PM4_ORANGE_MAX_UGM3) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM10Color(float pm) {
    if (pm <= 54.0f) return color_green();
    if (pm <= 154.0f) return color_yellow();
    if (pm <= 254.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM1Color(float pm) {
    if (pm <= 10.0f) return color_green();
    if (pm <= 25.0f) return color_yellow();
    if (pm <= 50.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getPM05Color(float pm) {
    if (pm <= AQ_PM05_GREEN_MAX_PPCM3) return color_green();
    if (pm <= AQ_PM05_YELLOW_MAX_PPCM3) return color_yellow();
    if (pm <= AQ_PM05_ORANGE_MAX_PPCM3) return color_orange();
    return color_red();
}

lv_color_t UiController::getPressureDeltaColor(float delta, bool valid, bool is24h) {
    if (!valid) return color_inactive();
    float d = fabsf(delta);
    if (is24h) {
        if (d < 2.0f) return color_green();
        if (d <= 6.0f) return color_yellow();
        if (d <= 10.0f) return color_orange();
        return color_red();
    }
    if (d < 1.0f) return color_green();
    if (d <= 3.0f) return color_yellow();
    if (d <= 6.0f) return color_orange();
    return color_red();
}

lv_color_t UiController::getVOCColor(int voc) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyHigh(static_cast<float>(voc), thresholds.voc));
}

lv_color_t UiController::getNOxColor(int nox) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyHigh(static_cast<float>(nox), thresholds.nox));
}

lv_color_t UiController::getOptionalGasColor(DfrOptionalGasSensor::OptionalGasType type,
                                             float ppm,
                                             bool valid) {
    const UiOptionalGasProfile::Profile &profile = UiOptionalGasProfile::forType(type);
    if (!UiOptionalGasProfile::isKnown(type)) return color_inactive();

    switch (UiOptionalGasProfile::classify(profile, ppm, valid)) {
        case UiOptionalGasProfile::Band::Green:
            return color_green();
        case UiOptionalGasProfile::Band::Yellow:
            return color_yellow();
        case UiOptionalGasProfile::Band::Orange:
            return color_orange();
        case UiOptionalGasProfile::Band::Red:
            return color_red();
        case UiOptionalGasProfile::Band::Inactive:
        default:
            return color_inactive();
    }
}

lv_color_t UiController::getHCHOColor(float hcho_ppb, bool valid) {
    if (!valid || !isfinite(hcho_ppb) || hcho_ppb < 0.0f) return color_inactive();
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    return color_for_display_band(DisplayThresholds::classifyHigh(hcho_ppb, thresholds.hcho));
}

AirQuality UiController::getAirQuality(const SensorData &data) {
    AirQuality aq{};
    const AirQualityEngine::Result result =
        AirQualityEngine::evaluate(data, sensorManager.isWarmupActive());

    if (!result.valid) {
        aq.status = UiText::StatusInitializing();
        aq.score = 0;
        aq.color = color_blue();
        return aq;
    }

    aq.score = result.score;
    switch (result.band) {
        case AirQualityEngine::Band::Excellent:
            aq.status = UiText::QualityExcellent();
            aq.color = color_green();
            break;
        case AirQualityEngine::Band::Good:
            aq.status = UiText::QualityGood();
            aq.color = color_green();
            break;
        case AirQualityEngine::Band::Moderate:
            aq.status = UiText::QualityModerate();
            aq.color = color_yellow();
            break;
        case AirQualityEngine::Band::Poor:
        case AirQualityEngine::Band::Invalid:
        default:
            aq.status = UiText::QualityPoor();
            aq.color = color_red();
            break;
    }

    return aq;
}

bool UiController::has_poor_gas_background_alert() {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    const bool hcho_valid = currentData.hcho_valid &&
                            isfinite(currentData.hcho) &&
                            currentData.hcho >= 0.0f;
    const bool co_valid = currentData.co_sensor_present &&
                          currentData.co_valid &&
                          isfinite(currentData.co_ppm) &&
                          currentData.co_ppm >= 0.0f;

    if (!poor_gas_background_alert_active_) {
        poor_gas_background_alert_active_ =
            (thresholds.background_alerts.hcho_enabled &&
             hcho_valid &&
             currentData.hcho > thresholds.hcho.orange) ||
            (thresholds.background_alerts.co_enabled &&
             co_valid &&
             currentData.co_ppm > thresholds.co.orange);
        return poor_gas_background_alert_active_;
    }

    const float hcho_exit_threshold = poor_gas_background_exit_threshold(thresholds.hcho.orange);
    const float co_exit_threshold = poor_gas_background_exit_threshold(thresholds.co.orange);
    const bool hcho_still_alert =
        thresholds.background_alerts.hcho_enabled &&
        hcho_valid &&
        currentData.hcho >= hcho_exit_threshold;
    const bool co_still_alert =
        thresholds.background_alerts.co_enabled &&
        co_valid &&
        currentData.co_ppm >= co_exit_threshold;
    poor_gas_background_alert_active_ = hcho_still_alert || co_still_alert;
    return poor_gas_background_alert_active_;
}

bool UiController::has_high_co2_background_alert() {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    if (!thresholds.background_alerts.co2_enabled) {
        high_co2_background_alert_active_ = false;
        return false;
    }
    const bool co2_valid = currentData.co2_valid && currentData.co2 > 0;

    const float on_threshold = thresholds.co2.orange;
    if (!high_co2_background_alert_active_) {
        high_co2_background_alert_active_ = co2_valid &&
                                            static_cast<float>(currentData.co2) > on_threshold;
        return high_co2_background_alert_active_;
    }

    const float co2_exit_threshold = poor_gas_background_exit_threshold(on_threshold);
    high_co2_background_alert_active_ = co2_valid &&
                                        static_cast<float>(currentData.co2) >= co2_exit_threshold;
    return high_co2_background_alert_active_;
}

void UiController::update_main_screen_background_alert() {
    if (!objects.background_pro || !lv_obj_is_valid(objects.background_pro)) {
        return;
    }

    lv_style_t *poor_gas_style = get_poor_gas_background_style();
    lv_style_t *high_co2_style = get_high_co2_background_style();
    lv_obj_remove_style(objects.background_pro, poor_gas_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(objects.background_pro, high_co2_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (has_poor_gas_background_alert()) {
        lv_obj_add_style(objects.background_pro, poor_gas_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (has_high_co2_background_alert()) {
        lv_obj_add_style(objects.background_pro, high_co2_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::set_dot_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (color.full == color_inactive().full) {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

lv_color_t UiController::blink_red(lv_color_t color) {
    if (alert_blink_enabled && (color.full == color_red().full) && !blink_state) {
        return color_inactive();
    }
    return color;
}

lv_color_t UiController::night_alert_color(lv_color_t color) {
    if (color.full == color_red().full) {
        return color_red();
    }
    // Keep warning levels faint but still distinguishable at night.
    if (color.full == color_orange().full) {
        return lv_color_mix(color_orange(), color_inactive(), 84);
    }
    if (color.full == color_yellow().full) {
        return lv_color_mix(color_yellow(), color_inactive(), 46);
    }
    return color_inactive();
}

lv_color_t UiController::alert_color_for_mode(lv_color_t color) {
    if (night_mode) {
        return night_alert_color(color);
    }
    return blink_red(color);
}

void UiController::compute_header_style(const AirQuality &aq,
                                        uint8_t status_severity,
                                        bool co_alert_active,
                                        lv_color_t &color,
                                        lv_opa_t &shadow_opa) {
    (void)aq;

    lv_color_t base = color_card_border();
    if (header_status_enabled) {
        if (status_severity >= static_cast<uint8_t>(StatusMessages::STATUS_RED)) {
            base = color_red();
        } else if (status_severity >= static_cast<uint8_t>(StatusMessages::STATUS_ORANGE)) {
            base = color_orange();
        } else if (status_severity >= static_cast<uint8_t>(StatusMessages::STATUS_YELLOW)) {
            base = color_yellow();
        } else {
            // "All good" stays green by design.
            base = color_green();
        }
    }
    if (co_alert_active) {
        base = color_red();
    }
    shadow_opa = header_status_enabled ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (alert_blink_enabled && header_status_enabled && (base.full == color_red().full) && !blink_state) {
        color = color_inactive();
        shadow_opa = LV_OPA_TRANSP;
        return;
    }
    color = base;
}

void UiController::apply_toggle_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_border_color(btn, color_green(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_color(btn, color_green(), LV_PART_MAIN | LV_STATE_CHECKED);
}

bool UiController::pressure_altitude_is_set() const {
    return storage.config().pressure_altitude_set;
}

int UiController::pressure_altitude_meters() const {
    return storage.config().pressure_altitude_m;
}

float UiController::pressure_absolute_to_msl_hpa(float pressure_hpa, int altitude_m) const {
    if (!isfinite(pressure_hpa)) {
        return pressure_hpa;
    }
    const float altitude = static_cast<float>(altitude_m);
    const float base = 1.0f - (altitude / 44330.0f);
    if (!isfinite(base) || base <= 0.0f) {
        return pressure_hpa;
    }
    const float corrected = pressure_hpa / powf(base, 5.255f);
    return isfinite(corrected) ? corrected : pressure_hpa;
}

float UiController::pressure_delta_to_msl_hpa(float pressure_delta_hpa) const {
    if (!pressure_altitude_is_set()) {
        return pressure_delta_hpa;
    }
    // For a fixed station altitude, MSL conversion is a constant scale factor,
    // so pressure deltas scale by the same factor as absolute pressure.
    return pressure_absolute_to_msl_hpa(pressure_delta_hpa, pressure_altitude_meters());
}

float UiController::pressure_hpa_to_display_units(float pressure_hpa) const {
    if (!isfinite(pressure_hpa)) {
        return pressure_hpa;
    }
    if (!pressure_display_uses_inhg()) {
        return pressure_hpa;
    }
    constexpr float kHpaToInHg = 0.0295299830714f;
    return pressure_hpa * kHpaToInHg;
}

float UiController::pressure_to_display(float pressure_hpa) const {
    if (pressure_altitude_is_set()) {
        pressure_hpa = pressure_absolute_to_msl_hpa(pressure_hpa, pressure_altitude_meters());
    }
    return pressure_hpa_to_display_units(pressure_hpa);
}

float UiController::pressure_delta_to_display(float pressure_delta_hpa) const {
    pressure_delta_hpa = pressure_delta_to_msl_hpa(pressure_delta_hpa);
    return pressure_hpa_to_display_units(pressure_delta_hpa);
}

const char *UiController::pressure_display_unit() const {
    return pressure_display_uses_inhg() ? "inHg" : "hPa";
}

void UiController::reset_pressure_altitude_pending() {
    pressure_altitude_pending_m_ =
        constrain(static_cast<int>(storage.config().pressure_altitude_m),
                  static_cast<int>(Config::PRESSURE_ALTITUDE_MIN_M),
                  static_cast<int>(Config::PRESSURE_ALTITUDE_MAX_M));
}

void UiController::style_accent_button(lv_obj_t *btn, lv_obj_t *label, lv_color_t accent) {
    if (!btn) {
        return;
    }
    const lv_color_t bg = lv_color_mix(accent, lv_color_black(), 84);
    const lv_color_t bg_grad = lv_color_mix(accent, lv_color_black(), 44);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(btn, bg_grad, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, accent, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(btn, accent, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (label) {
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::update_pressure_altitude_preview() {
    char buf[20];
    snprintf(buf, sizeof(buf), "%dm", pressure_altitude_pending_m_);
    safe_label_set_text(objects.altitude_value_meters, buf);

    constexpr float kMetersToFeet = 3.28084f;
    const long altitude_ft = lroundf(static_cast<float>(pressure_altitude_pending_m_) * kMetersToFeet);
    snprintf(buf, sizeof(buf), "%ldft", altitude_ft);
    safe_label_set_text(objects.altitude_value_ft, buf);

    const char *unit = pressure_display_unit();
    safe_label_set_text(objects.abs_pressure_unit, unit);
    safe_label_set_text(objects.msl_pressure_unit, unit);

    if (!currentData.pressure_valid || !isfinite(currentData.pressure)) {
        safe_label_set_text_static(objects.abs_pressure_value, UiText::ValueMissing());
        safe_label_set_text_static(objects.msl_pressure_value, UiText::ValueMissing());
        return;
    }

    const float abs_display = pressure_hpa_to_display_units(currentData.pressure);
    const float msl_hpa =
        pressure_absolute_to_msl_hpa(currentData.pressure, pressure_altitude_pending_m_);
    const float msl_display = pressure_hpa_to_display_units(msl_hpa);
    const char *format = pressure_display_uses_inhg() ? "%.1f" : "%.0f";

    snprintf(buf, sizeof(buf), format, abs_display);
    safe_label_set_text(objects.abs_pressure_value, buf);
    snprintf(buf, sizeof(buf), format, msl_display);
    safe_label_set_text(objects.msl_pressure_value, buf);
}

void UiController::sync_pressure_altitude_ui() {
    const bool pressure_selected =
        (info_sensor == INFO_PRESSURE_3H) || (info_sensor == INFO_PRESSURE_24H);
    const bool overlay_visible = pressure_selected && pressure_altitude_overlay_open_;

    set_visible(objects.container_set_altitude, overlay_visible);
    set_visible(objects.btn_set_altitude, pressure_selected && !overlay_visible);

    if (pressure_selected && !overlay_visible) {
        style_accent_button(objects.btn_set_altitude,
                            objects.label_btn_set_altitude,
                            pressure_altitude_is_set() ? color_green() : color_yellow());
    }

    if (!pressure_selected) {
        pressure_altitude_overlay_open_ = false;
        set_visible(objects.label_sensor_info_title, true);
        set_visible(objects.label_sensor_value, true);
        set_visible(objects.dot_sensor_info, true);
        set_visible(objects.label_sensor_info_unit, true);
        return;
    }

    const bool header_visible = !overlay_visible;
    set_visible(objects.label_sensor_info_title, header_visible);
    set_visible(objects.label_sensor_value, header_visible);
    set_visible(objects.dot_sensor_info, header_visible);
    set_visible(objects.label_sensor_info_unit, header_visible);
    set_visible(objects.pressure_info, !overlay_visible);
    set_visible(objects.container_thresholds_dots, !overlay_visible && should_show_threshold_dots());

    if (overlay_visible) {
        set_visible(objects.btn_info_graph, false);
        update_pressure_altitude_preview();
        if (objects.container_set_altitude) {
            lv_obj_move_foreground(objects.container_set_altitude);
        }
        if (objects.btn_back_1) {
            lv_obj_move_foreground(objects.btn_back_1);
        }
        return;
    }

    set_pressure_info_mode(pressure_graph_mode_);
}

bool UiController::co2_calib_confirm_visible() const {
    return objects.overlay_co2_calib_confirm &&
           !lv_obj_has_flag(objects.overlay_co2_calib_confirm, LV_OBJ_FLAG_HIDDEN);
}

bool UiController::co2_calib_confirm_interactive() const {
    return co2_calib_overlay_mode_ == Co2CalibOverlayMode::Confirm;
}

void UiController::clear_co2_calib_overlay_timer() {
    if (co2_calib_overlay_timer_) {
        lv_timer_del(co2_calib_overlay_timer_);
        co2_calib_overlay_timer_ = nullptr;
    }
}

void UiController::co2_calib_overlay_timer_cb(lv_timer_t *timer) {
    if (!timer) {
        return;
    }
    UiController *owner = static_cast<UiController *>(timer->user_data);
    if (owner) {
        owner->co2_calib_overlay_timer_ = nullptr;
        owner->co2_calib_overlay_mode_ =
            UiCo2Workflow::nextOverlayMode(owner->co2_calib_overlay_mode_,
                                           UiCo2Workflow::OverlayTransition::Hide);
        owner->update_co2_calib_overlay_ui();
    }
    lv_timer_del(timer);
}

void UiController::arm_co2_calib_overlay_autohide(uint32_t delay_ms) {
    clear_co2_calib_overlay_timer();
    if (delay_ms == 0) {
        co2_calib_overlay_mode_ =
            UiCo2Workflow::nextOverlayMode(co2_calib_overlay_mode_,
                                           UiCo2Workflow::OverlayTransition::Hide);
        update_co2_calib_overlay_ui();
        return;
    }

    co2_calib_overlay_timer_ = lv_timer_create(co2_calib_overlay_timer_cb, delay_ms, this);
    if (co2_calib_overlay_timer_) {
        lv_timer_set_repeat_count(co2_calib_overlay_timer_, 1);
    }
}

void UiController::sync_co2_asc_toggle_ui() {
    if (!objects.btn_co2_calib_asc) {
        return;
    }
    if (co2_asc_enabled) {
        lv_obj_add_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_co2_calib_asc, LV_STATE_CHECKED);
    }
}

void UiController::update_co2_calib_overlay_ui() {
    const bool visible = co2_calib_overlay_mode_ != Co2CalibOverlayMode::Hidden;
    const bool confirm_visible = co2_calib_overlay_mode_ == Co2CalibOverlayMode::Confirm;
    const bool calib_progress_visible =
        co2_calib_overlay_mode_ == Co2CalibOverlayMode::CalibrationProgress;
    const bool asc_progress_visible =
        co2_calib_overlay_mode_ == Co2CalibOverlayMode::AscProgress;

    set_visible(objects.overlay_co2_calib_confirm, visible);
    set_visible(objects.btn_co2_calib_confirm_ok, confirm_visible);
    set_visible(objects.btn_co2_calib_confirm_cancel, confirm_visible);
    set_visible(objects.label_co2_calib_confirm_title, confirm_visible);
    set_visible(objects.label_co2_calib_confirm_text, confirm_visible);
    set_visible(objects.label_co2_calib_progress_title, calib_progress_visible);
    set_visible(objects.label_asc_progress_title, asc_progress_visible);

    if (asc_progress_visible && objects.label_asc_progress_title) {
        safe_label_set_text(objects.label_asc_progress_title,
                            co2_asc_progress_enabling_
                                ? UiText::LabelCo2AscProgressEnableTitle()
                                : UiText::LabelCo2AscProgressDisableTitle());
    }

    if (visible && objects.overlay_co2_calib_confirm) {
        lv_obj_add_flag(objects.overlay_co2_calib_confirm, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(objects.overlay_co2_calib_confirm);
    }
}

void UiController::set_co2_calib_confirm_visible(bool visible) {
    clear_co2_calib_overlay_timer();
    co2_calib_overlay_mode_ = UiCo2Workflow::nextOverlayMode(
        co2_calib_overlay_mode_,
        visible ? UiCo2Workflow::OverlayTransition::ShowConfirm
                : UiCo2Workflow::OverlayTransition::Hide);
    update_co2_calib_overlay_ui();
}

void UiController::set_co2_calib_progress_visible(bool visible) {
    clear_co2_calib_overlay_timer();
    co2_calib_overlay_mode_ = UiCo2Workflow::nextOverlayMode(
        co2_calib_overlay_mode_,
        visible ? UiCo2Workflow::OverlayTransition::ShowCalibrationProgress
                : UiCo2Workflow::OverlayTransition::Hide);
    update_co2_calib_overlay_ui();
}

void UiController::set_co2_asc_progress_visible(bool visible, bool enabling) {
    clear_co2_calib_overlay_timer();
    co2_asc_progress_enabling_ = enabling;
    co2_calib_overlay_mode_ = UiCo2Workflow::nextOverlayMode(
        co2_calib_overlay_mode_,
        visible ? UiCo2Workflow::OverlayTransition::ShowAscProgress
                : UiCo2Workflow::OverlayTransition::Hide);
    update_co2_calib_overlay_ui();
}

void UiController::flush_ui_now() {
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }
    if (objects.overlay_co2_calib_confirm) {
        lv_obj_update_layout(objects.overlay_co2_calib_confirm);
    }
    lv_refr_now(disp);
}

void UiController::set_pressure_altitude_overlay_visible(bool visible) {
    pressure_altitude_overlay_open_ = visible;
    if (visible) {
        reset_pressure_altitude_pending();
    }
    update_sensor_info_ui();
}

void UiController::nudge_pressure_altitude(int delta_m) {
    pressure_altitude_pending_m_ =
        constrain(pressure_altitude_pending_m_ + delta_m,
                  static_cast<int>(Config::PRESSURE_ALTITUDE_MIN_M),
                  static_cast<int>(Config::PRESSURE_ALTITUDE_MAX_M));
    update_pressure_altitude_preview();
}

void UiController::apply_pressure_altitude_setting() {
    storage.config().pressure_altitude_m = pressure_altitude_pending_m_;
    storage.config().pressure_altitude_set = true;
    if (!storage.saveConfig(true)) {
        storage.requestSave();
        LOGE("UI", "failed to persist pressure altitude");
    }
    pressure_altitude_overlay_open_ = false;
    data_dirty = true;
    invalidate_active_graph_refresh_cache();
    update_ui();
    update_sensor_info_ui();
}

void UiController::update_clock_labels() {
    char buf[16];
    char ampm_buf[4] = {};
    tm local_tm = {};
    if (!timeManager.getLocalTime(local_tm)) {
        if (objects.label_time_value_1) safe_label_set_text(objects.label_time_value_1, UiText::TimeMissing());
        if (objects.label_date_value_1) safe_label_set_text(objects.label_date_value_1, UiText::DateMissing());
        if (objects.label_time_value_2) safe_label_set_text(objects.label_time_value_2, UiText::TimeMissing());
        if (objects.label_date_value_2) safe_label_set_text(objects.label_date_value_2, UiText::DateMissing());
        if (objects.label_time_ampm_title_1) safe_label_set_text(objects.label_time_ampm_title_1, "");
        if (objects.label_time_ampm_title_2) safe_label_set_text(objects.label_time_ampm_title_2, "");
        set_label_hidden(objects.label_time_ampm_title_1, true);
        set_label_hidden(objects.label_time_ampm_title_2, true);
        update_date_format_button();
        return;
    }
    format_clock_time_label(local_tm, time_format_24h_, buf, sizeof(buf), ampm_buf, sizeof(ampm_buf));
    if (objects.label_time_value_1) safe_label_set_text(objects.label_time_value_1, buf);
    if (objects.label_time_value_2) safe_label_set_text(objects.label_time_value_2, buf);
    const bool show_ampm = !time_format_24h_;
    if (objects.label_time_ampm_title_1) {
        safe_label_set_text(objects.label_time_ampm_title_1, show_ampm ? ampm_buf : "");
        set_label_hidden(objects.label_time_ampm_title_1, !show_ampm);
    }
    if (objects.label_time_ampm_title_2) {
        safe_label_set_text(objects.label_time_ampm_title_2, show_ampm ? ampm_buf : "");
        set_label_hidden(objects.label_time_ampm_title_2, !show_ampm);
    }
    switch (date_format_) {
        case Config::DateFormat::MDY:
            snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
                     local_tm.tm_mon + 1,
                     local_tm.tm_mday,
                     local_tm.tm_year + 1900);
            break;
        case Config::DateFormat::ISO:
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                     local_tm.tm_year + 1900,
                     local_tm.tm_mon + 1,
                     local_tm.tm_mday);
            break;
        case Config::DateFormat::DMY:
        default:
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
                     local_tm.tm_mday,
                     local_tm.tm_mon + 1,
                     local_tm.tm_year + 1900);
            break;
    }
    if (objects.label_date_value_1) safe_label_set_text(objects.label_date_value_1, buf);
    if (objects.label_date_value_2) safe_label_set_text(objects.label_date_value_2, buf);
    update_date_format_button();
}

void UiController::update_date_format_button() {
    if (!lvgl_ready || !objects.label_btn_date_format) {
        return;
    }
    const char *text = "DMY";
    switch (date_format_) {
        case Config::DateFormat::MDY:
            text = "MDY";
            break;
        case Config::DateFormat::ISO:
            text = "ISO";
            break;
        case Config::DateFormat::DMY:
        default:
            text = "DMY";
            break;
    }
    safe_label_set_text(objects.label_btn_date_format, text);
}

void UiController::set_button_enabled(lv_obj_t *btn, bool enabled) {
    if (!btn) return;
    if (enabled) lv_obj_clear_state(btn, LV_STATE_DISABLED);
    else lv_obj_add_state(btn, LV_STATE_DISABLED);
}

lv_color_t UiController::active_text_color() {
    return themeManager.activeTextColor(night_mode);
}

void UiController::set_night_mode_state(bool enabled, bool save_pref) {
    if (enabled == night_mode) {
        return;
    }
    if (!lvgl_ready) {
        night_mode = enabled;
        if (save_pref) {
            storage.config().night_mode = enabled;
            if (!storage.saveConfig(true)) {
                storage.requestSave();
                LOGE("UI", "failed to persist night mode preference");
            }
        }
        return;
    }

    // Theme/style updates must be serialized with LVGL rendering.
    if (!lvgl_port_lock(UI_LVGL_LOCK_TIMEOUT_MS)) {
        LOGE("UI", "LVGL lock timed out in set_night_mode_state");
        return;
    }
    night_mode = enabled;
    if (enabled) {
        night_mode_on_enter();
    }
    themeManager.applyActive(night_mode, datetime_ui_dirty);
    if (!enabled) {
        night_mode_on_exit();
    }
    data_dirty = true;
    if (!lvgl_port_unlock()) {
        LOGW("UI", "LVGL unlock failed in set_night_mode_state");
    }
    if (save_pref) {
        storage.config().night_mode = enabled;
        if (!storage.saveConfig(true)) {
            storage.requestSave();
            LOGE("UI", "failed to persist night mode preference");
        }
    }
}

void UiController::apply_auto_night_now() {
    bool desired = false;
    if (nightModeManager.applyNow(night_mode, desired)) {
        set_night_mode_state(desired, true);
    }
}

void UiController::sync_night_mode_toggle_ui() {
    if (!lvgl_ready || !objects.btn_night_mode) {
        return;
    }
    set_button_enabled(objects.btn_night_mode, !nightModeManager.isAutoEnabled());
    if (night_mode) {
        lv_obj_add_state(objects.btn_night_mode, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_night_mode, LV_STATE_CHECKED);
    }
}

void UiController::sync_auto_dim_button_state() {
    if (!objects.btn_auto_dim) {
        return;
    }
    if (nightModeManager.isAutoEnabled()) {
        lv_obj_add_state(objects.btn_auto_dim, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_auto_dim, LV_STATE_CHECKED);
    }
}

void UiController::sync_backlight_settings_button_state() {
    if (!objects.btn_head_status_1) {
        return;
    }
    if (backlightManager.isScheduleEnabled()) {
        lv_obj_add_state(objects.btn_head_status_1, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_head_status_1, LV_STATE_CHECKED);
    }
}

void UiController::confirm_show(ConfirmAction action) {
    confirm_action = action;
    if (!objects.container_confirm) {
        return;
    }
    bool show_voc = (action == CONFIRM_VOC_RESET);
    bool show_restart = (action == CONFIRM_RESTART);
    bool show_reset = (action == CONFIRM_FACTORY_RESET);

    set_visible(objects.container_confirm, true);
    set_visible(objects.container_confirm_card, true);
    set_visible(objects.btn_confirm_ok, true);
    set_visible(objects.btn_confirm_cancel, true);
    set_visible(objects.label_btn_confirm_cancel, true);
    lv_obj_add_flag(objects.container_confirm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(objects.container_confirm);

    set_visible(objects.label_btn_confirm_voc, show_voc);
    set_visible(objects.label_confirm_title_voc, show_voc);
    set_visible(objects.container_confirm_voc_text, show_voc);

    set_visible(objects.label_btn_confirm_restart, show_restart);
    set_visible(objects.label_confirm_title_restart, show_restart);
    set_visible(objects.container_confirm_restart_text, show_restart);

    set_visible(objects.label_btn_confirm_reset, show_reset);
    set_visible(objects.label_confirm_title_reset, show_reset);
    set_visible(objects.container_confirm_reset_text, show_reset);
}

void UiController::confirm_hide() {
    confirm_action = CONFIRM_NONE;
    set_visible(objects.container_confirm, false);
}

void UiController::mqtt_apply_pending() {
    MqttPendingCommands pending;
    if (!mqttRuntimeState.takePendingCommands(pending)) {
        return;
    }
    bool publish_needed = false;
    auto persist_dac_auto_state = [&](bool auto_mode, bool auto_armed) -> bool {
        if (storage.config().dac_auto_mode == auto_mode &&
            storage.config().dac_auto_armed == auto_armed) {
            return true;
        }
        if (storage.saveDacAutoState(auto_mode, auto_armed)) {
            return true;
        }
        LOGE("UI", "failed to persist DAC auto state from MQTT");
        return false;
    };
    if (pending.night_mode) {
        bool prev_night = night_mode;
        set_night_mode_state(pending.night_mode_value, true);
        sync_night_mode_toggle_ui();
        if (night_mode != prev_night) {
            publish_needed = true;
        }
    }
    if (pending.alert_blink) {
        if (alert_blink_enabled != pending.alert_blink_value) {
            alert_blink_enabled = pending.alert_blink_value;
            storage.config().alert_blink = alert_blink_enabled;
            if (!storage.saveConfig(true)) {
                storage.requestSave();
                LOGE("UI", "failed to persist alert blink change from MQTT");
            }
            if (alert_blink_enabled) {
                blink_state = true;
                last_blink_ms = millis();
            }
            sync_alert_blink_toggle_state();
            data_dirty = true;
            publish_needed = true;
        }
    }
    if (pending.backlight) {
        bool prev_backlight = backlightManager.isOn();
        backlightManager.setOn(pending.backlight_value);
        if (backlightManager.isOn() != prev_backlight) {
            publish_needed = true;
        }
    }
    FanControl::Mode effective_fan_mode = fanControl.mode();
    if (pending.fan_timer && Config::isDacTimerPresetSeconds(pending.fan_timer_seconds)) {
        fanControl.setTimerSeconds(pending.fan_timer_seconds);
        publish_needed = true;
    }
    if (pending.fan_mode) {
        switch (pending.fan_mode_value) {
            case FanHaMode::Auto:
                if (persist_dac_auto_state(true, true)) {
                    fanControl.requestAutoStart();
                    effective_fan_mode = FanControl::Mode::Auto;
                    publish_needed = true;
                }
                break;
            case FanHaMode::Manual:
                if (persist_dac_auto_state(false, false)) {
                    if (effective_fan_mode != FanControl::Mode::Manual) {
                        fanControl.setMode(FanControl::Mode::Manual);
                        effective_fan_mode = FanControl::Mode::Manual;
                    }
                    fanControl.requestStart();
                    publish_needed = true;
                }
                break;
            case FanHaMode::Stopped:
            default:
                if (persist_dac_auto_state(false, false)) {
                    if (effective_fan_mode != FanControl::Mode::Manual) {
                        fanControl.setMode(FanControl::Mode::Manual);
                        effective_fan_mode = FanControl::Mode::Manual;
                    }
                    fanControl.requestStop();
                    publish_needed = true;
                }
                break;
        }
    }
    if (pending.fan_manual_speed) {
        uint32_t step = pending.fan_manual_speed_value;
        if (step < 1u) {
            step = 1u;
        } else if (step > 10u) {
            step = 10u;
        }
        if (persist_dac_auto_state(false, false)) {
            if (effective_fan_mode != FanControl::Mode::Manual) {
                fanControl.setMode(FanControl::Mode::Manual);
                effective_fan_mode = FanControl::Mode::Manual;
            }
            fanControl.setManualStep(static_cast<uint8_t>(step));
            fanControl.requestStart();
            publish_needed = true;
        }
    }
    if (pending.restart) {
        LOGI("UI", "MQTT restart requested");
        WebHandlersRequestRestart();
    }
    if (publish_needed) {
        mqttRuntimeState.requestPublish();
    }
}

void UiController::sync_alert_blink_toggle_state() {
    if (!objects.btn_alert_blink) {
        return;
    }
    alert_blink_syncing = true;
    if (alert_blink_enabled) {
        lv_obj_add_state(objects.btn_alert_blink, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.btn_alert_blink, LV_STATE_CHECKED);
    }
    alert_blink_syncing = false;
}

void UiController::night_mode_on_enter() {
    alert_blink_before_night = alert_blink_enabled;
    night_blink_restore_pending = true;
    night_blink_user_changed = false;
    if (alert_blink_enabled) {
        alert_blink_enabled = false;
        sync_alert_blink_toggle_state();
    }
}

void UiController::night_mode_on_exit() {
    if (night_blink_restore_pending && !night_blink_user_changed) {
        if (alert_blink_enabled != alert_blink_before_night) {
            alert_blink_enabled = alert_blink_before_night;
            if (alert_blink_enabled) {
                blink_state = true;
                last_blink_ms = millis();
            }
            sync_alert_blink_toggle_state();
        }
    }
    night_blink_restore_pending = false;
    night_blink_user_changed = false;
}

void UiController::sync_wifi_toggle_state() {
    const bool wifi_enabled = connectivity_.wifi_enabled;
    wifi_toggle_syncing_ = true;
    if (objects.btn_wifi) {
        if (wifi_enabled) lv_obj_add_state(objects.btn_wifi, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_wifi, LV_STATE_CHECKED);
    }
    if (objects.btn_wifi_toggle) {
        if (wifi_enabled) lv_obj_add_state(objects.btn_wifi_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_wifi_toggle, LV_STATE_CHECKED);
    }
    wifi_toggle_syncing_ = false;
}

void UiController::sync_mqtt_toggle_state() {
    mqtt_toggle_syncing_ = true;
    if (objects.btn_mqtt) {
        if (connectivity_.mqtt_enabled) lv_obj_add_state(objects.btn_mqtt, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_mqtt, LV_STATE_CHECKED);
    }
    if (objects.btn_mqtt_toggle) {
        if (connectivity_.mqtt_enabled) lv_obj_add_state(objects.btn_mqtt_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_mqtt_toggle, LV_STATE_CHECKED);
    }
    mqtt_toggle_syncing_ = false;
}

void UiController::update_temp_offset_label() {
    if (!objects.label_temp_offset_value) {
        return;
    }
    float val = temp_offset;
    if (!temp_units_c) {
        // Offset is stored in C; show equivalent offset in F when F units are active.
        val = val * 9.0f / 5.0f;
    }
    val = lroundf(val * 10.0f) / 10.0f;
    if (fabsf(val) < 0.05f) {
        val = 0.0f;
    }
    char buf[16];
    if (val > 0.0f) {
        snprintf(buf, sizeof(buf), "+%.1f", val);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", val);
    }
    safe_label_set_text(objects.label_temp_offset_value, buf);
    sync_back_button_label(objects.label_btn_back, settings_has_unsaved_changes());
}

void UiController::update_hum_offset_label() {
    if (!objects.label_hum_offset_value) {
        return;
    }
    float val = hum_offset;
    if (fabsf(val) < 0.5f) {
        val = 0.0f;
    }
    char buf[16];
    if (val > 0.0f) {
        snprintf(buf, sizeof(buf), "+%.0f%%", val);
    } else if (val < 0.0f) {
        snprintf(buf, sizeof(buf), "%.0f%%", val);
    } else {
        strcpy(buf, UiText::ValueZeroPercent());
    }
    safe_label_set_text(objects.label_hum_offset_value, buf);
    sync_back_button_label(objects.label_btn_back, settings_has_unsaved_changes());
}

void UiController::update_led_indicators() {
    const bool visible = led_indicators_enabled;
    auto set_indicator_visible = [visible](lv_obj_t *obj) {
        if (!obj) {
            return;
        }
        visible ? lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN)
                : lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };

    set_indicator_visible(objects.dot_co2_1);
    set_indicator_visible(objects.co2_marker_1);
    set_indicator_visible(objects.dot_temp_1);
    set_indicator_visible(objects.dot_hum_1);
    set_indicator_visible(objects.dot_dp_1);
    set_indicator_visible(objects.dot_ah_1);
    set_indicator_visible(objects.dot_pm25_1);
    set_indicator_visible(objects.dot_pm05);
    set_indicator_visible(objects.dot_pm10_pro);
    set_indicator_visible(objects.dot_pm1);
    set_indicator_visible(objects.dot_voc_1);
    set_indicator_visible(objects.dot_voc_2);
    set_indicator_visible(objects.dot_nox_1);
    set_indicator_visible(objects.dot_nox_2);
    set_indicator_visible(objects.dot_hcho_1);
    set_indicator_visible(objects.dot_co);
    set_indicator_visible(objects.dot_mr);
    set_indicator_visible(objects.dot_sensor_info);
}

void UiController::set_chip_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;
    lv_obj_set_style_border_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (color.full == color_inactive().full) {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::update_co2_bar(int co2, bool valid) {
    if (!objects.co2_bar_fill_1 || !objects.co2_marker_1) {
        return;
    }
    if (!valid) {
        if (objects.co2_bar_mask_1) {
            lv_obj_set_width(objects.co2_bar_mask_1, 0);
        } else {
            lv_obj_set_width(objects.co2_bar_fill_1, 0);
        }
        lv_obj_set_x(objects.co2_marker_1, 2);
        return;
    }

    int bar_max = 330;
    int fill_w = lv_obj_get_width(objects.co2_bar_fill_1);
    if (fill_w > 0) {
        bar_max = fill_w;
    }
    int clamped = constrain(co2, 400, 2000);
    int w = map(clamped, 400, 2000, 0, bar_max);
    w = constrain(w, 0, bar_max);
    if (objects.co2_bar_mask_1) {
        lv_obj_set_width(objects.co2_bar_mask_1, w);
    } else {
        lv_obj_set_width(objects.co2_bar_fill_1, w);
    }

    const int marker_w = 14;
    int center = 4 + w;
    int x = center - (marker_w / 2);
    int track_w = objects.co2_bar_track_1 ? lv_obj_get_width(objects.co2_bar_track_1) : 0;
    int max_x = (track_w > 0) ? (track_w - marker_w - 2) : (340 - marker_w - 2);
    x = constrain(x, 2, max_x);
    lv_obj_set_x(objects.co2_marker_1, x);
}

void UiController::sync_co2_marker_border() {
    if (!objects.co2_marker_1) {
        return;
    }
    lv_obj_set_style_border_color(objects.co2_marker_1,
                                  color_card_background(),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
}

void UiController::update_ui() {
    AirQuality aq = getAirQuality(currentData);
    bool gas_warmup = sensorManager.isWarmupActive();
    bool show_co2_bar = !night_mode;
    const uint32_t now_ms = millis();
    update_status_message(now_ms, gas_warmup);
    lv_color_t header_col;
    lv_opa_t header_shadow;
    compute_header_style(aq, status_max_severity, co_status_alert_active, header_col, header_shadow);
    if (night_mode && header_status_enabled) {
        const bool status_red =
            status_max_severity >= static_cast<uint8_t>(StatusMessages::STATUS_RED);
        header_col = (co_status_alert_active || status_red) ? color_red() : color_inactive();
        header_shadow = (header_col.full == color_red().full) ? LV_OPA_COVER : LV_OPA_TRANSP;
    }
    if (objects.container_header_pro) {
        lv_obj_set_style_border_color(objects.container_header_pro, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(objects.container_header_pro, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(objects.container_header_pro, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.container_settings_header) {
        lv_obj_set_style_border_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(objects.container_settings_header, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    update_main_screen_background_alert();
    update_sensor_cards(aq, gas_warmup, show_co2_bar);
}

void UiController::update_settings_header() {
    if (!objects.container_settings_header) {
        return;
    }
    sync_back_button_label(objects.label_btn_back, settings_has_unsaved_changes());
    AirQuality aq = getAirQuality(currentData);
    bool co_alert_active = false;
    uint8_t status_severity = static_cast<uint8_t>(StatusMessages::STATUS_NONE);
    compute_status_summary(sensorManager.isWarmupActive(), co_alert_active, status_severity);
    lv_color_t header_col;
    lv_opa_t header_shadow;
    compute_header_style(aq, status_severity, co_alert_active, header_col, header_shadow);
    if (night_mode && header_status_enabled) {
        const bool status_red =
            status_severity >= static_cast<uint8_t>(StatusMessages::STATUS_RED);
        header_col = (co_alert_active || status_red) ? color_red() : color_inactive();
        header_shadow = (header_col.full == color_red().full) ? LV_OPA_COVER : LV_OPA_TRANSP;
    }
    lv_obj_set_style_border_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(objects.container_settings_header, header_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(objects.container_settings_header, header_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (objects.label_log_status || objects.log_status) {
        const SettingsLogSeverity log_severity = summarize_settings_log_severity(diag_ack_alert_seq_);
        const char *log_label = UiText::StatusOk();
        lv_color_t log_color = color_green();
        if (log_severity == SettingsLogSeverity::Error) {
            log_label = UiText::StatusErr();
            log_color = color_red();
        } else if (log_severity == SettingsLogSeverity::Warn) {
            log_label = UiText::StatusWarn();
            log_color = color_yellow();
        }
        if (objects.label_log_status) {
            safe_label_set_text(objects.label_log_status, log_label);
        }
        if (objects.log_status) {
            set_chip_color(objects.log_status, log_color);
        }
    }
    sync_night_mode_toggle_ui();
    sync_auto_dim_button_state();
    sync_backlight_settings_button_state();
}

void UiController::update_theme_custom_info(bool presets) {
    refreshConnectivitySnapshot();
    set_visible(objects.container_theme_custom_info, !presets);
    if (!presets && objects.qrcode_theme_custom) {
        const bool sta_mode = connectivity_.wifi_enabled &&
                              connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED);
        String theme_url = sta_mode ? connectivity_.theme_sta_url : connectivity_.theme_local_url;
        update_qrcode_if_needed(objects.qrcode_theme_custom,
                                theme_url.c_str(),
                                theme_custom_qr_cache_,
                                sizeof(theme_custom_qr_cache_));
    }
}

void UiController::compute_status_summary(bool gas_warmup,
                                          bool &co_alert_active,
                                          uint8_t &max_severity) const {
    co_alert_active = false;
    max_severity = static_cast<uint8_t>(StatusMessages::STATUS_NONE);

    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(currentData, gas_warmup, thresholds);
    for (size_t i = 0; i < result.count; ++i) {
        const StatusMessages::StatusMessage &msg = result.messages[i];
        if (msg.sensor == StatusMessages::STATUS_SENSOR_CO) {
            co_alert_active = true;
        }
        const uint8_t sev = static_cast<uint8_t>(msg.severity);
        if (sev > max_severity) {
            max_severity = sev;
        }
    }
}

void UiController::update_status_message(uint32_t now_ms, bool gas_warmup) {
    const DisplayThresholds::Config thresholds = displayThresholds.snapshot();
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(currentData, gas_warmup, thresholds);
    const StatusMessages::StatusMessage *messages = result.messages;
    const size_t count = result.count;
    const bool has_valid = result.has_valid;
    co_status_alert_active = false;
    status_max_severity = static_cast<uint8_t>(StatusMessages::STATUS_NONE);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t sev = static_cast<uint8_t>(messages[i].severity);
        if (sev > status_max_severity) {
            status_max_severity = sev;
        }
        if (messages[i].sensor == StatusMessages::STATUS_SENSOR_CO) {
            co_status_alert_active = true;
        }
    }

    uint32_t signature = static_cast<uint32_t>(count);
    for (size_t i = 0; i < count; ++i) {
        signature = signature * 131u + (static_cast<uint32_t>(messages[i].sensor) << 2) +
                    static_cast<uint32_t>(messages[i].severity);
    }
    if (signature != status_msg_signature) {
        status_msg_signature = signature;
        status_msg_index = 0;
        status_msg_last_ms = now_ms;
    }

    status_msg_count = static_cast<uint8_t>(count);

    if (count > 1 && (now_ms - status_msg_last_ms) >= STATUS_ROTATE_MS) {
        status_msg_index = static_cast<uint8_t>((status_msg_index + 1) % count);
        status_msg_last_ms = now_ms;
    }
    if (status_msg_index >= count) {
        status_msg_index = 0;
    }

    const char *status_text = nullptr;
    if (!has_valid) {
        status_text = UiText::StatusInitializing();
    } else if (count == 0) {
        status_text = UiText::StatusAllGood();
    } else {
        status_text = messages[status_msg_index].text;
    }

    if (objects.label_status_value_1) {
        safe_label_set_text(objects.label_status_value_1, status_text ? status_text : UiText::ValueMissing());
    }
}

void UiController::init_ui_defaults() {
    if (objects.co2_bar_mask_1) {
        lv_obj_clear_flag(objects.co2_bar_mask_1, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_clear_flag(objects.co2_bar_mask_1, LV_OBJ_FLAG_SCROLLABLE);
    }

    set_visible(objects.container_about, false);
    set_visible(objects.container_web_page, false);
    set_visible(objects.container_rtc_detection, false);
    clear_co2_calib_overlay_timer();
    co2_calib_overlay_mode_ = Co2CalibOverlayMode::Hidden;
    update_co2_calib_overlay_ui();

    if (objects.btn_info_graph) {
        lv_obj_add_flag(objects.btn_info_graph, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_info_graph, 18);
    }
    if (objects.btn_temp_range_1h) {
        lv_obj_add_flag(objects.btn_temp_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_1h, 12);
    }
    if (objects.btn_temp_range_3h) {
        lv_obj_add_flag(objects.btn_temp_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_3h, 12);
    }
    if (objects.btn_temp_range_24h) {
        lv_obj_add_flag(objects.btn_temp_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_temp_range_24h, 12);
    }
    if (objects.btn_voc_range_1h) {
        lv_obj_add_flag(objects.btn_voc_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_1h, 12);
    }
    if (objects.btn_voc_range_3h) {
        lv_obj_add_flag(objects.btn_voc_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_3h, 12);
    }
    if (objects.btn_voc_range_24h) {
        lv_obj_add_flag(objects.btn_voc_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_voc_range_24h, 12);
    }
    if (objects.btn_nox_range_1h) {
        lv_obj_add_flag(objects.btn_nox_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_1h, 12);
    }
    if (objects.btn_nox_range_3h) {
        lv_obj_add_flag(objects.btn_nox_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_3h, 12);
    }
    if (objects.btn_nox_range_24h) {
        lv_obj_add_flag(objects.btn_nox_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_nox_range_24h, 12);
    }
    if (objects.btn_hcho_range_1h) {
        lv_obj_add_flag(objects.btn_hcho_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_1h, 12);
    }
    if (objects.btn_hcho_range_3h) {
        lv_obj_add_flag(objects.btn_hcho_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_3h, 12);
    }
    if (objects.btn_hcho_range_24h) {
        lv_obj_add_flag(objects.btn_hcho_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_hcho_range_24h, 12);
    }
    if (objects.btn_pm05_range_1h) {
        lv_obj_add_flag(objects.btn_pm05_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_1h, 12);
    }
    if (objects.btn_pm05_range_3h) {
        lv_obj_add_flag(objects.btn_pm05_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_3h, 12);
    }
    if (objects.btn_pm05_range_24h) {
        lv_obj_add_flag(objects.btn_pm05_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm05_range_24h, 12);
    }
    if (objects.btn_pm25_4_range_1h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_1h, 12);
    }
    if (objects.btn_pm25_4_range_3h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_3h, 12);
    }
    if (objects.btn_pm25_4_range_24h) {
        lv_obj_add_flag(objects.btn_pm25_4_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm25_4_range_24h, 12);
    }
    if (objects.btn_pm1_10_range_1h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_1h, 12);
    }
    if (objects.btn_pm1_10_range_3h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_3h, 12);
    }
    if (objects.btn_pm1_10_range_24h) {
        lv_obj_add_flag(objects.btn_pm1_10_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_pm1_10_range_24h, 12);
    }
    if (objects.btn_co_range_1h) {
        lv_obj_add_flag(objects.btn_co_range_1h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_1h, 12);
    }
    if (objects.btn_co_range_3h) {
        lv_obj_add_flag(objects.btn_co_range_3h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_3h, 12);
    }
    if (objects.btn_co_range_24h) {
        lv_obj_add_flag(objects.btn_co_range_24h, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.btn_co_range_24h, 12);
    }
    if (objects.btn_wifi_reconnect) {
        // Action button: keep visual CHECKED feedback fully controlled from code.
        lv_obj_clear_flag(objects.btn_wifi_reconnect, LV_OBJ_FLAG_CHECKABLE);
    }
    if (objects.btn_wifi_start_ap) {
        // AP state indicator is managed manually in update_wifi_ui().
        lv_obj_clear_flag(objects.btn_wifi_start_ap, LV_OBJ_FLAG_CHECKABLE);
    }
    if (objects.chip_rtc_status) {
        lv_obj_add_flag(objects.chip_rtc_status, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(objects.chip_rtc_status, 12);
    }
    if (objects.chip_rtc_detection_auto) {
        lv_obj_add_flag(objects.chip_rtc_detection_auto, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.chip_rtc_detection_auto, 12);
    }
    if (objects.chip_rtc_detection_pcf8523) {
        lv_obj_add_flag(objects.chip_rtc_detection_pcf8523, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.chip_rtc_detection_pcf8523, 12);
    }
    if (objects.chip_rtc_detection_ds3231) {
        lv_obj_add_flag(objects.chip_rtc_detection_ds3231, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_ext_click_area(objects.chip_rtc_detection_ds3231, 12);
    }

    if (objects.wifi_status_icon_1) lv_obj_add_flag(objects.wifi_status_icon_1, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_2) lv_obj_add_flag(objects.wifi_status_icon_2, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_3) lv_obj_add_flag(objects.wifi_status_icon_3, LV_OBJ_FLAG_HIDDEN);
    if (objects.wifi_status_icon_4) lv_obj_add_flag(objects.wifi_status_icon_4, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_1) lv_obj_add_flag(objects.mqtt_status_icon_1, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_2) lv_obj_add_flag(objects.mqtt_status_icon_2, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_3) lv_obj_add_flag(objects.mqtt_status_icon_3, LV_OBJ_FLAG_HIDDEN);
    if (objects.mqtt_status_icon_4) lv_obj_add_flag(objects.mqtt_status_icon_4, LV_OBJ_FLAG_HIDDEN);

    if (objects.btn_mqtt) {
        lv_obj_set_style_bg_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.label_btn_mqtt) {
        lv_obj_set_style_text_color(objects.label_btn_mqtt, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.btn_wifi_reconnect) {
        lv_obj_set_style_bg_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.label_btn_wifi_reconnect) {
        lv_obj_set_style_text_color(objects.label_btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.label_btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.btn_wifi_start_ap) {
        lv_obj_set_style_bg_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.label_btn_wifi_start_ap) {
        lv_obj_set_style_text_color(objects.label_btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.label_btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.btn_dac_settings) {
        lv_obj_set_style_bg_color(objects.btn_dac_settings, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_dac_settings, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_dac_settings, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.label_dac_settings) {
        lv_obj_set_style_text_color(objects.label_dac_settings, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.btn_night_mode) {
        lv_obj_set_style_bg_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    if (objects.label_btn_night_mode) {
        lv_obj_set_style_text_color(objects.label_btn_night_mode, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
    }
    ui_language = storage.config().language;
    date_format_ = storage.config().date_format;
    time_format_24h_ = storage.config().time_format_24h;
    rtc_detection_saved_mode_ = storage.config().rtc_mode;
    rtc_detection_pending_mode_ = rtc_detection_saved_mode_;
    reset_pressure_altitude_pending();
    pressure_altitude_overlay_open_ = false;
    language_dirty = false;
    header_status_enabled = storage.config().header_status_enabled;
    UiLocalization::applyCurrentLanguage(*this);

    update_clock_labels();
    timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    update_datetime_ui();
    backlightManager.updateUi();
    nightModeManager.updateUi();
    sync_backlight_settings_button_state();
    update_led_indicators();
    update_temp_offset_label();
    update_hum_offset_label();
    update_wifi_ui();
    update_mqtt_ui();
    update_dac_ui(millis());
    update_ui();
    confirm_hide();
}

