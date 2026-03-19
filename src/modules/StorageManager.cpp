// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "StorageManager.h"

#include <string.h>
#include "core/Logger.h"

#ifndef UNIT_TEST
#include <LittleFS.h>
#include <ArduinoJson.h>
// [NEW] Only include in the real firmware build — not in native unit tests.
#include "modules/MqttManager.h"
#else
#include <map>
#include <string>
#include <vector>
#endif

namespace {

#ifdef UNIT_TEST
std::map<std::string, std::vector<uint8_t>> g_blob_store;
bool g_force_save_failure = false;
#endif

#ifndef UNIT_TEST
bool replaceFileAtomic(const char *tmp_path, const char *final_path) {
    String backup = String(final_path) + ".bak";
    if (LittleFS.exists(backup)) {
        LittleFS.remove(backup);
    }
    if (LittleFS.exists(final_path)) {
        if (!LittleFS.rename(final_path, backup)) {
            LittleFS.remove(final_path);
        }
    }
    if (!LittleFS.rename(tmp_path, final_path)) {
        if (LittleFS.exists(backup)) {
            LittleFS.rename(backup, final_path);
        }
        LittleFS.remove(tmp_path);
        return false;
    }
    if (LittleFS.exists(backup)) {
        LittleFS.remove(backup);
    }
    return true;
}

bool copyFileAtomic(const char *src_path, const char *dst_path) {
    if (!src_path || !dst_path) {
        return false;
    }
    File in = LittleFS.open(src_path, FILE_READ);
    if (!in) {
        return false;
    }
    String tmp = String(dst_path) + ".tmp";
    File out = LittleFS.open(tmp, FILE_WRITE);
    if (!out) {
        in.close();
        return false;
    }
    uint8_t buffer[512];
    while (in.available()) {
        size_t read = in.read(buffer, sizeof(buffer));
        if (read == 0) {
            break;
        }
        if (out.write(buffer, read) != read) {
            in.close();
            out.close();
            LittleFS.remove(tmp);
            return false;
        }
    }
    in.close();
    out.close();
    return replaceFileAtomic(tmp.c_str(), dst_path);
}

void readString(const ArduinoJson::JsonObject &obj, const char *key, String &out) {
    ArduinoJson::JsonVariantConst value = obj[key];
    if (value.isNull()) {
        return;
    }
    const char *value_str = value.as<const char *>();
    out = value_str ? value_str : "";
}

template <typename T>
void readValue(const ArduinoJson::JsonObject &obj, const char *key, T &out) {
    ArduinoJson::JsonVariantConst value = obj[key];
    if (value.isNull()) {
        return;
    }
    out = value.as<T>();
}
#endif

} // namespace

void StorageManager::begin(BootAction action) {
    config_ = Config::StoredConfig{};
    lkg_pending_ = false;
    lkg_start_ms_ = 0;
    mounted_ = false;
    config_loaded_ = false;
#ifdef UNIT_TEST
    g_force_save_failure = false;
    g_blob_store.clear();
    if (action == BootAction::SafeRollback) {
        restoreLastGood();
    } else if (action == BootAction::SafeFactoryReset) {
        clearAll();
    }
    mounted_ = true;
    config_loaded_ = true;
#else
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOGE("Storage", "LittleFS mount failed");
        // [NEW] Critical storage failure — always warn regardless of MQTT state.
        MqttPublishSystemEvent("STORAGE", "warning", "LittleFS mount failed");
        return;
    }
    mounted_ = true;
    if (action == BootAction::SafeRollback) {
        if (!restoreLastGood()) {
            LOGW("Storage", "last good config missing, factory reset");
            // [NEW] Safe rollback had no last-good config — mirrors web dashboard entry.
            MqttPublishSystemEvent("STORAGE", "warning", "last good config missing, factory reset");
            clearAll();
        } else {
            LOGW("Storage", "restored last good config");
            // [NEW] Successful safe rollback — matches "restored last good config" in Events tab.
            MqttPublishSystemEvent("STORAGE", "warning", "restored last good config");
        }
    } else if (action == BootAction::SafeFactoryReset) {
        LOGW("Storage", "factory reset requested");
        // [NEW] Factory reset — matches "factory reset requested" in Events tab.
        MqttPublishSystemEvent("STORAGE", "warning", "factory reset requested");
        clearAll();
    }
    bool loaded = loadConfig();
    if (loaded) {
        lkg_pending_ = true;
        lkg_start_ms_ = millis();
    }
