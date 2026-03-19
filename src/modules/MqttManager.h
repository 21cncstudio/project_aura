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
    void serviceConnectedLoop();

    bool isUserEnabled() const { return mqtt_user_enabled_; }
    bool isEnabled() const { return mqtt_enabled_; }
    bool isConnected() { return client_.connected(); }
    uint32_t connectAttempts() const { return mqtt_connect_attempts_; }
    uint8_t retryStage() const;
    uint32_t retryDelayMs() const;
    static uint8_t retryStageForAttempts(uint32_t failed_attempts);
    static uint32_t retryDelayMsForAttempts(uint32_t failed_attempts);
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
    String &hostRef() { return mqtt_host_; }
    uint16_t &portRef() { return mqtt_port_; }
    String &userRef() { return mqtt_user_; }
    String &passRef() { return mqtt_pass_; }
    String &baseTopicRef() { return mqtt_base_topic_; }
    String &deviceNameRef() { return mqtt_device_name_; }
    String &deviceIdRef() { return mqtt_device_id_; }
    bool &discoveryRef() { return mqtt_discovery_; }
    bool &anonymousRef() { return mqtt_anonymous_; }

    // [NEW] Publishes an air quality band-change event to "<base_topic>/events".
    // Payload includes event_type, a human-readable message, the numeric value
    // and unit so HA displays full context — not just the event type key.
    // retain=false — events are transient signals, not persistent state.
    void publishEvent(const char *metric_key, const char *band,
                      float value, const char *unit, const char *message);

    // [NEW] Publishes a system log event to "<base_topic>/events".
    // Mirrors the entries shown in the web dashboard Events tab (SENSORS,
    // STORAGE, TIME, UI, SEN66, …) so HA sees the same system events.
    // event_type is always "system_info" or "system_warning".
    // retain=false — same reasoning as air quality events.
    void publishSystemEvent(const char *category, const char *severity,
                            const char *message);

private:
    void loadPrefs();
    void initDeviceId();
    void refreshHostBuffer();
    void setupClient();
    bool prepareBrokerEndpoint(IPAddress &resolved_ip, bool &using_resolved_ip,
                               bool &is_mdns_host);
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
    // [NEW] Registers the "Events" entity as a sensor (not event) via HA MQTT
    // Discovery. The sensor state is the human-readable message text so it
    // appears directly in the HA logbook. Also cleans up legacy "event"-type
    // entities from earlier firmware builds by publishing empty retained payloads.
    void publishDiscoveryEvent();
    void publishNightModeAvailability();
    void publishState(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on);

    void handleCallback(char *topic, uint8_t *payload, unsigned int length);
    static void staticCallback(char *topic, uint8_t *payload, unsigned int length);
    static bool payloadIsOn(const char *payload);
    static bool payloadIsOff(const char *payload);

    StorageManager *storage_ = nullptr;
    AuraNetworkManager *network_ = nullptr;
    WiFiClient net_;
    PubSubClient client_;
    bool ui_dirty_ = false;

    static constexpr size_t kMqttHostBufferSize = 256;
    static constexpr size_t kMqttStatePayloadBufferSize = Config::MQTT_BUFFER_SIZE;
    String mqtt_host_;
    char mqtt_host_buf_[kMqttHostBufferSize] = {0};
    char mqtt_state_payload_buf_[kMqttStatePayloadBufferSize] = {0};
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
    uint32_t mqtt_connect_attempts_ = 0;
    bool mqtt_connect_deferred_by_web_ = false;
    bool mqtt_publish_deferred_by_web_ = false;
    String mqtt_mdns_cache_host_;
    IPAddress mqtt_mdns_cache_ip_;
    uint32_t mqtt_mdns_cache_ts_ms_ = 0;
    bool mqtt_mdns_cache_success_ = false;
    bool mqtt_mdns_cache_valid_ = false;
    bool auto_night_enabled_ = false;
    PendingCommands pending_;
};

// [NEW] Free function for air quality band-change events.
// Called from SensorManager without creating a circular header dependency.
// message is the same human-readable string already written to the serial log,
// e.g. "CO2 worsened to bad: 1480 ppm".
void MqttPublishEvent(const char *metric_key, const char *band,
                      float value, const char *unit, const char *message);

// [NEW] Free function for system log events.
// Called from SensorManager (and any other module) to mirror system log
// entries into HA, matching what the web dashboard Events tab shows.
// severity: "info" or "warning"
void MqttPublishSystemEvent(const char *category, const char *severity,
                            const char *message);
