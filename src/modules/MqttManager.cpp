// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/MqttManager.h"

#include <math.h>
#include <string.h>
#include <WiFi.h>
#include "core/Logger.h"
#include "modules/StorageManager.h"
#include "modules/NetworkManager.h"

namespace {

MqttManager *g_mqtt = nullptr;

constexpr uint8_t kMqttRetryStages = 3;
constexpr uint8_t kMqttRetryStageAttempts = Config::MQTT_CONNECT_MAX_FAILS;
constexpr uint8_t kMqttRetryMaxAttempts = kMqttRetryStages * kMqttRetryStageAttempts;

uint8_t retry_stage_for_attempts(uint32_t attempts) {
    return static_cast<uint8_t>(attempts / kMqttRetryStageAttempts);
}

uint32_t retry_delay_for_stage(uint8_t stage) {
    switch (stage) {
        case 0: return Config::MQTT_RETRY_MS;
        case 1: return Config::MQTT_RETRY_LONG_MS;
        default: return Config::MQTT_RETRY_HOURLY_MS;
    }
}

const char *retry_delay_label(uint32_t delay_ms) {
    if (delay_ms == Config::MQTT_RETRY_MS) {
        return "30 seconds";
    }
    if (delay_ms == Config::MQTT_RETRY_LONG_MS) {
        return "10 minutes";
    }
    return "1 hour";
}

} // namespace

MqttManager::MqttManager() : client_(net_) {}

void MqttManager::begin(StorageManager &storage, AuraNetworkManager &network) {
    storage_ = &storage;
    network_ = &network;
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
}

void MqttManager::initDeviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[24];
    snprintf(buf, sizeof(buf), "aura_%04X%08X", static_cast<uint16_t>(mac >> 32),
             static_cast<uint32_t>(mac & 0xFFFFFFFF));
    mqtt_device_id_ = buf;
}

void MqttManager::setupClient() {
    client_.setServer(mqtt_host_.c_str(), mqtt_port_);
    client_.setBufferSize(Config::MQTT_BUFFER_SIZE);
    client_.setKeepAlive(30);
    client_.setSocketTimeout(1);
    client_.setCallback(MqttManager::staticCallback);
}

String MqttManager::stateTopic() const {
    return mqtt_base_topic_ + "/state";
}

String MqttManager::availabilityTopic() const {
    return mqtt_base_topic_ + "/status";
}

String MqttManager::nightModeAvailabilityTopic() const {
    return mqtt_base_topic_ + "/availability/night_mode";
}

String MqttManager::discoveryTopic(const char *object_id) const {
    String topic = "homeassistant/sensor/";
    topic += mqtt_device_id_;
    topic += "_";
    topic += object_id;
    topic += "/config";
    return topic;
}

String MqttManager::commandTopic(const char *command) const {
    String topic = mqtt_base_topic_;
    topic += "/command/";
    topic += command;
    return topic;
}

void MqttManager::publishDiscoverySensor(const char *object_id, const char *name,
                                         const char *unit, const char *device_class,
                                         const char *state_class, const char *value_template,
                                         const char *icon) {
    if (!client_.connected()) {
        return;
    }
    String payload = "{";
    payload += "\"name\":\"";
    payload += name;
    payload += "\",\"unique_id\":\"";
    payload += mqtt_device_id_;
    payload += "_";
    payload += object_id;
    payload += "\",\"state_topic\":\"";
    payload += stateTopic();
    payload += "\",\"availability_topic\":\"";
    payload += availabilityTopic();
    payload += "\",\"payload_available\":\"";
    payload += Config::MQTT_AVAIL_ONLINE;
    payload += "\",\"payload_not_available\":\"";
    payload += Config::MQTT_AVAIL_OFFLINE;
    payload += "\"";
    if (value_template && value_template[0] != '\0') {
        payload += ",\"value_template\":\"";
        payload += value_template;
        payload += "\"";
    }
    if (unit && unit[0] != '\0') {
        payload += ",\"unit_of_measurement\":\"";
        payload += unit;
        payload += "\"";
    }
    if (device_class && device_class[0] != '\0') {
        payload += ",\"device_class\":\"";
        payload += device_class;
        payload += "\"";
    }
    if (state_class && state_class[0] != '\0') {
        payload += ",\"state_class\":\"";
        payload += state_class;
        payload += "\"";
    }
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        payload += icon;
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    payload += mqtt_device_id_;
    payload += "\"],\"name\":\"";
    payload += mqtt_device_name_;
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    String topic = discoveryTopic(object_id);
    client_.publish(topic.c_str(), payload.c_str(), true);
}