#endif
}

const Config::StoredConfig &StorageManager::config() const {
    return config_;
}

Config::StoredConfig &StorageManager::config() {
    return config_;
}

bool StorageManager::saveConfig(bool force) {
    if (!force) {
        markDirty();
        return true;
    }
    return saveConfigInternal();
}

void StorageManager::requestSave() {
    markDirty();
}

void StorageManager::poll(uint32_t now_ms) {
    if (!dirty_) {
        if (lkg_pending_ &&
            (now_ms - lkg_start_ms_) >= Config::LAST_GOOD_COMMIT_DELAY_MS) {
            if (commitLastGood()) {
                LOGI("Storage", "config committed as last known good");
                // [NEW] Matches "config committed as last known good" in web dashboard Events tab.
#ifndef UNIT_TEST
                MqttPublishSystemEvent("STORAGE", "info", "config committed as last known good");
#endif
            } else {
                LOGW("Storage", "last known good commit failed");
                // [NEW] LKG commit failure — important for diagnosing boot-loop scenarios.
#ifndef UNIT_TEST
                MqttPublishSystemEvent("STORAGE", "warning", "last known good commit failed");
#endif
            }
            lkg_pending_ = false;
        }
        return;
    }
    if (now_ms - last_save_ms_ < debounce_ms_) {
        return;
    }
    saveConfigInternal();
}

void StorageManager::clearAll() {
#ifndef UNIT_TEST
    LittleFS.remove(kConfigPath);
    LittleFS.remove(kLastGoodPath);
    LittleFS.remove(kVocStatePath);
    LittleFS.remove(kPressurePath);
    LittleFS.remove(kChartsPath);
    LittleFS.remove(kDacAutoPath);
#else
    g_blob_store.clear();
#endif
    config_ = Config::StoredConfig{};
    dirty_ = false;
    last_save_ms_ = 0;
    lkg_pending_ = false;
    lkg_start_ms_ = 0;
    config_loaded_ = false;
}

bool StorageManager::commitLastGood() {
#ifndef UNIT_TEST
    if (!LittleFS.exists(kConfigPath)) {
        return false;
    }
    return copyFileAtomic(kConfigPath, kLastGoodPath);
#else
    auto it = g_blob_store.find(kConfigPath);
    if (it == g_blob_store.end()) {
        return false;
    }
    g_blob_store[kLastGoodPath] = it->second;
    return true;
#endif
}

bool StorageManager::restoreLastGood() {
#ifndef UNIT_TEST
    if (!LittleFS.exists(kLastGoodPath)) {
        return false;
    }
    return copyFileAtomic(kLastGoodPath, kConfigPath);
#else
    auto it = g_blob_store.find(kLastGoodPath);
    if (it == g_blob_store.end()) {
        return false;
    }
    g_blob_store[kConfigPath] = it->second;
    return true;
#endif
}

void StorageManager::loadWiFiSettings(String &ssid, String &pass, bool &enabled) {
    ssid = config_.wifi_ssid;
    pass = config_.wifi_pass;
    enabled = config_.wifi_enabled;
}

bool StorageManager::saveWiFiSettings(const String &ssid, const String &pass, bool enabled) {
    const Config::StoredConfig previous = config_;
    config_.wifi_ssid = ssid;
    config_.wifi_pass = pass;
    config_.wifi_enabled = enabled;
    if (!saveConfig(true)) {
        config_ = previous;
        LOGE("Storage", "failed to persist WiFi settings");
        return false;
    }
    return true;
}

void StorageManager::saveWiFiEnabled(bool enabled) {
    config_.wifi_enabled = enabled;
    if (!saveConfig(true)) {
        requestSave();
        LOGE("Storage", "failed to persist WiFi enabled state");
    }
}

void StorageManager::clearWiFiCredentials() {
    config_.wifi_ssid = "";
    config_.wifi_pass = "";
    if (!saveConfig(true)) {
        requestSave();
        LOGE("Storage", "failed to clear WiFi credentials");
    }
}

