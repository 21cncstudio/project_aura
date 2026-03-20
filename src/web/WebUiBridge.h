// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config/AppData.h"
#include "modules/DacAutoConfig.h"

class WebUiBridge {
public:
    enum class DispatchMode : uint8_t {
        DirectCallback = 0,
        DeferredReply,
    };

    struct Snapshot {
        bool available = false;
        bool night_mode = false;
        bool night_mode_locked = false;
        bool backlight_on = false;
        bool units_c = true;
        float temp_offset = 0.0f;
        float hum_offset = 0.0f;
        String display_name;
        bool mqtt_screen_open = false;
        bool theme_screen_open = false;
        bool theme_custom_screen_open = false;
        ThemeColors theme_preview_colors{};
    };

    struct SettingsUpdate {
        bool has_night_mode = false;
        bool night_mode = false;
        bool has_backlight = false;
        bool backlight_on = false;
        bool has_units_c = false;
        bool units_c = true;
        bool has_temp_offset = false;
        float temp_offset = 0.0f;
        bool has_hum_offset = false;
        float hum_offset = 0.0f;
        bool has_display_name = false;
        String display_name;
        bool restart_requested = false;
    };

    struct ApplyResult {
        bool success = false;
        uint16_t status_code = 503;
        String error_message;
        bool restart_requested = false;
        Snapshot snapshot;
    };

    struct ThemeUpdate {
        ThemeColors colors{};
    };

    struct DacActionUpdate {
        enum class Type : uint8_t {
            SetMode = 0,
            SetManualStep,
            SetTimerSeconds,
            Start,
            Stop,
            StartAuto,
        };

        Type type = Type::Start;
        bool auto_mode = false;
        uint8_t manual_step = 1;
        uint32_t timer_seconds = 0;
    };

    struct DacAutoUpdate {
        DacAutoConfig config{};
        bool rearm = false;
    };

    struct WifiSaveUpdate {
        String ssid;
        String pass;
        bool enabled = true;
    };

    struct MqttSaveUpdate {
        String host;
        uint16_t port = 1883;
        String user;
        String pass;
        String base_topic;
        String device_name;
        bool discovery = true;
        bool anonymous = false;
    };

    using SettingsApplyFn = ApplyResult (*)(const SettingsUpdate &update, void *ctx);
    using ThemeApplyFn = ApplyResult (*)(const ThemeUpdate &update, void *ctx);
    using DacActionApplyFn = ApplyResult (*)(const DacActionUpdate &update, void *ctx);
    using DacAutoApplyFn = ApplyResult (*)(const DacAutoUpdate &update, void *ctx);
    using WifiSaveApplyFn = ApplyResult (*)(const WifiSaveUpdate &update, void *ctx);
    using MqttSaveApplyFn = ApplyResult (*)(const MqttSaveUpdate &update, void *ctx);

    WebUiBridge();

    void bindSettingsApplier(void *ctx, SettingsApplyFn fn);
    void bindThemeApplier(void *ctx, ThemeApplyFn fn);
    void bindDacActionApplier(void *ctx, DacActionApplyFn fn);
    void bindDacAutoApplier(void *ctx, DacAutoApplyFn fn);
    void bindWifiSaveApplier(void *ctx, WifiSaveApplyFn fn);
    void bindMqttSaveApplier(void *ctx, MqttSaveApplyFn fn);
    void setDispatchMode(DispatchMode mode);
    void publishSnapshot(const Snapshot &snapshot);
    Snapshot snapshot() const;
    bool isAvailable() const;
    ApplyResult applySettings(const SettingsUpdate &update);
    ApplyResult applyTheme(const ThemeUpdate &update);
    ApplyResult applyDacAction(const DacActionUpdate &update);
    ApplyResult applyDacAuto(const DacAutoUpdate &update);
    ApplyResult applyWifiSave(const WifiSaveUpdate &update);
    ApplyResult applyMqttSave(const MqttSaveUpdate &update);
    bool consumePendingSettingsRequest(SettingsUpdate &update, uint32_t &request_id);
    void completePendingSettingsRequest(uint32_t request_id, const ApplyResult &result);
    bool consumePendingThemeRequest(ThemeUpdate &update, uint32_t &request_id);
    void completePendingThemeRequest(uint32_t request_id, const ApplyResult &result);
    bool consumePendingDacActionRequest(DacActionUpdate &update, uint32_t &request_id);
    void completePendingDacActionRequest(uint32_t request_id, const ApplyResult &result);
    bool consumePendingDacAutoRequest(DacAutoUpdate &update, uint32_t &request_id);
    void completePendingDacAutoRequest(uint32_t request_id, const ApplyResult &result);
    bool consumePendingWifiSaveRequest(WifiSaveUpdate &update, uint32_t &request_id);
    void completePendingWifiSaveRequest(uint32_t request_id, const ApplyResult &result);
    bool consumePendingMqttSaveRequest(MqttSaveUpdate &update, uint32_t &request_id);
    void completePendingMqttSaveRequest(uint32_t request_id, const ApplyResult &result);

