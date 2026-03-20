// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/MqttManager.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_event.h>
#include "core/Logger.h"
#include "core/WifiPowerSaveGuard.h"
#include "modules/MqttPayloadBuilder.h"
#include "modules/StorageManager.h"
#include "modules/NetworkManager.h"
#include "web/WebRuntime.h"

namespace {

MqttManager *g_mqtt = nullptr;

constexpr size_t kTopicBufferSize = 256;
constexpr uint32_t kMqttMdnsSuccessCacheMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kMqttMdnsFailureCacheMs = 60UL * 1000UL;
constexpr int kMqttConnectTimeoutShortMs = 3000;
constexpr int kMqttConnectTimeoutSlowMs = 3000;
constexpr uint8_t kMqttLongRetryLogEveryAttempts = 6;

void append_json_escaped(String &out, const char *value) {
    if (!value) {
        return;
    }
    const uint8_t *p = reinterpret_cast<const uint8_t *>(value);
    while (*p) {
        switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(*p));
                    out += buf;
                } else {
                    out += static_cast<char>(*p);
                }
                break;
        }
        ++p;
    }
}

void append_json_escaped(String &out, const String &value) {
    append_json_escaped(out, value.c_str());
}

const char *retry_delay_label(uint32_t delay_ms) {
    if (delay_ms == Config::MQTT_RETRY_MS) {
        return "30 seconds";
    }
    if (delay_ms == Config::MQTT_RETRY_MEDIUM_MS) {
        return "2 minutes";
    }
    return "10 minutes";
}

bool should_log_connect_failure(uint32_t failed_attempts) {
    const uint8_t stage = MqttManager::retryStageForAttempts(failed_attempts);
    if (stage < 2) {
        return true;
    }
    const uint32_t long_stage_attempt =
        failed_attempts -
        (static_cast<uint32_t>(Config::MQTT_RETRY_SHORT_ATTEMPTS) +
         static_cast<uint32_t>(Config::MQTT_RETRY_MEDIUM_ATTEMPTS));
    return long_stage_attempt <= 1 ||
           (long_stage_attempt % kMqttLongRetryLogEveryAttempts) == 0;
}

int connect_timeout_ms_for_attempts(uint32_t failed_attempts) {
    return MqttManager::retryStageForAttempts(failed_attempts) >= 1
               ? kMqttConnectTimeoutSlowMs
               : kMqttConnectTimeoutShortMs;
}

void append_buffer_to_string(String &out, const char *data, size_t length) {
    if (!data || length == 0) {
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        out += data[i];
    }
}

void build_state_topic(char *out, size_t out_size, const String &base) {
    snprintf(out, out_size, "%s/state", base.c_str());
}

void build_availability_topic(char *out, size_t out_size, const String &base) {
    snprintf(out, out_size, "%s/status", base.c_str());
}

void build_night_mode_availability_topic(char *out, size_t out_size, const String &base) {
    snprintf(out, out_size, "%s/availability/night_mode", base.c_str());
}

void build_command_topic(char *out, size_t out_size, const String &base, const char *command) {
    snprintf(out, out_size, "%s/command/%s", base.c_str(), command);
}

void build_discovery_topic(char *out, size_t out_size, const char *component,
                           const String &device_id, const char *object_id) {
    snprintf(out, out_size, "homeassistant/%s/%s_%s/config",
             component, device_id.c_str(), object_id);
}