void StorageManager::loadMqttSettings(String &host, uint16_t &port, String &user, String &pass,
                                      String &base_topic, String &device_name,
                                      bool &user_enabled, bool &discovery, bool &anonymous) {
    host = config_.mqtt_host;
    port = config_.mqtt_port;
    user = config_.mqtt_user;
    pass = config_.mqtt_pass;
    base_topic = config_.mqtt_base_topic;
    device_name = config_.mqtt_device_name;
    user_enabled = config_.mqtt_user_enabled;
    discovery = config_.mqtt_discovery;
    anonymous = config_.mqtt_anonymous;
}

bool StorageManager::saveMqttSettings(const String &host, uint16_t port, const String &user,
                                      const String &pass, const String &base_topic,
                                      const String &device_name, bool discovery, bool anonymous) {
    const Config::StoredConfig previous = config_;
    config_.mqtt_host = host;
    config_.mqtt_port = port;
    config_.mqtt_user = user;
    config_.mqtt_pass = pass;
    config_.mqtt_base_topic = base_topic;
    config_.mqtt_device_name = device_name;
    config_.mqtt_discovery = discovery;
    config_.mqtt_anonymous = anonymous;
    if (!saveConfig(true)) {
        config_ = previous;
        LOGE("Storage", "failed to persist MQTT settings");
        return false;
    }
    return true;
}

void StorageManager::saveMqttEnabled(bool enabled) {
    config_.mqtt_user_enabled = enabled;
    if (!saveConfig(true)) {
        requestSave();
        LOGE("Storage", "failed to persist MQTT enabled state");
    }
}

bool StorageManager::saveRtcMode(Config::RtcMode mode) {
    const Config::RtcMode previous = config_.rtc_mode;
    config_.rtc_mode = mode;
    if (!saveConfig(true)) {
        config_.rtc_mode = previous;
        LOGE("Storage", "failed to persist RTC mode");
        return false;
    }
    return true;
}

bool StorageManager::saveDacAutoState(bool auto_mode, bool auto_armed) {
    const bool prev_mode = config_.dac_auto_mode;
    const bool prev_armed = config_.dac_auto_armed;

    config_.dac_auto_mode = auto_mode;
    config_.dac_auto_armed = auto_armed;

    if (!saveConfig(true)) {
        config_.dac_auto_mode = prev_mode;
        config_.dac_auto_armed = prev_armed;
        return false;
    }
    return true;
}

bool StorageManager::loadVocState(uint8_t *out, size_t len) const {
    return loadBlob(kVocStatePath, out, len);
}

bool StorageManager::saveVocState(const uint8_t *data, size_t len) {
    return saveBlobAtomic(kVocStatePath, data, len);
}

void StorageManager::clearVocState() {
    removeBlob(kVocStatePath);
}

bool StorageManager::loadBlob(const char *path, void *out, size_t len) const {
#ifndef UNIT_TEST
    if (!path || !out) {
        return false;
    }
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        return false;
    }
    if (static_cast<size_t>(file.size()) != len) {
        file.close();
        return false;
    }
    size_t read = file.readBytes(reinterpret_cast<char *>(out), len);
    file.close();
    return read == len;
#else
    auto it = g_blob_store.find(path ? path : "");
    if (it == g_blob_store.end()) {
        return false;
    }
    if (it->second.size() != len) {
        return false;
    }
    memcpy(out, it->second.data(), len);
    return true;
#endif
}

bool StorageManager::saveBlobAtomic(const char *path, const void *data, size_t len) {
#ifndef UNIT_TEST
    if (!path || !data) {
        return false;
    }
    String tmp = String(path) + ".tmp";
    File file = LittleFS.open(tmp, FILE_WRITE);
    if (!file) {
        return false;
    }
    size_t written = file.write(reinterpret_cast<const uint8_t *>(data), len);
    file.close();
    if (written != len) {
        LittleFS.remove(tmp);
        return false;
    }
    return replaceFileAtomic(tmp.c_str(), path);
#else
    if (!path || !data) {
        return false;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
    g_blob_store[path] = std::vector<uint8_t>(bytes, bytes + len);
    return true;
#endif
}

bool StorageManager::removeBlob(const char *path) {
#ifndef UNIT_TEST
    if (!path) {
        return false;
    }
    return LittleFS.remove(path);
#else
    if (!path) {
        return false;
    }
    return g_blob_store.erase(path) > 0;
#endif
}

bool StorageManager::loadText(const char *path, String &out) const {
#ifndef UNIT_TEST
    if (!path) {
        return false;
    }
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        return false;
    }
    out = file.readString();
    file.close();
    return true;
#else
    auto it = g_blob_store.find(path ? path : "");
    if (it == g_blob_store.end()) {
        return false;
    }
    out = String(reinterpret_cast<const char *>(it->second.data()), it->second.size());
    return true;
#endif
}