void MqttManager::publishDiscoverySwitch(const char *object_id, const char *name,
                                         const char *value_template, const char *icon) {
    if (!client_.connected()) {
        return;
    }
    String payload = "{";
    payload += "\"name\":\"";
    payload += name;
    payload += "\",\"unique_id\":\"";
    payload += mqtt_device_id_;
    payload += "_";
    payload += object_id;
    payload += "\",\"state_topic\":\"";
    payload += stateTopic();
    payload += "\",\"command_topic\":\"";
    payload += commandTopic(object_id);
    if (strcmp(object_id, "night_mode") == 0) {
        payload += "\",\"availability\":[{\"topic\":\"";
        payload += availabilityTopic();
        payload += "\",\"payload_available\":\"";
        payload += Config::MQTT_AVAIL_ONLINE;
        payload += "\",\"payload_not_available\":\"";
        payload += Config::MQTT_AVAIL_OFFLINE;
        payload += "\"},{\"topic\":\"";
        payload += nightModeAvailabilityTopic();
        payload += "\",\"payload_available\":\"";
        payload += Config::MQTT_AVAIL_ONLINE;
        payload += "\",\"payload_not_available\":\"";
        payload += Config::MQTT_AVAIL_OFFLINE;
        payload += "\"}]";
        payload += ",\"availability_mode\":\"all\"";
    } else {
        payload += "\",\"availability_topic\":\"";
        payload += availabilityTopic();
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
        payload += value_template;
        payload += "\"";
    }
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        payload += icon;
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    payload += mqtt_device_id_;
    payload += "\"],\"name\":\"";
    payload += mqtt_device_name_;
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    String topic = "homeassistant/switch/";
    topic += mqtt_device_id_;
    topic += "_";
    topic += object_id;
    topic += "/config";
    client_.publish(topic.c_str(), payload.c_str(), true);
}

void MqttManager::publishDiscoveryButton(const char *object_id, const char *name,
                                         const char *payload_press, const char *icon) {
    if (!client_.connected()) {
        return;
    }
    String payload = "{";
    payload += "\"name\":\"";
    payload += name;
    payload += "\",\"unique_id\":\"";
    payload += mqtt_device_id_;
    payload += "_";
    payload += object_id;
    payload += "\",\"command_topic\":\"";
    payload += commandTopic(object_id);
    payload += "\",\"payload_press\":\"";
    payload += payload_press;
    payload += "\",\"availability_topic\":\"";
    payload += availabilityTopic();
    payload += "\"";
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        payload += icon;
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    payload += mqtt_device_id_;
    payload += "\"],\"name\":\"";
    payload += mqtt_device_name_;
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";

    String topic = "homeassistant/button/";
    topic += mqtt_device_id_;
    topic += "_";
    topic += object_id;
    topic += "/config";
    client_.publish(topic.c_str(), payload.c_str(), true);
}