bool equals_ignore_case(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

void trim_ascii(char *text) {
    if (!text) {
        return;
    }
    char *start = text;
    while (*start && isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    char *end = start + strlen(start);
    while (end > start && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    size_t len = static_cast<size_t>(end - start);
    if (start != text) {
        memmove(text, start, len);
    }
    text[len] = '\0';
}

} // namespace

MqttManager::MqttManager() {
    command_context_mutex_ = xSemaphoreCreateMutexStatic(&command_context_mutex_buffer_);
}

uint8_t MqttManager::retryStageForAttempts(uint32_t failed_attempts) {
    if (failed_attempts <= Config::MQTT_RETRY_SHORT_ATTEMPTS) {
        return 0;
    }
    const uint32_t medium_limit =
        static_cast<uint32_t>(Config::MQTT_RETRY_SHORT_ATTEMPTS) +
        static_cast<uint32_t>(Config::MQTT_RETRY_MEDIUM_ATTEMPTS);
    if (failed_attempts <= medium_limit) {
        return 1;
    }
    return 2;
}

uint32_t MqttManager::retryDelayMsForAttempts(uint32_t failed_attempts) {
    switch (retryStageForAttempts(failed_attempts)) {
        case 0: return Config::MQTT_RETRY_MS;
        case 1: return Config::MQTT_RETRY_MEDIUM_MS;
        default: return Config::MQTT_RETRY_LONG_MS;
    }
}

uint8_t MqttManager::retryStage() const {
    return retryStageForAttempts(mqtt_connect_attempts_);
}

uint32_t MqttManager::retryDelayMs() const {
    return retryDelayMsForAttempts(mqtt_connect_attempts_);
}

void MqttManager::begin(StorageManager &storage,
                        AuraNetworkManager &network,
                        MqttRuntimeState &runtime_state) {
    storage_ = &storage;
    network_ = &network;
    runtime_state_ = &runtime_state;
    g_mqtt = this;
    loadPrefs();
    initDeviceId();
    setupClient();
}

void MqttManager::loadPrefs() {
    if (!storage_) {
        return;
    }
    storage_->loadMqttSettings(mqtt_host_, mqtt_port_, mqtt_user_, mqtt_pass_, mqtt_base_topic_,
                               mqtt_device_name_, mqtt_user_enabled_, mqtt_discovery_,
                               mqtt_anonymous_);
    mqtt_enabled_ = mqtt_user_enabled_;
    if (mqtt_base_topic_.endsWith("/")) {
        mqtt_base_topic_.remove(mqtt_base_topic_.length() - 1);
    }
    if (mqtt_base_topic_.isEmpty()) {
        mqtt_base_topic_ = Config::MQTT_DEFAULT_BASE;
    }
    if (mqtt_device_name_.isEmpty()) {
        mqtt_device_name_ = Config::MQTT_DEFAULT_NAME;
    }
    if (mqtt_port_ == 0) {
        mqtt_port_ = Config::MQTT_DEFAULT_PORT;
    }
    refreshHostBuffer();
}

void MqttManager::initDeviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[24];
    snprintf(buf, sizeof(buf), "aura_%04X%08X", static_cast<uint16_t>(mac >> 32),
             static_cast<uint32_t>(mac & 0xFFFFFFFF));
    mqtt_device_id_ = buf;
}

void MqttManager::refreshHostBuffer() {
    String broker_host = mqtt_host_;
    broker_host.trim();
    if (broker_host != mqtt_host_) {
        mqtt_host_ = broker_host;
    }

    if (mqtt_host_.length() >= kMqttHostBufferSize) {
        mqtt_host_.remove(kMqttHostBufferSize - 1);
        LOGW("MQTT", "broker host truncated to %u chars",
             static_cast<unsigned>(kMqttHostBufferSize - 1));
    }

    const size_t len = mqtt_host_.length();
    if (len > 0) {
        memcpy(mqtt_host_buf_, mqtt_host_.c_str(), len);
    }
    mqtt_host_buf_[len] = '\0';
}

void MqttManager::setupClient() {
    refreshHostBuffer();
    snprintf(mqtt_broker_endpoint_buf_, sizeof(mqtt_broker_endpoint_buf_), "%s", mqtt_host_buf_);
}

void MqttManager::stopClient() {
    if (mqtt_manual_stop_) {
        return;
    }
    mqtt_manual_stop_ = true;
    mqtt_connection_signal_.store(static_cast<uint8_t>(ConnectionSignal::None),
                                  std::memory_order_release);
    mqtt_connecting_ = false;
    mqtt_connected_ = false;

    if (!client_) {
        mqtt_client_started_ = false;
        return;
    }

    if (mqtt_client_started_) {
        esp_mqtt_client_disconnect(client_);
        return;
    }
    mqtt_client_needs_destroy_ = true;
}

void MqttManager::destroyClient() {
    mqtt_connection_signal_.store(static_cast<uint8_t>(ConnectionSignal::None),
                                  std::memory_order_release);
    mqtt_active_client_.store(nullptr, std::memory_order_release);
    if (client_) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    mqtt_client_started_ = false;
    mqtt_client_needs_destroy_ = false;
    mqtt_connecting_ = false;
    mqtt_connected_ = false;
    mqtt_manual_stop_ = false;
    mqtt_event_topic_.clear();
    mqtt_event_payload_.clear();
}

bool MqttManager::connectTransport(const char *client_id, const char *will_topic) {
    destroyClient();

    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = mqtt_broker_endpoint_buf_;
    config.broker.address.port = mqtt_port_;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    config.credentials.client_id = client_id;
    if (!mqtt_anonymous_ && mqtt_user_.length()) {
        config.credentials.username = mqtt_user_.c_str();
        config.credentials.authentication.password = mqtt_pass_.c_str();
    }
    config.session.last_will.topic = will_topic;
    config.session.last_will.msg = Config::MQTT_AVAIL_OFFLINE;
    config.session.last_will.msg_len = static_cast<int>(strlen(Config::MQTT_AVAIL_OFFLINE));
    config.session.last_will.qos = 0;
    config.session.last_will.retain = 1;
    config.session.keepalive = 30;
    config.network.timeout_ms = connect_timeout_ms_for_attempts(mqtt_connect_attempts_);
    config.network.disable_auto_reconnect = true;
    config.buffer.size = Config::MQTT_BUFFER_SIZE;
    config.buffer.out_size = Config::MQTT_BUFFER_SIZE;

    client_ = esp_mqtt_client_init(&config);
    if (!client_) {
        LOGW("MQTT", "esp_mqtt_client_init failed");
        return false;
    }
    if (esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY,
                                       &MqttManager::staticEventHandler, this) != ESP_OK) {
        LOGW("MQTT", "esp_mqtt_client_register_event failed");
        destroyClient();
        return false;
    }
    mqtt_active_client_.store(client_, std::memory_order_release);
    if (esp_mqtt_client_start(client_) != ESP_OK) {
        mqtt_active_client_.store(nullptr, std::memory_order_release);
        LOGW("MQTT", "esp_mqtt_client_start failed");
        destroyClient();
        return false;
    }

    mqtt_client_started_ = true;
    mqtt_connecting_ = true;
    mqtt_manual_stop_ = false;
    mqtt_last_error_rc_.store(0, std::memory_order_release);
    return true;
}

bool MqttManager::publishMessage(const char *topic, const char *payload, bool retain) {
    if (!client_ || !mqtt_connected_) {
        return false;
    }
    return esp_mqtt_client_publish(client_,
                                   topic,
                                   payload ? payload : "",
                                   0,
                                   0,
                                   retain ? 1 : 0) >= 0;
}

bool MqttManager::publishMessage(const char *topic, const uint8_t *payload, size_t length, bool retain) {
    if (!client_ || !mqtt_connected_) {
        return false;
    }
    return esp_mqtt_client_publish(client_,
                                   topic,
                                   reinterpret_cast<const char *>(payload),
                                   static_cast<int>(length),
                                   0,
                                   retain ? 1 : 0) >= 0;
}

bool MqttManager::subscribeTopic(const char *topic) {
    if (!client_ || !mqtt_connected_) {
        return false;
    }
    return esp_mqtt_client_subscribe(client_, topic, 0) >= 0;
}

