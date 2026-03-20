// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mqtt_client.h>
#include "config/AppConfig.h"
#include "config/AppData.h"
#include "core/MqttRuntimeState.h"
#include "modules/MqttRuntime.h"

class StorageManager;
class AuraNetworkManager;

class MqttManager : public MqttRuntime {
public:
    MqttManager();

    void begin(StorageManager &storage,
               AuraNetworkManager &network,
               MqttRuntimeState &runtime_state);
    void poll(MqttRuntimeState &runtime_state);

    void syncWithWifi();
    void requestReconnect();
    void setUserEnabled(bool enabled);
    void applySavedSettings(const String &host,
                           uint16_t port,
                           const String &user,
                           const String &pass,
                           const String &base_topic,
                           const String &device_name,
                           bool discovery,
                           bool anonymous);

    bool isUserEnabled() const { return mqtt_user_enabled_; }
    bool isEnabled() const { return mqtt_enabled_; }
    bool isConnected() override { return mqtt_connected_; }
    uint32_t connectAttempts() const { return mqtt_connect_attempts_; }
    uint8_t retryStage() const override;
    uint32_t retryDelayMs() const;
    static uint8_t retryStageForAttempts(uint32_t failed_attempts);
    static uint32_t retryDelayMsForAttempts(uint32_t failed_attempts);
    bool isUiDirty() const { return ui_dirty_.load(std::memory_order_acquire); }
    void clearUiDirty() { ui_dirty_.store(false, std::memory_order_release); }
    void markUiDirty() { ui_dirty_.store(true, std::memory_order_release); }

    const String &host() const { return mqtt_host_; }
    uint16_t port() const { return mqtt_port_; }
    const String &user() const { return mqtt_user_; }
    const String &pass() const { return mqtt_pass_; }
    const String &baseTopic() const { return mqtt_base_topic_; }
    const String &deviceName() const { return mqtt_device_name_; }
    const String &deviceId() const { return mqtt_device_id_; }
    bool discoveryEnabled() const { return mqtt_discovery_; }
    bool isAnonymous() const { return mqtt_anonymous_; }

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
    struct BrokerEndpoint {
        IPAddress ip;
        bool use_ip = false;
        bool is_mdns_host = false;
    };

    void loadPrefs();
    void initDeviceId();
    void refreshHostBuffer();
    void setupClient();
    void stopClient();
    void destroyClient();
    bool prepareBrokerEndpoint(BrokerEndpoint &endpoint);
    void applyBrokerEndpoint(const BrokerEndpoint &endpoint);
    bool connectTransport(const char *client_id, const char *will_topic);
    bool publishMessage(const char *topic, const char *payload, bool retain);
    bool publishMessage(const char *topic, const uint8_t *payload, size_t length, bool retain);
    bool subscribeTopic(const char *topic);
    bool connectClient();
    void handleEvent(esp_mqtt_event_handle_t event);
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
    void publishState(const MqttRuntimeSnapshot &runtime);
    void lockCommandContext() const;
    void unlockCommandContext() const;

    void handleIncomingMessage(const char *topic, const uint8_t *payload, size_t length);
    static void staticEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                   void *event_data);
    static bool payloadIsOn(const char *payload);
    static bool payloadIsOff(const char *payload);

    enum class ConnectionSignal : uint8_t {
        None = 0,
        Connected,
        Disconnected,
    };

    StorageManager *storage_ = nullptr;
    AuraNetworkManager *network_ = nullptr;
    MqttRuntimeState *runtime_state_ = nullptr;
    esp_mqtt_client_handle_t client_ = nullptr;
    std::atomic<bool> ui_dirty_{false};
    std::atomic<esp_mqtt_client_handle_t> mqtt_active_client_{nullptr};
    std::atomic<uint8_t> mqtt_connection_signal_{static_cast<uint8_t>(ConnectionSignal::None)};
    std::atomic<int> mqtt_last_error_rc_{0};
    mutable StaticSemaphore_t command_context_mutex_buffer_{};
    mutable SemaphoreHandle_t command_context_mutex_ = nullptr;

    static constexpr size_t kMqttHostBufferSize = 256;
    static constexpr size_t kMqttStatePayloadBufferSize = Config::MQTT_BUFFER_SIZE;
    String mqtt_host_;
    char mqtt_host_buf_[kMqttHostBufferSize] = {0};
    char mqtt_broker_endpoint_buf_[kMqttHostBufferSize] = {0};
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
    bool mqtt_connected_ = false;
    bool mqtt_connected_last_ = false;
    bool mqtt_connecting_ = false;
    bool mqtt_client_started_ = false;
    uint8_t mqtt_fail_count_ = 0;
    uint32_t mqtt_connect_attempts_ = 0;
    bool mqtt_connect_deferred_by_web_ = false;
    bool mqtt_publish_deferred_by_web_ = false;
    bool mqtt_manual_stop_ = false;
    bool mqtt_client_needs_destroy_ = false;
    // MQTT_EVENT_DATA may arrive in chunks; these buffers belong only to the esp-mqtt event task.
    String mqtt_event_topic_;
    String mqtt_event_payload_;
    String mqtt_mdns_cache_host_;
    IPAddress mqtt_mdns_cache_ip_;
    uint32_t mqtt_mdns_cache_ts_ms_ = 0;
    bool mqtt_mdns_cache_success_ = false;
    bool mqtt_mdns_cache_valid_ = false;
    bool auto_night_enabled_ = false;
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