bool StorageManager::saveTextAtomic(const char *path, const String &text) {
#ifndef UNIT_TEST
    if (!path) {
        return false;
    }
    String tmp = String(path) + ".tmp";
    File file = LittleFS.open(tmp, FILE_WRITE);
    if (!file) {
        return false;
    }
    size_t written = file.print(text);
    file.close();
    if (written != text.length()) {
        LittleFS.remove(tmp);
        return false;
    }
    return replaceFileAtomic(tmp.c_str(), path);
#else
    if (!path) {
        return false;
    }
    const char *chars = text.c_str();
    g_blob_store[path] = std::vector<uint8_t>(chars, chars + text.length());
    return true;
#endif
}

bool StorageManager::loadConfig() {
#ifndef UNIT_TEST
    if (!LittleFS.exists(kConfigPath)) {
        LOGI("Storage", "config not found, using defaults");
        config_loaded_ = false;
        return false;
    }
    File file = LittleFS.open(kConfigPath, FILE_READ);
    if (!file) {
        LOGW("Storage", "config open failed");
        config_loaded_ = false;
        return false;
    }
#if ARDUINOJSON_VERSION_MAJOR >= 7
    ArduinoJson::JsonDocument doc;
#else
    ArduinoJson::DynamicJsonDocument doc(4096);
#endif
    ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, file);
    file.close();
    if (err) {
        LOGW("Storage", "config parse failed: %s", err.c_str());
        config_loaded_ = false;
        return false;
    }
    Config::StoredConfig loaded;
    ArduinoJson::JsonObject root = doc.as<ArduinoJson::JsonObject>();

    ArduinoJson::JsonObject wifi = root["wifi"].as<ArduinoJson::JsonObject>();
    if (!wifi.isNull()) {
        readValue(wifi, "enabled", loaded.wifi_enabled);
        readString(wifi, "ssid", loaded.wifi_ssid);
        readString(wifi, "pass", loaded.wifi_pass);
    }

    ArduinoJson::JsonObject mqtt = root["mqtt"].as<ArduinoJson::JsonObject>();
    if (!mqtt.isNull()) {
        readString(mqtt, "host", loaded.mqtt_host);
        readValue(mqtt, "port", loaded.mqtt_port);
        readString(mqtt, "user", loaded.mqtt_user);
        readString(mqtt, "pass", loaded.mqtt_pass);
        readString(mqtt, "base", loaded.mqtt_base_topic);
        readString(mqtt, "name", loaded.mqtt_device_name);
        readValue(mqtt, "enabled", loaded.mqtt_user_enabled);
        readValue(mqtt, "discovery", loaded.mqtt_discovery);
        readValue(mqtt, "anonymous", loaded.mqtt_anonymous);
        if (mqtt["anonymous"].isNull()) {
            loaded.mqtt_anonymous =
                (loaded.mqtt_user.length() == 0 && loaded.mqtt_pass.length() == 0);
        }
    }

    ArduinoJson::JsonObject ui = root["ui"].as<ArduinoJson::JsonObject>();
    if (!ui.isNull()) {
        readValue(ui, "temp_offset", loaded.temp_offset);
        readValue(ui, "hum_offset", loaded.hum_offset);
        readValue(ui, "units_c", loaded.units_c);
        readValue(ui, "units_mdy", loaded.units_mdy);
        readValue(ui, "night_mode", loaded.night_mode);
        readValue(ui, "header_status_enabled", loaded.header_status_enabled);
        readValue(ui, "led_indicators", loaded.led_indicators);
        readValue(ui, "alert_blink", loaded.alert_blink);
        readValue(ui, "asc_enabled", loaded.asc_enabled);
        readString(ui, "display_name", loaded.web_display_name);
        int lang_raw = static_cast<int>(Config::Language::EN);
        readValue(ui, "lang", lang_raw);
        loaded.language = Config::clampLanguage(lang_raw);
    }

    ArduinoJson::JsonObject backlight = root["backlight"].as<ArduinoJson::JsonObject>();
    if (!backlight.isNull()) {
        readValue(backlight, "timeout_s", loaded.backlight_timeout_s);
        readValue(backlight, "schedule_enabled", loaded.backlight_schedule_enabled);
        readValue(backlight, "alarm_wake", loaded.backlight_alarm_wake);
        readValue(backlight, "sleep_hour", loaded.backlight_sleep_hour);
        readValue(backlight, "sleep_minute", loaded.backlight_sleep_minute);
        readValue(backlight, "wake_hour", loaded.backlight_wake_hour);
        readValue(backlight, "wake_minute", loaded.backlight_wake_minute);
    }

    ArduinoJson::JsonObject auto_night = root["auto_night"].as<ArduinoJson::JsonObject>();
    if (!auto_night.isNull()) {
        readValue(auto_night, "enabled", loaded.auto_night_enabled);
        readValue(auto_night, "start_hour", loaded.auto_night_start_hour);
        readValue(auto_night, "start_minute", loaded.auto_night_start_minute);
        readValue(auto_night, "end_hour", loaded.auto_night_end_hour);
        readValue(auto_night, "end_minute", loaded.auto_night_end_minute);
    }

    ArduinoJson::JsonObject time = root["time"].as<ArduinoJson::JsonObject>();
    if (!time.isNull()) {
        readValue(time, "ntp_enabled", loaded.ntp_enabled);
        readValue(time, "tz_idx", loaded.tz_index);
        int rtc_mode_raw = static_cast<int>(Config::RtcMode::Auto);
        readValue(time, "rtc_mode", rtc_mode_raw);
        loaded.rtc_mode = Config::clampRtcMode(rtc_mode_raw);
    }

    ArduinoJson::JsonObject dac = root["dac"].as<ArduinoJson::JsonObject>();
    if (!dac.isNull()) {
        readValue(dac, "auto_mode", loaded.dac_auto_mode);
        readValue(dac, "auto_armed", loaded.dac_auto_armed);
    }

    ArduinoJson::JsonObject theme = root["theme"].as<ArduinoJson::JsonObject>();
    if (!theme.isNull()) {
        readValue(theme, "valid", loaded.theme.valid);
        readValue(theme, "screen_bg", loaded.theme.screen_bg);
        readValue(theme, "card_bg", loaded.theme.card_bg);
        readValue(theme, "card_border", loaded.theme.card_border);
        readValue(theme, "text_primary", loaded.theme.text_primary);
        readValue(theme, "shadow_color", loaded.theme.shadow_color);
        readValue(theme, "shadow_enabled", loaded.theme.shadow_enabled);
        readValue(theme, "gradient_enabled", loaded.theme.gradient_enabled);
        readValue(theme, "gradient_color", loaded.theme.gradient_color);
        readValue(theme, "gradient_direction", loaded.theme.gradient_direction);
        readValue(theme, "screen_gradient_enabled", loaded.theme.screen_gradient_enabled);
        readValue(theme, "screen_gradient_color", loaded.theme.screen_gradient_color);
        readValue(theme, "screen_gradient_direction", loaded.theme.screen_gradient_direction);
    }

    config_ = loaded;
    config_loaded_ = true;
    return true;