bool MqttManager::prepareBrokerEndpoint(BrokerEndpoint &endpoint) {
    endpoint = BrokerEndpoint{};

    refreshHostBuffer();
    String broker_host = mqtt_host_;
    if (broker_host.isEmpty()) {
        return false;
    }

    String host_lc = broker_host;
    host_lc.toLowerCase();
    endpoint.is_mdns_host = host_lc.endsWith(".local");
    if (!endpoint.is_mdns_host) {
        return true;
    }

    const uint32_t now = millis();
    if (mqtt_mdns_cache_valid_ && broker_host.equalsIgnoreCase(mqtt_mdns_cache_host_)) {
        const uint32_t cache_age_ms = now - mqtt_mdns_cache_ts_ms_;
        const uint32_t cache_ttl_ms =
            mqtt_mdns_cache_success_ ? kMqttMdnsSuccessCacheMs : kMqttMdnsFailureCacheMs;
        if (cache_age_ms < cache_ttl_ms) {
            if (mqtt_mdns_cache_success_) {
                endpoint.ip = mqtt_mdns_cache_ip_;
                endpoint.use_ip = true;
            }
            return true;
        }
    }

    String mdns_name = broker_host.substring(0, broker_host.length() - 6);
    mdns_name.trim();
    if (mdns_name.isEmpty()) {
        return true;
    }

    IPAddress mdns_ip = MDNS.queryHost(mdns_name.c_str());
    mqtt_mdns_cache_host_ = broker_host;
    mqtt_mdns_cache_ts_ms_ = now;
    mqtt_mdns_cache_valid_ = true;
    if (static_cast<uint32_t>(mdns_ip) != 0U) {
        mqtt_mdns_cache_success_ = true;
        mqtt_mdns_cache_ip_ = mdns_ip;
        endpoint.ip = mdns_ip;
        endpoint.use_ip = true;
    } else {
        mqtt_mdns_cache_success_ = false;
        mqtt_mdns_cache_ip_ = IPAddress();
    }
    return true;
}

void MqttManager::applyBrokerEndpoint(const BrokerEndpoint &endpoint) {
    if (endpoint.use_ip) {
        String ip = endpoint.ip.toString();
        snprintf(mqtt_broker_endpoint_buf_, sizeof(mqtt_broker_endpoint_buf_), "%s", ip.c_str());
        return;
    }
    snprintf(mqtt_broker_endpoint_buf_, sizeof(mqtt_broker_endpoint_buf_), "%s", mqtt_host_buf_);
}

void MqttManager::publishDiscoverySensor(const char *object_id, const char *name,
                                         const char *unit, const char *device_class,
                                         const char *state_class, const char *value_template,
                                         const char *icon) {
    if (!mqtt_connected_) {
        return;
    }
    String payload = MqttPayloadBuilder::buildDiscoverySensorPayload(
        mqtt_device_id_,
        mqtt_device_name_,
        mqtt_base_topic_,
        object_id,
        name,
        unit,
        device_class,
        state_class,
        value_template,
        icon);

    char topic[kTopicBufferSize];
    build_discovery_topic(topic, sizeof(topic), "sensor", mqtt_device_id_, object_id);
    publishMessage(topic, payload.c_str(), true);
}

void MqttManager::publishDiscoverySwitch(const char *object_id, const char *name,
                                         const char *value_template, const char *icon) {
    if (!mqtt_connected_) {
        return;
    }
    String payload;
    payload.reserve(640);
    payload = "{";
    payload += "\"name\":\"";
    append_json_escaped(payload, name);
    payload += "\",\"unique_id\":\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "_";
    append_json_escaped(payload, object_id);
    payload += "\",\"state_topic\":\"";
    char topic[kTopicBufferSize];
    build_state_topic(topic, sizeof(topic), mqtt_base_topic_);
    append_json_escaped(payload, topic);
    payload += "\",\"command_topic\":\"";
    build_command_topic(topic, sizeof(topic), mqtt_base_topic_, object_id);
    append_json_escaped(payload, topic);
    if (strcmp(object_id, "night_mode") == 0) {
        payload += "\",\"availability\":[{\"topic\":\"";
        build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
        append_json_escaped(payload, topic);
        payload += "\",\"payload_available\":\"";
        payload += Config::MQTT_AVAIL_ONLINE;
        payload += "\",\"payload_not_available\":\"";
        payload += Config::MQTT_AVAIL_OFFLINE;
        payload += "\"},{\"topic\":\"";
        build_night_mode_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
        append_json_escaped(payload, topic);
        payload += "\",\"payload_available\":\"";
        payload += Config::MQTT_AVAIL_ONLINE;
        payload += "\",\"payload_not_available\":\"";
        payload += Config::MQTT_AVAIL_OFFLINE;
        payload += "\"}]";
        payload += ",\"availability_mode\":\"all\"";
    } else {
        payload += "\",\"availability_topic\":\"";
        build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
        append_json_escaped(payload, topic);
        payload += "\",\"payload_available\":\"";
        payload += Config::MQTT_AVAIL_ONLINE;
        payload += "\",\"payload_not_available\":\"";
        payload += Config::MQTT_AVAIL_OFFLINE;
        payload += "\"";
    }
    payload += ",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"";
    payload += ",\"state_on\":\"ON\",\"state_off\":\"OFF\"";
    if (value_template && value_template[0] != '\0') {
        payload += ",\"value_template\":\"";
        append_json_escaped(payload, value_template);
        payload += "\"";
    }
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        append_json_escaped(payload, icon);
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "\"],\"name\":\"";
    append_json_escaped(payload, mqtt_device_name_);
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    build_discovery_topic(topic, sizeof(topic), "switch", mqtt_device_id_, object_id);
    publishMessage(topic, payload.c_str(), true);
}

void MqttManager::publishDiscoveryButton(const char *object_id, const char *name,
                                         const char *payload_press, const char *icon) {
    if (!mqtt_connected_) {
        return;
    }
    String payload;
    payload.reserve(420);
    payload = "{";
    payload += "\"name\":\"";
    append_json_escaped(payload, name);
    payload += "\",\"unique_id\":\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "_";
    append_json_escaped(payload, object_id);
    payload += "\",\"command_topic\":\"";
    char topic[kTopicBufferSize];
    build_command_topic(topic, sizeof(topic), mqtt_base_topic_, object_id);
    append_json_escaped(payload, topic);
    payload += "\",\"payload_press\":\"";
    append_json_escaped(payload, payload_press);
    payload += "\",\"availability_topic\":\"";
    build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
    append_json_escaped(payload, topic);
    payload += "\"";
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        append_json_escaped(payload, icon);
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "\"],\"name\":\"";
    append_json_escaped(payload, mqtt_device_name_);
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    build_discovery_topic(topic, sizeof(topic), "button", mqtt_device_id_, object_id);
    publishMessage(topic, payload.c_str(), true);
}

