// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config/AppConfig.h"

class AuraNetworkManager;
class MqttManager;

struct ConnectivityRuntimeSnapshot {
    bool wifi_enabled = false;
    bool wifi_connected = false;
    bool wifi_ap_mode = false;
    bool wifi_scan_in_progress = false;
    int wifi_state = 0;
    int wifi_sta_status = 0;
    uint8_t wifi_retry_count = 0;
    String wifi_ssid;
    String ap_ssid;
    String wifi_scan_options;
    String hostname;
    String sta_ip;
    String ap_ip;
    bool has_rssi = false;
    int rssi = 0;
    String dashboard_local_url;
    String dashboard_sta_url;
    String mqtt_local_url;
    String mqtt_sta_url;
    String theme_local_url;
    String theme_sta_url;
    String dac_local_url;
    String dac_sta_url;
    bool mqtt_user_enabled = false;
    bool mqtt_enabled = false;
    bool mqtt_connected = false;
    uint8_t mqtt_retry_stage = 0;
    String mqtt_host;
    uint16_t mqtt_port = Config::MQTT_DEFAULT_PORT;
    String mqtt_user;
    String mqtt_pass;
    String mqtt_device_name;
    String mqtt_device_id;
    String mqtt_base_topic;
    bool mqtt_discovery = false;
    bool mqtt_anonymous = false;
};

class ConnectivityRuntime {
public:
    ConnectivityRuntime();
    void update(AuraNetworkManager &networkManager, MqttManager &mqttManager);
    ConnectivityRuntimeSnapshot snapshot() const;

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    ConnectivityRuntimeSnapshot snapshot_;
};
