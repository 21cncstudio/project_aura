// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config/AppConfig.h"
#include "config/AppData.h"

class StorageManager;
class AuraNetworkManager;

class MqttManager {
public:
    struct PendingCommands {
        bool night_mode = false;
        bool night_mode_value = false;
        bool alert_blink = false;
        bool alert_blink_value = false;
        bool backlight = false;
        bool backlight_value = false;
        bool restart = false;
    };

    MqttManager();

    void begin(StorageManager &storage, AuraNetworkManager &network);
    void poll(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on);

    void syncWithWifi();
    void requestReconnect();
    void requestPublish();
    void setUserEnabled(bool enabled);
    void updateNightModeAvailability(bool auto_night_enabled);

    bool isUserEnabled() const { return mqtt_user_enabled_; }
    bool isEnabled() const { return mqtt_enabled_; }
    bool isConnected() { return client_.connected(); }
    uint8_t connectFailCount() const { return mqtt_connect_fail_count_; }
    uint32_t connectAttempts() const { return mqtt_connect_attempts_; }
    bool retryExhausted() const { return mqtt_retry_exhausted_; }
    bool isUiDirty() const { return ui_dirty_; }
    void clearUiDirty() { ui_dirty_ = false; }
    void markUiDirty() { ui_dirty_ = true; }

    const String &host() const { return mqtt_host_; }
    uint16_t port() const { return mqtt_port_; }
    const String &baseTopic() const { return mqtt_base_topic_; }
    const String &deviceName() const { return mqtt_device_name_; }
    const String &deviceId() const { return mqtt_device_id_; }

    bool takePending(PendingCommands &out);

    PubSubClient &client() { return client_; }
    bool &userEnabledRef() { return mqtt_user_enabled_; }
    uint8_t &connectFailCountRef() { return mqtt_connect_fail_count_; }
    String &hostRef() { return mqtt_host_; }
    uint16_t &portRef() { return mqtt_port_; }
    String &userRef() { return mqtt_user_; }
    String &passRef() { return mqtt_pass_; }
    String &baseTopicRef() { return mqtt_base_topic_; }
    String &deviceNameRef() { return mqtt_device_name_; }
    String &deviceIdRef() { return mqtt_device_id_; }
    bool &discoveryRef() { return mqtt_discovery_; }
    bool &anonymousRef() { return mqtt_anonymous_; }

private:
    void loadPrefs();
    void initDeviceId();
    void setupClient();
    bool connectClient(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on);
    void publishDiscovery();
    void publishDiscoverySensor(const char *object_id, const char *name,
                                const char *unit, const char *device_class,
                                const char *state_class, const char *value_template,
                                const char *icon);
    void publishDiscoverySwitch(const char *object_id, const char *name,
                                const char *value_template, const char *icon);
    void publishDiscoveryButton(const char *object_id, const char *name,
                                const char *payload_press, const char *icon);
    void publishNightModeAvailability();
    void publishState(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on);

    String stateTopic() const;
    String availabilityTopic() const;
    String nightModeAvailabilityTopic() const;
    String discoveryTopic(const char *object_id) const;
    String commandTopic(const char *command) const;

    void handleCallback(char *topic, uint8_t *payload, unsigned int length);
    static void staticCallback(char *topic, uint8_t *payload, unsigned int length);
    static float computeDewPointC(float temp_c, float rh);
    static bool payloadIsOn(const String &payload);
    static bool payloadIsOff(const String &payload);

    StorageManager *storage_ = nullptr;
    AuraNetworkManager *network_ = nullptr;
    WiFiClient net_;
    PubSubClient client_;
    bool ui_dirty_ = false;

    String mqtt_host_;
    uint16_t mqtt_port_ = Config::MQTT_DEFAULT_PORT;
    String mqtt_user_;
    String mqtt_pass_;
    String mqtt_base_topic_;
    String mqtt_device_name_;
    String mqtt_device_id_;
    bool mqtt_user_enabled_ = true;
    bool mqtt_enabled_ = true;
    bool mqtt_discovery_ = true;
    bool mqtt_anonymous_ = false;
    bool mqtt_discovery_sent_ = false;
    uint32_t mqtt_last_attempt_ms_ = 0;
    uint32_t mqtt_last_publish_ms_ = 0;
    bool mqtt_publish_requested_ = false;
    bool mqtt_connected_last_ = false;
    uint8_t mqtt_fail_count_ = 0;
    uint8_t mqtt_connect_fail_count_ = 0;
    uint32_t mqtt_connect_attempts_ = 0;
    bool mqtt_retry_exhausted_ = false;
    bool auto_night_enabled_ = false;
    PendingCommands pending_;
};