// [NEW] Registers the "Events" sensor entity via HA MQTT Discovery.
//
// Uses component type "sensor" instead of "event" so the state value —
// the "message" field from the payload — appears directly in the HA logbook
// and history as readable text, e.g.:
//   "CO2 worsened to bad: 1480 ppm"
//   "NTP sync completed, local time=2026-03-19 09:41:28"
//   "SEN66 start attempts exhausted, stop probing until reboot"
//
// The value_template extracts the "message" key from the JSON payload that
// publishEvent() and publishSystemEvent() both send.
// For HA automations the full payload (event_type, category, value, unit)
// is still available via trigger.to_state.attributes.
void MqttManager::publishDiscoveryEvent() {
    if (!client_.connected()) {
        return;
    }

    char topic[kTopicBufferSize];
    // [NEW] "sensor" instead of "event" — state is the message text, not an event_type key.
    build_discovery_topic(topic, sizeof(topic), "sensor", mqtt_device_id_, "events");

    char state_topic[kTopicBufferSize];
    snprintf(state_topic, sizeof(state_topic), "%s/events", mqtt_base_topic_.c_str());
    char avail_topic[kTopicBufferSize];
    build_availability_topic(avail_topic, sizeof(avail_topic), mqtt_base_topic_);

    String payload;
    payload.reserve(512);
    payload = "{\"name\":\"Events\"";
    payload += ",\"unique_id\":\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "_events\"";
    payload += ",\"state_topic\":\"";
    append_json_escaped(payload, state_topic);
    payload += "\"";
    // [NEW] value_template extracts the human-readable message from the JSON payload.
    // This is what HA displays in the logbook and history graph label.
    payload += ",\"value_template\":\"{{ value_json.message }}\"";
    payload += ",\"icon\":\"mdi:bell-alert\"";
    payload += ",\"availability_topic\":\"";
    append_json_escaped(payload, avail_topic);
    payload += "\",\"payload_available\":\"";
    payload += Config::MQTT_AVAIL_ONLINE;
    payload += "\",\"payload_not_available\":\"";
    payload += Config::MQTT_AVAIL_OFFLINE;
    payload += "\"";
    payload += ",\"device\":{\"identifiers\":[\"";
    append_json_escaped(payload, mqtt_device_id_);
    payload += "\"],\"name\":\"";
    append_json_escaped(payload, mqtt_device_name_);
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    client_.publish(topic, payload.c_str(), true);
}

void MqttManager::publishDiscovery() {
    if (!mqtt_discovery_ || mqtt_discovery_sent_ || !mqtt_connected_) {
        return;
    }
    // Remove legacy PM4 discovery entity variant (retained) from older firmware versions.
    char legacy_topic[kTopicBufferSize];
    build_discovery_topic(legacy_topic, sizeof(legacy_topic), "sensor", mqtt_device_id_, "pm4_0");
    publishMessage(legacy_topic, "", true);

    // [NEW] Remove the two legacy event-type entities that were registered in
    // earlier firmware builds. Publishing an empty retained payload tells HA
    // to delete the entity from the device registry.
    // "air_events" — first iteration object_id before it was renamed to "events".
    build_discovery_topic(legacy_topic, sizeof(legacy_topic), "event", mqtt_device_id_, "air_events");
    client_.publish(legacy_topic, "", true);
    // "events" as the old "event" component type — now replaced by "sensor".
    build_discovery_topic(legacy_topic, sizeof(legacy_topic), "event", mqtt_device_id_, "events");
    client_.publish(legacy_topic, "", true);

    publishDiscoverySensor("temperature", "Temperature", "\\u00b0C",
                           "temperature", "measurement", "{{ value_json.temp }}", "");
    publishDiscoverySensor("humidity", "Humidity", "%",
                           "humidity", "measurement", "{{ value_json.humidity }}", "");
    publishDiscoverySensor("dew_point", "Dew Point", "\\u00b0C",
                           "temperature", "measurement", "{{ value_json.dew_point }}", "mdi:thermometer-water");
    publishDiscoverySensor("absolute_humidity", "Absolute Humidity", "g/m\\u00b3",
                           "", "measurement", "{{ value_json.absolute_humidity }}", "mdi:water");
    publishDiscoverySensor("co2", "CO2", "ppm",
                           "carbon_dioxide", "measurement", "{{ value_json.co2 }}", "");
    publishDiscoverySensor("co", "CO", "ppm",
                           "carbon_monoxide", "measurement", "{{ value_json.co }}", "mdi:molecule-co");
    publishDiscoverySensor("voc_index", "VOC Index", "index",
                           "", "measurement", "{{ value_json.voc_index }}", "mdi:blur");
    publishDiscoverySensor("nox_index", "NOx Index", "index",
                           "", "measurement", "{{ value_json.nox_index }}", "mdi:cloud-alert");
    publishDiscoverySensor("hcho", "HCHO", "ppb",
                           "volatile_organic_compounds_parts", "measurement",
                           "{{ value_json.hcho }}", "mdi:flask-outline");
    publishDiscoverySensor("pm1", "PM1.0", "\\u00b5g/m\\u00b3",
                           "", "measurement", "{{ value_json.pm1 }}", "mdi:molecule");
    publishDiscoverySensor("pm4", "PM4.0", "\\u00b5g/m\\u00b3",
                           "", "measurement", "{{ value_json.pm4 }}", "mdi:molecule-co2");
    publishDiscoverySensor("pm05", "PM0.5", "#/cm\\u00b3",
                           "", "measurement", "{{ value_json.pm05 }}", "mdi:dots-hexagon");
    publishDiscoverySensor("pm25", "PM2.5", "\\u00b5g/m\\u00b3",
                           "pm25", "measurement", "{{ value_json.pm25 }}", "");
    publishDiscoverySensor("pm10", "PM10", "\\u00b5g/m\\u00b3",
                           "pm10", "measurement", "{{ value_json.pm10 }}", "");
    publishDiscoverySensor("pressure", "Pressure", "hPa",
                           "pressure", "measurement", "{{ value_json.pressure }}", "");
    publishDiscoverySensor("pressure_delta_3h", "Pressure Delta 3h", "hPa",
                           "", "measurement", "{{ value_json.pressure_delta_3h }}", "mdi:trending-up");
    publishDiscoverySensor("pressure_delta_24h", "Pressure Delta 24h", "hPa",
                           "", "measurement", "{{ value_json.pressure_delta_24h }}", "mdi:trending-up");
    publishDiscoverySwitch("night_mode", "Night Mode", "{{ value_json.night_mode }}", "mdi:weather-night");
    publishDiscoverySwitch("alert_blink", "Alert Blink", "{{ value_json.alert_blink }}", "mdi:alarm-light");
    publishDiscoverySwitch("backlight", "Backlight", "{{ value_json.backlight }}", "mdi:television");
    publishDiscoveryButton("restart", "Restart", "PRESS", "mdi:restart");

    // [NEW] Register the combined air quality + system events entity.
    publishDiscoveryEvent();

    mqtt_discovery_sent_ = true;
    publishNightModeAvailability();
}