void MqttManager::publishDiscovery() {
    if (!mqtt_discovery_ || mqtt_discovery_sent_ || !client_.connected()) {
        return;
    }
    publishDiscoverySensor("temperature", "Temperature", "\\u00b0C",
                           "temperature", "measurement", "{{ value_json.temp }}", "");
    publishDiscoverySensor("humidity", "Humidity", "%",
                           "humidity", "measurement", "{{ value_json.humidity }}", "");
    publishDiscoverySensor("dew_point", "Dew Point", "\\u00b0C",
                           "temperature", "measurement", "{{ value_json.dew_point }}", "mdi:thermometer-water");
    publishDiscoverySensor("co2", "CO2", "ppm",
                           "carbon_dioxide", "measurement", "{{ value_json.co2 }}", "");
    publishDiscoverySensor("voc_index", "VOC Index", "index",
                           "", "measurement", "{{ value_json.voc_index }}", "mdi:blur");
    publishDiscoverySensor("nox_index", "NOx Index", "index",
                           "", "measurement", "{{ value_json.nox_index }}", "mdi:cloud-alert");
    publishDiscoverySensor("hcho", "HCHO", "ppb",
                           "volatile_organic_compounds_parts", "measurement",
                           "{{ value_json.hcho }}", "mdi:flask-outline");
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
    mqtt_discovery_sent_ = true;
    publishNightModeAvailability();
}

void MqttManager::publishNightModeAvailability() {
    if (!client_.connected()) {
        return;
    }
    String topic = nightModeAvailabilityTopic();
    const char *payload = auto_night_enabled_ ? Config::MQTT_AVAIL_OFFLINE : Config::MQTT_AVAIL_ONLINE;
    client_.publish(topic.c_str(), payload, true);
}

void MqttManager::publishState(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on) {
    if (!client_.connected()) {
        return;
    }
    String payload;
    payload.reserve(512);
    payload += "{";
    bool first = true;
    auto add_int = [&](const char *key, bool valid, int value) {
        if (!first) payload += ",";
        first = false;
        payload += "\"";
        payload += key;
        payload += "\":";
        if (valid) payload += String(value);
        else payload += "null";
    };
    auto add_float = [&](const char *key, bool valid, float value, int decimals) {
        if (!first) payload += ",";
        first = false;
        payload += "\"";
        payload += key;
        payload += "\":";
        if (valid) payload += String(value, decimals);
        else payload += "null";
    };
    auto add_bool = [&](const char *key, bool value) {
        if (!first) payload += ",";
        first = false;
        payload += "\"";
        payload += key;
        payload += "\":\"";
        payload += value ? "ON" : "OFF";
        payload += "\"";
    };

    float dew_c = NAN;
    bool dew_valid = data.temp_valid && data.hum_valid;
    if (dew_valid) {
        dew_c = computeDewPointC(data.temperature, data.humidity);
        dew_valid = isfinite(dew_c);
    }

    add_float("temp", data.temp_valid, data.temperature, 1);
    add_float("humidity", data.hum_valid, data.humidity, 1);
    add_float("dew_point", dew_valid, dew_c, 1);
    add_int("co2", data.co2_valid, data.co2);
    add_int("voc_index", data.voc_valid, data.voc_index);
    add_int("nox_index", data.nox_valid, data.nox_index);
    add_float("hcho", data.hcho_valid, data.hcho, 1);
    add_float("pm25", data.pm25_valid, data.pm25, 1);
    add_float("pm10", data.pm10_valid, data.pm10, 1);
    add_float("pressure", data.pressure_valid, data.pressure, 1);
    add_float("pressure_delta_3h", data.pressure_delta_3h_valid, data.pressure_delta_3h, 1);
    add_float("pressure_delta_24h", data.pressure_delta_24h_valid, data.pressure_delta_24h, 1);
    add_bool("night_mode", night_mode);
    add_bool("alert_blink", alert_blink);
    add_bool("backlight", backlight_on);
    payload += "}";

    String topic = stateTopic();
    bool published = client_.publish(topic.c_str(), payload.c_str(), true);

    if (published) {
        mqtt_fail_count_ = 0;
        mqtt_last_publish_ms_ = millis();
    } else {
        mqtt_fail_count_++;
        Logger::log(Logger::Warn, "MQTT", "publish failed (%u/%u)",
                    mqtt_fail_count_, Config::MQTT_MAX_FAILS);

        if (mqtt_fail_count_ >= Config::MQTT_MAX_FAILS) {
            LOGW("MQTT", "too many failures, disconnecting");
            client_.disconnect();
            mqtt_fail_count_ = 0;
        }
    }
}