    void requestFirmwareUpdateScreen(bool active);
    bool consumePendingFirmwareUpdateScreen(bool &active);
    void setMqttScreenOpen(bool open);
    void setThemeScreenOpen(bool open, bool custom_open);

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    StaticSemaphore_t settings_reply_semaphore_buffer_{};
    SemaphoreHandle_t settings_reply_semaphore_ = nullptr;
    StaticSemaphore_t theme_reply_semaphore_buffer_{};
    SemaphoreHandle_t theme_reply_semaphore_ = nullptr;
    StaticSemaphore_t dac_action_reply_semaphore_buffer_{};
    SemaphoreHandle_t dac_action_reply_semaphore_ = nullptr;
    StaticSemaphore_t dac_auto_reply_semaphore_buffer_{};
    SemaphoreHandle_t dac_auto_reply_semaphore_ = nullptr;
    StaticSemaphore_t wifi_save_reply_semaphore_buffer_{};
    SemaphoreHandle_t wifi_save_reply_semaphore_ = nullptr;
    StaticSemaphore_t mqtt_save_reply_semaphore_buffer_{};
    SemaphoreHandle_t mqtt_save_reply_semaphore_ = nullptr;
    Snapshot snapshot_{};
    DispatchMode dispatch_mode_ = DispatchMode::DirectCallback;
    void *settings_ctx_ = nullptr;
    SettingsApplyFn settings_apply_fn_ = nullptr;
    void *theme_ctx_ = nullptr;
    ThemeApplyFn theme_apply_fn_ = nullptr;
    void *dac_action_ctx_ = nullptr;
    DacActionApplyFn dac_action_apply_fn_ = nullptr;
    void *dac_auto_ctx_ = nullptr;
    DacAutoApplyFn dac_auto_apply_fn_ = nullptr;
    void *wifi_save_ctx_ = nullptr;
    WifiSaveApplyFn wifi_save_apply_fn_ = nullptr;
    void *mqtt_save_ctx_ = nullptr;
    MqttSaveApplyFn mqtt_save_apply_fn_ = nullptr;
    SettingsUpdate pending_settings_update_{};
    uint32_t pending_settings_request_id_ = 0;
    bool pending_settings_request_ = false;
    ApplyResult pending_settings_result_{};
    uint32_t pending_settings_result_id_ = 0;
    ThemeUpdate pending_theme_update_{};
    uint32_t pending_theme_request_id_ = 0;
    bool pending_theme_request_ = false;
    ApplyResult pending_theme_result_{};
    uint32_t pending_theme_result_id_ = 0;
    DacActionUpdate pending_dac_action_update_{};
    uint32_t pending_dac_action_request_id_ = 0;
    bool pending_dac_action_request_ = false;
    ApplyResult pending_dac_action_result_{};
    uint32_t pending_dac_action_result_id_ = 0;
    DacAutoUpdate pending_dac_auto_update_{};
    uint32_t pending_dac_auto_request_id_ = 0;
    bool pending_dac_auto_request_ = false;
    ApplyResult pending_dac_auto_result_{};
    uint32_t pending_dac_auto_result_id_ = 0;
    WifiSaveUpdate pending_wifi_save_update_{};
    uint32_t pending_wifi_save_request_id_ = 0;
    bool pending_wifi_save_request_ = false;
    ApplyResult pending_wifi_save_result_{};
    uint32_t pending_wifi_save_result_id_ = 0;
    MqttSaveUpdate pending_mqtt_save_update_{};
    uint32_t pending_mqtt_save_request_id_ = 0;
    bool pending_mqtt_save_request_ = false;
    ApplyResult pending_mqtt_save_result_{};
    uint32_t pending_mqtt_save_result_id_ = 0;
    bool firmware_update_screen_pending_ = false;
    bool firmware_update_screen_active_ = false;
};
