#include "StorageManager.h"

#include <string.h>
#include "core/Logger.h"

#ifndef UNIT_TEST
#include <LittleFS.h>
#include <ArduinoJson.h>
#else
#include <map>
#include <string>
#include <vector>
#endif

namespace {

#ifdef UNIT_TEST
std::map<std::string, std::vector<uint8_t>> g_blob_store;
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
        return;
    }
    mounted_ = true;
    if (action == BootAction::SafeRollback) {
        if (!restoreLastGood()) {
            LOGW("Storage", "last good config missing, factory reset");
            clearAll();
        } else {
            LOGW("Storage", "restored last good config");
        }
    } else if (action == BootAction::SafeFactoryReset) {
        LOGW("Storage", "factory reset requested");
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
            } else {
                LOGW("Storage", "last known good commit failed");
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

void StorageManager::saveWiFiSettings(const String &ssid, const String &pass, bool enabled) {
    config_.wifi_ssid = ssid;
    config_.wifi_pass = pass;
    config_.wifi_enabled = enabled;
    saveConfig(true);
}

void StorageManager::saveWiFiEnabled(bool enabled) {
    config_.wifi_enabled = enabled;
    saveConfig(true);
}

void StorageManager::clearWiFiCredentials() {
    config_.wifi_ssid = "";
    config_.wifi_pass = "";
    saveConfig(true);
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

void StorageManager::saveMqttSettings(const String &host, uint16_t port, const String &user,
                                      const String &pass, const String &base_topic,
                                      const String &device_name, bool discovery, bool anonymous) {
    config_.mqtt_host = host;
    config_.mqtt_port = port;
    config_.mqtt_user = user;
    config_.mqtt_pass = pass;
    config_.mqtt_base_topic = base_topic;
    config_.mqtt_device_name = device_name;
    config_.mqtt_discovery = discovery;
    config_.mqtt_anonymous = anonymous;
    saveConfig(true);
}

void StorageManager::saveMqttEnabled(bool enabled) {
    config_.mqtt_user_enabled = enabled;
    saveConfig(true);
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
        readValue(ui, "night_mode", loaded.night_mode);
        readValue(ui, "led_indicators", loaded.led_indicators);
        readValue(ui, "alert_blink", loaded.alert_blink);
        readValue(ui, "asc_enabled", loaded.asc_enabled);
    }

    ArduinoJson::JsonObject backlight = root["backlight"].as<ArduinoJson::JsonObject>();
    if (!backlight.isNull()) {
        readValue(backlight, "timeout_s", loaded.backlight_timeout_s);
        readValue(backlight, "schedule_enabled", loaded.backlight_schedule_enabled);
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
    ui["night_mode"] = config_.night_mode;
    ui["led_indicators"] = config_.led_indicators;
    ui["alert_blink"] = config_.alert_blink;
    ui["asc_enabled"] = config_.asc_enabled;

    ArduinoJson::JsonObject backlight = root["backlight"].to<ArduinoJson::JsonObject>();
    backlight["timeout_s"] = config_.backlight_timeout_s;
    backlight["schedule_enabled"] = config_.backlight_schedule_enabled;
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
    last_save_ms_ = millis();
    dirty_ = false;
    lkg_pending_ = true;
    lkg_start_ms_ = last_save_ms_;
    return true;
#endif
}

void StorageManager::markDirty() {
    dirty_ = true;
}