#else
    config_loaded_ = true;
    return true;
#endif
}

bool StorageManager::saveConfigInternal() {
#ifndef UNIT_TEST
#if ARDUINOJSON_VERSION_MAJOR >= 7
    ArduinoJson::JsonDocument doc;
#else
    ArduinoJson::DynamicJsonDocument doc(4096);
#endif
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();

    ArduinoJson::JsonObject wifi = root["wifi"].to<ArduinoJson::JsonObject>();
    wifi["enabled"] = config_.wifi_enabled;
    wifi["ssid"] = config_.wifi_ssid;
    wifi["pass"] = config_.wifi_pass;

    ArduinoJson::JsonObject mqtt = root["mqtt"].to<ArduinoJson::JsonObject>();
    mqtt["host"] = config_.mqtt_host;
    mqtt["port"] = config_.mqtt_port;
    mqtt["user"] = config_.mqtt_user;
    mqtt["pass"] = config_.mqtt_pass;
    mqtt["base"] = config_.mqtt_base_topic;
    mqtt["name"] = config_.mqtt_device_name;
    mqtt["enabled"] = config_.mqtt_user_enabled;
    mqtt["discovery"] = config_.mqtt_discovery;
    mqtt["anonymous"] = config_.mqtt_anonymous;

    ArduinoJson::JsonObject ui = root["ui"].to<ArduinoJson::JsonObject>();
    ui["temp_offset"] = config_.temp_offset;
    ui["hum_offset"] = config_.hum_offset;
    ui["units_c"] = config_.units_c;
    ui["units_mdy"] = config_.units_mdy;
    ui["night_mode"] = config_.night_mode;
    ui["header_status_enabled"] = config_.header_status_enabled;
    ui["led_indicators"] = config_.led_indicators;
    ui["alert_blink"] = config_.alert_blink;
    ui["asc_enabled"] = config_.asc_enabled;
    ui["display_name"] = config_.web_display_name;
    ui["lang"] = static_cast<uint8_t>(config_.language);

    ArduinoJson::JsonObject backlight = root["backlight"].to<ArduinoJson::JsonObject>();
    backlight["timeout_s"] = config_.backlight_timeout_s;
    backlight["schedule_enabled"] = config_.backlight_schedule_enabled;
    backlight["alarm_wake"] = config_.backlight_alarm_wake;
    backlight["sleep_hour"] = config_.backlight_sleep_hour;
    backlight["sleep_minute"] = config_.backlight_sleep_minute;
    backlight["wake_hour"] = config_.backlight_wake_hour;
    backlight["wake_minute"] = config_.backlight_wake_minute;

    ArduinoJson::JsonObject auto_night = root["auto_night"].to<ArduinoJson::JsonObject>();
    auto_night["enabled"] = config_.auto_night_enabled;
    auto_night["start_hour"] = config_.auto_night_start_hour;
    auto_night["start_minute"] = config_.auto_night_start_minute;
    auto_night["end_hour"] = config_.auto_night_end_hour;
    auto_night["end_minute"] = config_.auto_night_end_minute;

    ArduinoJson::JsonObject time = root["time"].to<ArduinoJson::JsonObject>();
    time["ntp_enabled"] = config_.ntp_enabled;
    time["tz_idx"] = config_.tz_index;
    time["rtc_mode"] = static_cast<uint8_t>(config_.rtc_mode);

    ArduinoJson::JsonObject dac = root["dac"].to<ArduinoJson::JsonObject>();
    dac["auto_mode"] = config_.dac_auto_mode;
    dac["auto_armed"] = config_.dac_auto_armed;

    ArduinoJson::JsonObject theme = root["theme"].to<ArduinoJson::JsonObject>();
    theme["valid"] = config_.theme.valid;
    theme["screen_bg"] = config_.theme.screen_bg;
    theme["card_bg"] = config_.theme.card_bg;
    theme["card_border"] = config_.theme.card_border;
    theme["text_primary"] = config_.theme.text_primary;
    theme["shadow_color"] = config_.theme.shadow_color;
    theme["shadow_enabled"] = config_.theme.shadow_enabled;
    theme["gradient_enabled"] = config_.theme.gradient_enabled;
    theme["gradient_color"] = config_.theme.gradient_color;
    theme["gradient_direction"] = config_.theme.gradient_direction;
    theme["screen_gradient_enabled"] = config_.theme.screen_gradient_enabled;
    theme["screen_gradient_color"] = config_.theme.screen_gradient_color;
    theme["screen_gradient_direction"] = config_.theme.screen_gradient_direction;

    String tmp = String(kConfigPath) + ".tmp";
    File file = LittleFS.open(tmp, FILE_WRITE);
    if (!file) {
        return false;
    }
    if (ArduinoJson::serializeJson(doc, file) == 0) {
        file.close();
        LittleFS.remove(tmp);
        return false;
    }
    file.close();
    if (!replaceFileAtomic(tmp.c_str(), kConfigPath)) {
        return false;
    }
    config_loaded_ = true;
    last_save_ms_ = millis();
    dirty_ = false;
    lkg_pending_ = true;
    lkg_start_ms_ = last_save_ms_;
    return true;
#else
    if (g_force_save_failure) {
        return false;
    }
    last_save_ms_ = millis();
    dirty_ = false;
    lkg_pending_ = true;
    lkg_start_ms_ = last_save_ms_;
    return true;
#endif
}

#ifdef UNIT_TEST
void StorageManager::setTestForceSaveFailure(bool enabled) {
    g_force_save_failure = enabled;
}
#endif

void StorageManager::markDirty() {
    dirty_ = true;
}