void MqttManager::publishNightModeAvailability() {
    if (!mqtt_connected_) {
        return;
    }
    char topic[kTopicBufferSize];
    build_night_mode_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
    const char *payload = auto_night_enabled_ ? Config::MQTT_AVAIL_OFFLINE : Config::MQTT_AVAIL_ONLINE;
    publishMessage(topic, payload, true);
}

void MqttManager::publishState(const MqttRuntimeSnapshot &runtime) {
    if (!mqtt_connected_) {
        return;
    }
    const size_t payload_len = MqttPayloadBuilder::buildStatePayload(
        mqtt_state_payload_buf_, sizeof(mqtt_state_payload_buf_),
        runtime.data, runtime.night_mode, runtime.alert_blink, runtime.backlight_on);
    if (payload_len == 0) {
        Logger::log(Logger::Warn, "MQTT", "state payload build failed");
        return;
    }

    char topic[kTopicBufferSize];
    build_state_topic(topic, sizeof(topic), mqtt_base_topic_);
    bool published = publishMessage(topic,
                                    reinterpret_cast<const uint8_t *>(mqtt_state_payload_buf_),
                                    payload_len,
                                    true);

    if (published) {
        mqtt_fail_count_ = 0;
        mqtt_last_publish_ms_ = millis();
    } else {
        mqtt_fail_count_++;
        Logger::log(Logger::Warn, "MQTT", "publish failed (%u/%u)",
                    mqtt_fail_count_, Config::MQTT_MAX_FAILS);

        if (mqtt_fail_count_ >= Config::MQTT_MAX_FAILS) {
            LOGW("MQTT", "too many failures, disconnecting");
            stopClient();
            mqtt_fail_count_ = 0;
        }
    }
}

// [NEW] Publishes an air quality band-change event to "<base_topic>/events".
//
// Payload example:
//   {"event_type":"co2_bad","message":"CO2 worsened to bad: 1480 ppm",
//    "value":1480.00,"unit":"ppm"}
//
// The "message" field is the same human-readable string already written to the
// serial log, so HA shows exactly what the web dashboard Events tab would show.
// retain=false — events are transient, not persistent state.
void MqttManager::publishEvent(const char *metric_key, const char *band,
                                float value, const char *unit,
                                const char *message) {
    if (!client_.connected()) {
        return;
    }

    char event_type[48];
    snprintf(event_type, sizeof(event_type), "%s_%s", metric_key, band);

    char topic[kTopicBufferSize];
    snprintf(topic, sizeof(topic), "%s/events", mqtt_base_topic_.c_str());

    // Build the payload with append_json_escaped for the message field
    // to handle any special characters that might appear in log strings.
    String payload;
    payload.reserve(256);
    payload = "{\"event_type\":\"";
    payload += event_type;
    payload += "\",\"message\":\"";
    append_json_escaped(payload, message ? message : "");
    payload += "\",\"value\":";
    char val_buf[24];
    snprintf(val_buf, sizeof(val_buf), "%.2f", value);
    payload += val_buf;
    payload += ",\"unit\":\"";
    append_json_escaped(payload, unit ? unit : "");
    payload += "\"}";

    client_.publish(topic, payload.c_str(), false);
}

// [NEW] Publishes a system log event to "<base_topic>/events".
//
// Mirrors the system log entries visible in the web dashboard Events tab so
// HA sees the same events (sensor errors, NTP sync, boot milestones, etc.).
//
// Payload example:
//   {"event_type":"system_warning","message":"SEN66 not found (1/3)",
//    "category":"SENSORS"}
//
// severity "info"    -> event_type "system_info"
// severity "warning" -> event_type "system_warning"
// retain=false — same as air quality events.
void MqttManager::publishSystemEvent(const char *category, const char *severity,
                                      const char *message) {
    if (!client_.connected()) {
        return;
    }

    // Map severity string to the declared event_type values.
    const char *event_type = (severity && strcmp(severity, "warning") == 0)
                             ? "system_warning"
                             : "system_info";

    char topic[kTopicBufferSize];
    snprintf(topic, sizeof(topic), "%s/events", mqtt_base_topic_.c_str());

    String payload;
    payload.reserve(256);
    payload = "{\"event_type\":\"";
    payload += event_type;
    payload += "\",\"message\":\"";
    append_json_escaped(payload, message ? message : "");
    payload += "\",\"category\":\"";
    append_json_escaped(payload, category ? category : "");
    payload += "\"}";

    client_.publish(topic, payload.c_str(), false);
}