bool MqttManager::connectClient(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on) {
    if (!mqtt_enabled_ || mqtt_host_.isEmpty()) {
        return false;
    }
    if (mqtt_retry_exhausted_) {
        return false;
    }

    auto note_connect_failure = [&](int rc, bool log_details) {
        if (mqtt_connect_attempts_ < UINT32_MAX) {
            mqtt_connect_attempts_++;
        }
        if (mqtt_connect_attempts_ >= kMqttRetryMaxAttempts) {
            mqtt_retry_exhausted_ = true;
            mqtt_connect_fail_count_ = Config::MQTT_CONNECT_MAX_FAILS;
            LOGW("MQTT", "retries exhausted, manual reconnect required");
            ui_dirty_ = true;
            return;
        }
        if (log_details) {
            uint8_t stage = retry_stage_for_attempts(mqtt_connect_attempts_);
            uint32_t delay_ms = retry_delay_for_stage(stage);
            Logger::log(Logger::Warn, "MQTT",
                        "connect failed rc=%d (attempt %lu/%u), retry in %s",
                        rc,
                        static_cast<unsigned long>(mqtt_connect_attempts_),
                        static_cast<unsigned>(kMqttRetryMaxAttempts),
                        retry_delay_label(delay_ms));
        }
        ui_dirty_ = true;
    };

    if (!mqtt_anonymous_ && (mqtt_user_.isEmpty() || mqtt_pass_.isEmpty())) {
        LOGW("MQTT", "credentials missing and anonymous mode is OFF, connection disabled");
        note_connect_failure(-1, false);
        return false;
    }

    // Diagnostics: check network state before MQTT connect.
    bool network_ready = network_ && network_->isEnabled() && network_->isConnected();
    wl_status_t wifi_status = WiFi.status();
    bool wifi_connected = (wifi_status == WL_CONNECTED);
    IPAddress local_ip = WiFi.localIP();
    int32_t rssi = WiFi.RSSI();

    Logger::log(Logger::Info, "MQTT",
                "connecting to %s:%u (NetworkMgr=%s, WiFi.status=%d, IP=%s, RSSI=%ld dBm)",
                mqtt_host_.c_str(),
                static_cast<unsigned>(mqtt_port_),
                network_ready ? "ready" : "NOT READY",
                static_cast<int>(wifi_status),
                local_ip.toString().c_str(),
                static_cast<long>(rssi));

    String client_id = mqtt_device_id_;
    String will_topic = availabilityTopic();
    bool ok = false;
    if (mqtt_anonymous_) {
        ok = client_.connect(client_id.c_str(), nullptr, nullptr,
                             will_topic.c_str(), 0, true, Config::MQTT_AVAIL_OFFLINE);
    } else if (mqtt_user_.length()) {
        ok = client_.connect(client_id.c_str(), mqtt_user_.c_str(), mqtt_pass_.c_str(),
                             will_topic.c_str(), 0, true, Config::MQTT_AVAIL_OFFLINE);
    } else {
        ok = client_.connect(client_id.c_str(), nullptr, nullptr,
                             will_topic.c_str(), 0, true, Config::MQTT_AVAIL_OFFLINE);
    }
    if (!ok) {
        note_connect_failure(client_.state(), true);
        return false;
    }
    LOGI("MQTT", "connected");
    mqtt_fail_count_ = 0;
    mqtt_connect_fail_count_ = 0;
    mqtt_connect_attempts_ = 0;
    mqtt_retry_exhausted_ = false;
    ui_dirty_ = true;
    client_.subscribe((mqtt_base_topic_ + "/command/#").c_str());
    client_.publish(will_topic.c_str(), Config::MQTT_AVAIL_ONLINE, true);
    publishNightModeAvailability();
    mqtt_discovery_sent_ = false;
    publishDiscovery();
    publishState(data, night_mode, alert_blink, backlight_on);
    return true;
}

