// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

class AuraNetworkManager;
class ConnectivityRuntime;
class MqttManager;

class NetworkCommandQueue {
public:
    enum class Type : uint8_t {
        ApplyPendingWifiEnabled = 0,
        SetWifiEnabled,
        SetMqttUserEnabled,
        SyncMqttWithWifi,
        RequestMqttReconnect,
        RequestWifiReconnect,
        RequestWifiScanStart,
        RequestWifiScanStop,
        ToggleWifiApMode,
        ClearWifiCredentials,
    };

    struct Command {
        Type type = Type::ApplyPendingWifiEnabled;
        bool bool_value = false;
    };

    struct WifiSettingsUpdate {
        String ssid;
        String pass;
        bool enabled = true;
    };

    struct MqttSettingsUpdate {
        String host;
        uint16_t port = 1883;
        String user;
        String pass;
        String base_topic;
        String device_name;
        bool discovery = true;
        bool anonymous = false;
    };

    NetworkCommandQueue();

    bool enqueue(Type type, bool bool_value = false);
    bool publishSavedWifiSettings(const WifiSettingsUpdate &update);
    bool publishSavedMqttSettings(const MqttSettingsUpdate &update);
    bool publishWifiScanRequest(Type type);
    bool tryDequeue(Command &out);
    void processAll(AuraNetworkManager &networkManager,
                    MqttManager &mqttManager,
                    ConnectivityRuntime &connectivityRuntime);

private:
    void lock() const;
    void unlock() const;
    bool takePendingWifiScanRequest(Type &out);
    bool takePendingWifiSettingsUpdate(WifiSettingsUpdate &out);
    bool takePendingMqttSettingsUpdate(MqttSettingsUpdate &out);

    static constexpr uint8_t kCapacity = 16;

    StaticQueue_t queue_storage_{};
    uint8_t queue_buffer_[kCapacity * sizeof(Command)] = {};
    QueueHandle_t queue_ = nullptr;
    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    Type pending_wifi_scan_request_ = Type::RequestWifiScanStart;
    bool pending_wifi_scan_request_valid_ = false;
    WifiSettingsUpdate pending_wifi_settings_update_{};
    bool pending_wifi_settings_valid_ = false;
    MqttSettingsUpdate pending_mqtt_settings_update_{};
    bool pending_mqtt_settings_valid_ = false;
};