bool MqttManager::connectClient(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on) {
bool MqttManager::connectClient() {
    if (!mqtt_enabled_) {
        return false;
    }
    BrokerEndpoint broker_endpoint;
    if (!prepareBrokerEndpoint(broker_endpoint)) {
        return false;
    }
    applyBrokerEndpoint(broker_endpoint);

    auto note_connect_failure = [&](int rc, bool log_details) {
        if (mqtt_connect_attempts_ < UINT32_MAX) {
            mqtt_connect_attempts_++;
        }
        if (log_details && should_log_connect_failure(mqtt_connect_attempts_)) {
            uint32_t delay_ms = retryDelayMsForAttempts(mqtt_connect_attempts_);
            Logger::log(Logger::Warn, "MQTT",
                        "connect failed rc=%d (attempt %lu), retry in %s",
                        rc,
                        static_cast<unsigned long>(mqtt_connect_attempts_),
                        retry_delay_label(delay_ms));
        }
        ui_dirty_ = true;
    };

    if (!mqtt_anonymous_ && (mqtt_user_.isEmpty() || mqtt_pass_.isEmpty())) {
        LOGW("MQTT", "credentials missing and anonymous mode is OFF, connection disabled");
        note_connect_failure(-1, false);
        return false;
    }

    bool network_ready = network_ && network_->isEnabled() && network_->isConnected();
    wl_status_t wifi_status = WiFi.status();
    IPAddress local_ip = WiFi.localIP();
    int32_t rssi = WiFi.RSSI();
    String broker_target = mqtt_host_;
    if (broker_endpoint.use_ip) {
        broker_target = broker_endpoint.ip.toString();
        LOGI("MQTT", "resolved mDNS broker %s -> %s",
             mqtt_host_.c_str(), broker_target.c_str());
    } else if (broker_endpoint.is_mdns_host) {
        LOGW("MQTT", "mDNS resolve failed for %s, falling back to system DNS",
             mqtt_host_.c_str());
    }

    Logger::log(Logger::Info, "MQTT",
                "connecting to %s:%u (NetworkMgr=%s, WiFi.status=%d, IP=%s, RSSI=%ld dBm)",
                broker_target.c_str(),
                static_cast<unsigned>(mqtt_port_),
                network_ready ? "ready" : "NOT READY",
                static_cast<int>(wifi_status),
                local_ip.toString().c_str(),
                static_cast<long>(rssi));

    const char *client_id = mqtt_device_id_.c_str();
    char will_topic[kTopicBufferSize];
    build_availability_topic(will_topic, sizeof(will_topic), mqtt_base_topic_);
    WifiPowerSaveGuard wifi_ps_guard;
    wifi_ps_guard.suspend();
    mqtt_connection_signal_.store(static_cast<uint8_t>(ConnectionSignal::None),
                                  std::memory_order_release);
    mqtt_client_needs_destroy_ = false;
    if (!connectTransport(client_id, will_topic)) {
        note_connect_failure(-1, true);
        return false;
    }

    LOGI("MQTT", "connect started");
    // [NEW] Notify HA that a connection attempt is in progress.
    // The async connect result will be confirmed via connectTransport callback.
    publishSystemEvent("MQTT", "info", "MQTT connecting");
    ui_dirty_ = true;
    return true;
}

bool MqttManager::payloadIsOn(const char *payload) {
    if (!payload) {
        return false;
    }
    if (equals_ignore_case(payload, "ON") || strcmp(payload, "1") == 0 ||
        equals_ignore_case(payload, "TRUE") || equals_ignore_case(payload, "PRESS")) {
        return true;
    }
    return false;
}

bool MqttManager::payloadIsOff(const char *payload) {
    if (!payload) {
        return false;
    }
    if (equals_ignore_case(payload, "OFF") || strcmp(payload, "0") == 0 ||
        equals_ignore_case(payload, "FALSE")) {
        return true;
    }
    return false;
}

void MqttManager::handleIncomingMessage(const char *topic, const uint8_t *payload, size_t length) {
    String base_topic;
    bool auto_night_enabled = false;
    lockCommandContext();
    base_topic = mqtt_base_topic_;
    auto_night_enabled = auto_night_enabled_;
    unlockCommandContext();

    String t(topic ? topic : "");
    char msg[32];
    size_t copy_len = length < (sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
    if (payload && copy_len > 0) {
        memcpy(msg, payload, copy_len);
    }
    msg[copy_len] = '\0';
    trim_ascii(msg);
    if (!t.startsWith(base_topic)) {
        return;
    }
    String suffix = t.substring(base_topic.length());
    if (!suffix.startsWith("/command/")) {
        return;
    }
    String cmd = suffix.substring(strlen("/command/"));
    bool is_on = payloadIsOn(msg);
    bool is_off = payloadIsOff(msg);
    MqttPendingCommands pending_update;
    bool has_pending_update = false;

    if (cmd == "night_mode") {
        if (auto_night_enabled) {
            LOGI("MQTT", "night mode ignored (auto night enabled)");
            return;
        }
        if (is_on || is_off) {
            pending_update.night_mode_value = is_on;
            pending_update.night_mode = true;
            has_pending_update = true;
        }
    } else if (cmd == "alert_blink") {
        if (is_on || is_off) {
            pending_update.alert_blink_value = is_on;
            pending_update.alert_blink = true;
            has_pending_update = true;
        }
    } else if (cmd == "backlight") {
        if (is_on || is_off) {
            pending_update.backlight_value = is_on;
            pending_update.backlight = true;
            has_pending_update = true;
        }
    } else if (cmd == "restart") {
        if (is_on) {
            pending_update.restart = true;
            has_pending_update = true;
        }
    }

    if (has_pending_update && runtime_state_) {
        runtime_state_->mergePendingCommands(pending_update);
    }
}

void MqttManager::handleEvent(esp_mqtt_event_handle_t event) {
    if (!event) {
        return;
    }
    const esp_mqtt_client_handle_t active_client =
        mqtt_active_client_.load(std::memory_order_acquire);
    if (!active_client || event->client != active_client) {
        return;
    }

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            mqtt_connection_signal_.store(static_cast<uint8_t>(ConnectionSignal::Connected),
                                          std::memory_order_release);
            break;
        }
        case MQTT_EVENT_DISCONNECTED: {
            mqtt_connection_signal_.store(static_cast<uint8_t>(ConnectionSignal::Disconnected),
                                          std::memory_order_release);
            break;
        }
        case MQTT_EVENT_ERROR: {
            int error_rc = -1;
            if (event->error_handle) {
                const esp_mqtt_error_codes_t *error = event->error_handle;
                if (error->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    error_rc = static_cast<int>(error->connect_return_code);
                } else if (error->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT &&
                           error->esp_transport_sock_errno != 0) {
                    error_rc = -error->esp_transport_sock_errno;
                }
                mqtt_last_error_rc_.store(error_rc, std::memory_order_release);
                Logger::log(Logger::Warn, "MQTT",
                            "error event type=%d rc=%d tls=%d stack=%d sock=%d",
                            static_cast<int>(error->error_type),
                            error_rc,
                            static_cast<int>(error->esp_tls_last_esp_err),
                            static_cast<int>(error->esp_tls_stack_err),
                            static_cast<int>(error->esp_transport_sock_errno));
            } else {
                mqtt_last_error_rc_.store(error_rc, std::memory_order_release);
                LOGW("MQTT", "error event without details");
            }
            break;
        }
        case MQTT_EVENT_DATA: {
            if (event->current_data_offset == 0) {
                mqtt_event_topic_.clear();
                mqtt_event_payload_.clear();
                mqtt_event_topic_.reserve(event->topic_len > 0 ? static_cast<size_t>(event->topic_len) : 0);
                mqtt_event_payload_.reserve(event->total_data_len > 0 ? static_cast<size_t>(event->total_data_len) : static_cast<size_t>(event->data_len));
                append_buffer_to_string(mqtt_event_topic_, event->topic, static_cast<size_t>(event->topic_len));
            }
            append_buffer_to_string(mqtt_event_payload_, event->data, static_cast<size_t>(event->data_len));
            const int received_end = event->current_data_offset + event->data_len;
            if (event->total_data_len <= 0 || received_end >= event->total_data_len) {
                handleIncomingMessage(mqtt_event_topic_.c_str(),
                                      reinterpret_cast<const uint8_t *>(mqtt_event_payload_.c_str()),
                                      mqtt_event_payload_.length());
                mqtt_event_topic_.clear();
                mqtt_event_payload_.clear();
            }
            break;
        }
        default:
            break;
    }
}