bool MqttManager::payloadIsOn(const String &payload) {
    if (payload.equalsIgnoreCase("ON") || payload == "1" || payload.equalsIgnoreCase("TRUE") ||
        payload.equalsIgnoreCase("PRESS")) {
        return true;
    }
    return false;
}

bool MqttManager::payloadIsOff(const String &payload) {
    if (payload.equalsIgnoreCase("OFF") || payload == "0" || payload.equalsIgnoreCase("FALSE")) {
        return true;
    }
    return false;
}

void MqttManager::handleCallback(char *topic, uint8_t *payload, unsigned int length) {
    String t(topic ? topic : "");
    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; ++i) {
        msg += static_cast<char>(payload[i]);
    }
    msg.trim();
    if (!t.startsWith(mqtt_base_topic_)) {
        return;
    }
    String suffix = t.substring(mqtt_base_topic_.length());
    if (!suffix.startsWith("/command/")) {
        return;
    }
    String cmd = suffix.substring(strlen("/command/"));
    bool is_on = payloadIsOn(msg);
    bool is_off = payloadIsOff(msg);

    if (cmd == "night_mode") {
        if (auto_night_enabled_) {
            LOGI("MQTT", "night mode ignored (auto night enabled)");
            return;
        }
        if (is_on || is_off) {
            pending_.night_mode_value = is_on;
            pending_.night_mode = true;
        }
    } else if (cmd == "alert_blink") {
        if (is_on || is_off) {
            pending_.alert_blink_value = is_on;
            pending_.alert_blink = true;
        }
    } else if (cmd == "backlight") {
        if (is_on || is_off) {
            pending_.backlight_value = is_on;
            pending_.backlight = true;
        }
    } else if (cmd == "restart") {
        if (is_on) {
            pending_.restart = true;
        }
    }
}

void MqttManager::staticCallback(char *topic, uint8_t *payload, unsigned int length) {
    if (g_mqtt) {
        g_mqtt->handleCallback(topic, payload, length);
    }
}

float MqttManager::computeDewPointC(float temp_c, float rh) {
    if (!isfinite(temp_c) || !isfinite(rh) || rh <= 0.0f) {
        return NAN;
    }
    float rh_clamped = fminf(fmaxf(rh, 1.0f), 100.0f);
    constexpr float kA = 17.62f;
    constexpr float kB = 243.12f;
    float gamma = logf(rh_clamped / 100.0f) + (kA * temp_c) / (kB + temp_c);
    return (kB * gamma) / (kA - gamma);
}

void MqttManager::poll(const SensorData &data, bool night_mode, bool alert_blink, bool backlight_on) {
    if (!mqtt_enabled_) {
        if (client_.connected()) {
            String topic = availabilityTopic();
            client_.publish(topic.c_str(), Config::MQTT_AVAIL_OFFLINE, true);
            client_.disconnect();
            mqtt_fail_count_ = 0;
            if (!mqtt_user_enabled_) {
                mqtt_connect_fail_count_ = 0;
                mqtt_connect_attempts_ = 0;
                mqtt_retry_exhausted_ = false;
            }
        }
        if (mqtt_connected_last_) {
            mqtt_connected_last_ = false;
            ui_dirty_ = true;
        }
        return;
    }
    if (!network_ || !network_->isConnected()) {
        if (client_.connected()) {
            LOGW("MQTT", "network unavailable, disconnecting gracefully");
            String topic = availabilityTopic();
            client_.publish(topic.c_str(), Config::MQTT_AVAIL_OFFLINE, true);
            client_.disconnect();
            mqtt_fail_count_ = 0;
        }
        if (mqtt_connected_last_) {
            mqtt_connected_last_ = false;
            ui_dirty_ = true;
            LOGI("MQTT", "marked as disconnected (network unavailable)");
        }
        return;
    }

    client_.loop();
    bool connected = client_.connected();
    if (connected != mqtt_connected_last_) {
        mqtt_connected_last_ = connected;
        ui_dirty_ = true;
    }
    if (!connected) {
        if (mqtt_retry_exhausted_) {
            return;
        }
        uint32_t now = millis();
        if (mqtt_connect_attempts_ >= kMqttRetryMaxAttempts) {
            mqtt_retry_exhausted_ = true;
            mqtt_connect_fail_count_ = Config::MQTT_CONNECT_MAX_FAILS;
            ui_dirty_ = true;
            return;
        }
        uint8_t stage = retry_stage_for_attempts(mqtt_connect_attempts_);
        uint32_t retry_delay = retry_delay_for_stage(stage);
        if (now - mqtt_last_attempt_ms_ >= retry_delay) {
            mqtt_last_attempt_ms_ = now;
            connectClient(data, night_mode, alert_blink, backlight_on);
        }
        return;
    }
    publishDiscovery();
    uint32_t now = millis();
    if (mqtt_publish_requested_ || (now - mqtt_last_publish_ms_ >= Config::MQTT_PUBLISH_MS)) {
        mqtt_publish_requested_ = false;
        publishState(data, night_mode, alert_blink, backlight_on);
    }
}

void MqttManager::syncWithWifi() {
    bool wifi_ready = network_ && network_->isEnabled() && network_->isConnected();
    bool desired = mqtt_user_enabled_ && wifi_ready;
    if (desired != mqtt_enabled_) {
        mqtt_enabled_ = desired;
        if (mqtt_enabled_) {
            mqtt_fail_count_ = 0;
            setupClient();
            if (!mqtt_retry_exhausted_) {
                mqtt_connect_fail_count_ = 0;
                mqtt_last_attempt_ms_ = 0;
            }
        } else {
            if (client_.connected()) {
                if (wifi_ready) {
                    String topic = availabilityTopic();
                    client_.publish(topic.c_str(), Config::MQTT_AVAIL_OFFLINE, true);
                }
                client_.disconnect();
            }
            mqtt_fail_count_ = 0;
            if (!mqtt_user_enabled_) {
                mqtt_connect_fail_count_ = 0;
                mqtt_connect_attempts_ = 0;
                mqtt_retry_exhausted_ = false;
            }
        }
    }
    ui_dirty_ = true;
}

void MqttManager::requestReconnect() {
    LOGI("MQTT", "manual reconnect requested");
    mqtt_connect_fail_count_ = 0;
    mqtt_connect_attempts_ = 0;
    mqtt_retry_exhausted_ = false;
    mqtt_last_attempt_ms_ = 0;
    if (client_.connected()) {
        client_.disconnect();
    }
    ui_dirty_ = true;
}

void MqttManager::requestPublish() {
    mqtt_publish_requested_ = true;
}

void MqttManager::setUserEnabled(bool enabled) {
    if (mqtt_user_enabled_ == enabled) {
        return;
    }
    mqtt_user_enabled_ = enabled;
    mqtt_connect_fail_count_ = 0;
    mqtt_connect_attempts_ = 0;
    mqtt_retry_exhausted_ = false;
    if (storage_) {
        storage_->saveMqttEnabled(mqtt_user_enabled_);
    }
}

void MqttManager::updateNightModeAvailability(bool auto_night_enabled) {
    auto_night_enabled_ = auto_night_enabled;
    publishNightModeAvailability();
}

bool MqttManager::takePending(PendingCommands &out) {
    if (!pending_.night_mode && !pending_.alert_blink && !pending_.backlight && !pending_.restart) {
        return false;
    }
    out = pending_;
    pending_ = PendingCommands{};
    return true;
}