void MqttManager::staticEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                     void *event_data) {
    (void)base;
    (void)event_id;

    MqttManager *manager = static_cast<MqttManager *>(handler_args);
    if (!manager) {
        manager = g_mqtt;
    }
    if (manager) {
        manager->handleEvent(static_cast<esp_mqtt_event_handle_t>(event_data));
    }
}

void MqttManager::poll(MqttRuntimeState &runtime_state) {
    auto note_connect_failure = [&](int rc) {
        if (mqtt_connect_attempts_ < UINT32_MAX) {
            mqtt_connect_attempts_++;
        }
        if (should_log_connect_failure(mqtt_connect_attempts_)) {
            uint32_t delay_ms = retryDelayMsForAttempts(mqtt_connect_attempts_);
            Logger::log(Logger::Warn, "MQTT",
                        "connect failed rc=%d (attempt %lu), retry in %s",
                        rc,
                        static_cast<unsigned long>(mqtt_connect_attempts_),
                        retry_delay_label(delay_ms));
        }
        ui_dirty_ = true;
    };

    const ConnectionSignal connection_signal = static_cast<ConnectionSignal>(
        mqtt_connection_signal_.exchange(static_cast<uint8_t>(ConnectionSignal::None),
                                         std::memory_order_acq_rel));
    if (connection_signal == ConnectionSignal::Connected) {
        mqtt_connected_ = true;
        mqtt_connecting_ = false;
        mqtt_fail_count_ = 0;
        mqtt_connect_attempts_ = 0;
        mqtt_last_error_rc_.store(0, std::memory_order_release);
        ui_dirty_ = true;

        char subscribe_topic[kTopicBufferSize];
        snprintf(subscribe_topic, sizeof(subscribe_topic), "%s/command/#", mqtt_base_topic_.c_str());
        subscribeTopic(subscribe_topic);

        char will_topic[kTopicBufferSize];
        build_availability_topic(will_topic, sizeof(will_topic), mqtt_base_topic_);
        publishMessage(will_topic, Config::MQTT_AVAIL_ONLINE, true);
        publishNightModeAvailability();
        mqtt_publish_requested_ = true;
        LOGI("MQTT", "connected");
    } else if (connection_signal == ConnectionSignal::Disconnected) {
        const bool was_connecting = mqtt_connecting_;
        const bool was_connected = mqtt_connected_;
        mqtt_connected_ = false;
        mqtt_connecting_ = false;
        ui_dirty_ = true;

        if (!mqtt_manual_stop_ && was_connecting) {
            note_connect_failure(mqtt_last_error_rc_.load(std::memory_order_acquire));
        }
        mqtt_client_needs_destroy_ = true;

        if (was_connected) {
            LOGW("MQTT", "disconnected");
        }
        if (mqtt_manual_stop_) {
            mqtt_manual_stop_ = false;
        }
    }
    if (mqtt_client_needs_destroy_ && client_) {
        if (mqtt_client_started_) {
            esp_mqtt_client_stop(client_);
            mqtt_client_started_ = false;
        }
        destroyClient();
    }

    const MqttRuntimeSnapshot runtime = runtime_state.snapshot();
    if (runtime.auto_night_enabled != auto_night_enabled_) {
        lockCommandContext();
        auto_night_enabled_ = runtime.auto_night_enabled;
        unlockCommandContext();
        publishNightModeAvailability();
    }
    if (runtime_state.consumePublishRequest()) {
        mqtt_publish_requested_ = true;
    }

    if (!mqtt_enabled_) {
        mqtt_connect_deferred_by_web_ = false;
        mqtt_publish_deferred_by_web_ = false;
        if (mqtt_connected_) {
            char topic[kTopicBufferSize];
            build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
            publishMessage(topic, Config::MQTT_AVAIL_OFFLINE, true);
        }
        if (client_) {
            stopClient();
        }
        mqtt_fail_count_ = 0;
        if (!mqtt_user_enabled_) {
            mqtt_connect_attempts_ = 0;
        }
        if (mqtt_connected_last_) {
            mqtt_connected_last_ = false;
            ui_dirty_ = true;
        }
        return;
    }
    if (!network_ || !network_->isConnected()) {
        mqtt_connect_deferred_by_web_ = false;
        mqtt_publish_deferred_by_web_ = false;
        if (mqtt_connected_) {
            LOGW("MQTT", "network unavailable, disconnecting gracefully");
            char topic[kTopicBufferSize];
            build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
            publishMessage(topic, Config::MQTT_AVAIL_OFFLINE, true);
        }
        if (client_) {
            stopClient();
        }
        mqtt_fail_count_ = 0;
        if (mqtt_connected_last_) {
            mqtt_connected_last_ = false;
            ui_dirty_ = true;
            LOGI("MQTT", "marked as disconnected (network unavailable)");
        }
        return;
    }

    if (mqtt_connected_ != mqtt_connected_last_) {
        mqtt_connected_last_ = mqtt_connected_;
        ui_dirty_ = true;
    }
    if (!mqtt_connected_) {
        mqtt_publish_deferred_by_web_ = false;
        if (mqtt_connecting_) {
            return;
        }
        uint32_t now = millis();
        uint32_t retry_delay = retryDelayMsForAttempts(mqtt_connect_attempts_);
        const bool immediate_first_attempt =
            (mqtt_connect_attempts_ == 0 && mqtt_last_attempt_ms_ == 0);
        if (immediate_first_attempt || (now - mqtt_last_attempt_ms_ >= retry_delay)) {
            if (WebHandlersShouldPauseMqttConnect()) {
                if (!mqtt_connect_deferred_by_web_) {
                    WebHandlersNoteMqttConnectDeferred();
                    mqtt_connect_deferred_by_web_ = true;
                }
                return;
            }
            mqtt_connect_deferred_by_web_ = false;
            mqtt_last_attempt_ms_ = now;
            connectClient();
        }
        return;
    }
    uint32_t now = millis();
    const bool publish_due =
        mqtt_publish_requested_ || (now - mqtt_last_publish_ms_ >= Config::MQTT_PUBLISH_MS);
    const bool discovery_due = mqtt_discovery_ && !mqtt_discovery_sent_;
    if (discovery_due || publish_due) {
        if (WebHandlersShouldPauseMqttPublish()) {
            if (!mqtt_publish_deferred_by_web_) {
                WebHandlersNoteMqttPublishDeferred();
                mqtt_publish_deferred_by_web_ = true;
            }
            return;
        }
        mqtt_publish_deferred_by_web_ = false;
        if (discovery_due) {
            publishDiscovery();
        }
    } else {
        mqtt_publish_deferred_by_web_ = false;
    }
    mqtt_connect_deferred_by_web_ = false;
    if (publish_due) {
        mqtt_publish_requested_ = false;
        publishState(runtime);
        return;
    }
}

void MqttManager::syncWithWifi() {
    bool wifi_ready = network_ && network_->isEnabled() && network_->isConnected();
    bool desired = mqtt_user_enabled_ && wifi_ready;
    if (desired != mqtt_enabled_) {
        mqtt_connect_deferred_by_web_ = false;
        mqtt_publish_deferred_by_web_ = false;
        mqtt_enabled_ = desired;
        if (mqtt_enabled_) {
            mqtt_fail_count_ = 0;
            setupClient();
            mqtt_last_attempt_ms_ = 0;
            mqtt_last_error_rc_.store(0, std::memory_order_release);
        } else {
            if (mqtt_connected_) {
                if (wifi_ready) {
                    char topic[kTopicBufferSize];
                    build_availability_topic(topic, sizeof(topic), mqtt_base_topic_);
                    publishMessage(topic, Config::MQTT_AVAIL_OFFLINE, true);
                }
            }
            if (client_) {
                stopClient();
            }
            mqtt_fail_count_ = 0;
            if (!mqtt_user_enabled_) {
                mqtt_connect_attempts_ = 0;
            }
        }
    }
    ui_dirty_ = true;
}

void MqttManager::requestReconnect() {
    LOGI("MQTT", "manual reconnect requested");
    mqtt_connect_attempts_ = 0;
    mqtt_connect_deferred_by_web_ = false;
    mqtt_publish_deferred_by_web_ = false;
    mqtt_discovery_sent_ = false;
    mqtt_last_attempt_ms_ = 0;
    mqtt_mdns_cache_valid_ = false;
    mqtt_last_error_rc_.store(0, std::memory_order_release);
    if (client_) {
        stopClient();
    }
    ui_dirty_ = true;
}

void MqttManager::setUserEnabled(bool enabled) {
    if (mqtt_user_enabled_ == enabled) {
        return;
    }
    mqtt_user_enabled_ = enabled;
    mqtt_connect_deferred_by_web_ = false;
    mqtt_publish_deferred_by_web_ = false;
    mqtt_discovery_sent_ = false;
    mqtt_connect_attempts_ = 0;
    if (storage_) {
        storage_->saveMqttEnabled(mqtt_user_enabled_);
    }
}

void MqttManager::applySavedSettings(const String &host,
                                     uint16_t port,
                                     const String &user,
                                     const String &pass,
                                     const String &base_topic,
                                     const String &device_name,
                                     bool discovery,
                                     bool anonymous) {
    String next_host = host;
    uint16_t next_port = port == 0 ? Config::MQTT_DEFAULT_PORT : port;
    String next_user = user;
    String next_pass = pass;
    String next_base_topic = base_topic;
    String next_device_name = device_name;

    if (next_base_topic.endsWith("/")) {
        next_base_topic.remove(next_base_topic.length() - 1);
    }
    if (next_base_topic.isEmpty()) {
        next_base_topic = Config::MQTT_DEFAULT_BASE;
    }
    if (next_device_name.isEmpty()) {
        next_device_name = Config::MQTT_DEFAULT_NAME;
    }

    lockCommandContext();
    mqtt_host_ = next_host;
    mqtt_port_ = next_port;
    mqtt_user_ = next_user;
    mqtt_pass_ = next_pass;
    mqtt_base_topic_ = next_base_topic;
    mqtt_device_name_ = next_device_name;
    mqtt_discovery_ = discovery;
    mqtt_anonymous_ = anonymous;
    unlockCommandContext();

    mqtt_discovery_sent_ = false;
    mqtt_connect_attempts_ = 0;
    mqtt_fail_count_ = 0;
    mqtt_last_attempt_ms_ = 0;
    mqtt_last_error_rc_.store(0, std::memory_order_release);
    mqtt_mdns_cache_valid_ = false;
    mqtt_connect_deferred_by_web_ = false;
    mqtt_publish_deferred_by_web_ = false;
    mqtt_publish_requested_ = false;
    refreshHostBuffer();
    setupClient();
    if (client_) {
        stopClient();
    }
    ui_dirty_ = true;
}

void MqttManager::lockCommandContext() const {
    if (command_context_mutex_) {
        xSemaphoreTake(command_context_mutex_, portMAX_DELAY);
    }
}

void MqttManager::unlockCommandContext() const {
    if (command_context_mutex_) {
        xSemaphoreGive(command_context_mutex_);
    }
}

// [NEW] Free function for air quality events — avoids circular header deps.
void MqttPublishEvent(const char *metric_key, const char *band,
                      float value, const char *unit, const char *message) {
    if (g_mqtt) {
        g_mqtt->publishEvent(metric_key, band, value, unit, message);
    }
}

// [NEW] Free function for system log events — avoids circular header deps.
void MqttPublishSystemEvent(const char *category, const char *severity,
                            const char *message) {
    if (g_mqtt) {
        g_mqtt->publishSystemEvent(category, severity, message);
    }
}
