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
#else
#include <map>
#include <string>
#include <vector>
#endif

namespace {

struct TextSnapshot {
    bool exists = false;
    String text;
};

int16_t clampPressureAltitudeM(int value) {
    const int min_value = static_cast<int>(Config::PRESSURE_ALTITUDE_MIN_M);
    const int max_value = static_cast<int>(Config::PRESSURE_ALTITUDE_MAX_M);
    if (value < min_value) {
        return static_cast<int16_t>(min_value);
    }
    if (value > max_value) {
        return static_cast<int16_t>(max_value);
    }
    return static_cast<int16_t>(value);
}

#ifdef UNIT_TEST
std::map<std::string, std::vector<uint8_t>> g_blob_store;
std::map<std::string, Config::StoredConfig> g_config_records;
bool g_force_save_failure = false;
uint32_t g_last_good_commit_failures_remaining = 0;
uint32_t g_last_good_commit_attempts = 0;
bool g_preserve_test_persistence_for_next_begin = false;
#endif

#ifndef UNIT_TEST
bool recoverFileAtomic(const char *final_path) {
    if (!final_path || LittleFS.exists(final_path)) {
        return final_path && LittleFS.exists(final_path);
    }
    String backup = String(final_path) + ".bak";
    return LittleFS.exists(backup) && LittleFS.rename(backup, final_path);
}

bool replaceFileAtomic(const char *tmp_path, const char *final_path) {
    String backup = String(final_path) + ".bak";
    if (!LittleFS.exists(final_path) && LittleFS.exists(backup) &&
        !LittleFS.rename(backup, final_path)) {
        LittleFS.remove(tmp_path);
        return false;
    }
    if (LittleFS.exists(backup)) {
        LittleFS.remove(backup);
    }
    if (LittleFS.exists(final_path)) {
        if (!LittleFS.rename(final_path, backup)) {
            LittleFS.remove(tmp_path);
            return false;
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
    if (!recoverFileAtomic(src_path)) {
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
#else
bool recoverFileAtomic(const char *final_path) {
    if (!final_path) {
        return false;
    }
    const std::string final_key(final_path);
    if (g_blob_store.find(final_key) != g_blob_store.end()) {
        return true;
    }
    const std::string backup_key = final_key + ".bak";
    auto backup = g_blob_store.find(backup_key);
    if (backup == g_blob_store.end()) {
        return false;
    }
    g_blob_store[final_key] = std::move(backup->second);
    g_blob_store.erase(backup);
    return true;
}

bool replaceFileAtomic(const char *tmp_path, const char *final_path) {
    if (!tmp_path || !final_path) {
        return false;
    }
    const std::string tmp_key(tmp_path);
    const std::string final_key(final_path);
    const std::string backup_key = final_key + ".bak";
    auto tmp = g_blob_store.find(tmp_key);
    if (tmp == g_blob_store.end()) {
        return false;
    }
    g_blob_store.erase(backup_key);
    auto final = g_blob_store.find(final_key);
    if (final != g_blob_store.end()) {
        g_blob_store[backup_key] = std::move(final->second);
        g_blob_store.erase(final);
    }
    g_blob_store[final_key] = std::move(tmp->second);
    g_blob_store.erase(tmp);
    g_blob_store.erase(backup_key);
    return true;
}
#endif

TextSnapshot captureText(StorageManager &storage, const char *path) {
    TextSnapshot snapshot{};
    snapshot.exists = storage.loadText(path, snapshot.text);
    return snapshot;
}

bool saveOrRemoveText(StorageManager &storage, const char *path, const String &text) {
    if (text.length() > 0) {
        return storage.saveTextAtomic(path, text);
    }
    String existing;
    if (!storage.loadText(path, existing)) {
        return true;
    }
    return storage.removeBlob(path);
}

void restoreText(StorageManager &storage, const char *path, const TextSnapshot &snapshot) {
    if (snapshot.exists) {
        storage.saveTextAtomic(path, snapshot.text);
        return;
    }
    String existing;
    if (storage.loadText(path, existing)) {
        storage.removeBlob(path);
    }
}

} // namespace

void StorageManager::begin(BootAction action) {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
    config_ = Config::StoredConfig{};
    lkg_pending_ = false;
    resetLastGoodHealthProof();
    mounted_ = false;
    config_loaded_ = false;
#ifdef UNIT_TEST
    if (g_preserve_test_persistence_for_next_begin) {
        g_preserve_test_persistence_for_next_begin = false;
        g_force_save_failure = false;
        g_last_good_commit_failures_remaining = 0;
        g_last_good_commit_attempts = 0;
    } else {
        resetTestPersistence();
    }
    mounted_ = true;
#else
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOGE("Storage", "LittleFS mount failed");
        return;
    }
    mounted_ = true;
#endif

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
    if (loadConfig()) {
        startLastGoodCandidate();
    }
}

const Config::StoredConfig &StorageManager::config() const {
    return config_;
}

Config::StoredConfig &StorageManager::config() {
    return config_;
}

bool StorageManager::saveConfig(bool force) {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
    if (!force) {
        markDirty();
        return true;
    }
    return saveConfigInternal();
}

void StorageManager::requestSave() {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
    markDirty();
}

void StorageManager::observeLastGoodHealth(uint32_t now_ms,
                                           OperationalHealth health) {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
    if (!lkg_pending_) {
        return;
    }

    if (health == OperationalHealth::Unhealthy) {
        resetLastGoodHealthProof();
        return;
    }
    if (health == OperationalHealth::Unavailable) {
        lkg_health_window_active_ = false;
        return;
    }

    if (!lkg_health_window_active_) {
        lkg_health_window_active_ = true;
        lkg_last_health_sample_ms_ = now_ms;
        return;
    }

    const uint32_t elapsed_ms =
        static_cast<uint32_t>(now_ms - lkg_last_health_sample_ms_);
    lkg_last_health_sample_ms_ = now_ms;
    if (elapsed_ms > Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS) {
        return;
    }

    const uint32_t remaining_ms =
        Config::LAST_GOOD_COMMIT_DELAY_MS - lkg_healthy_accumulated_ms_;
    if (elapsed_ms >= remaining_ms) {
        lkg_healthy_accumulated_ms_ = Config::LAST_GOOD_COMMIT_DELAY_MS;
    } else {
        lkg_healthy_accumulated_ms_ += elapsed_ms;
    }
}

void StorageManager::poll(uint32_t now_ms) {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
    if (dirty_) {
        if (static_cast<uint32_t>(now_ms - last_save_ms_) < debounce_ms_) {
            return;
        }
        if (!saveConfigInternal()) {
            return;
        }
    }

    if (!lkg_pending_ ||
        !lkg_health_window_active_ ||
        lkg_healthy_accumulated_ms_ < Config::LAST_GOOD_COMMIT_DELAY_MS ||
        !lastGoodRetryReady(now_ms)) {
        return;
    }

    if (commitLastGood()) {
        LOGI("Storage", "config committed as last known good");
        lkg_pending_ = false;
        resetLastGoodHealthProof();
        return;
    }

    LOGW("Storage", "last known good commit failed");
    lkg_retry_started_ms_ = now_ms;
    if (lkg_retry_delay_ms_ == 0) {
        lkg_retry_delay_ms_ = Config::LAST_GOOD_RETRY_INITIAL_DELAY_MS;
    } else if (lkg_retry_delay_ms_ >=
               Config::LAST_GOOD_RETRY_MAX_DELAY_MS / 2U) {
        lkg_retry_delay_ms_ = Config::LAST_GOOD_RETRY_MAX_DELAY_MS;
    } else {
        lkg_retry_delay_ms_ *= 2U;
    }
}

void StorageManager::clearAll() {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
#ifndef UNIT_TEST
    removeBlob(kConfigPath);
    removeBlob(kLastGoodPath);
    removeBlob(kVocStatePath);
    removeBlob(kPressurePath);
    removeBlob(kChartsPath);
    removeBlob(kDacAutoPath);
    removeBlob(kDisplayThresholdsPath);
    removeBlob(kMqttCaCertPath);
    removeBlob(kWifiEapCaCertPath);
    removeBlob(kWifiEapClientCertPath);
    removeBlob(kWifiEapClientKeyPath);
#else
    g_blob_store.clear();
    g_config_records.clear();
#endif
    config_ = Config::StoredConfig{};
    dirty_ = false;
    last_save_ms_ = 0;
    lkg_pending_ = false;
    resetLastGoodHealthProof();
    config_loaded_ = false;
}

bool StorageManager::commitLastGood() {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
#ifndef UNIT_TEST
    if (!recoverFileAtomic(kConfigPath)) {
        return false;
    }
    return copyFileAtomic(kConfigPath, kLastGoodPath);
#else
    ++g_last_good_commit_attempts;
    if (g_last_good_commit_failures_remaining > 0) {
        --g_last_good_commit_failures_remaining;
        return false;
    }
    auto it = g_config_records.find(kConfigPath);
    if (it == g_config_records.end()) {
        return false;
    }
    g_config_records[kLastGoodPath] = it->second;
    return true;
#endif
}

bool StorageManager::restoreLastGood() {
    const std::lock_guard<std::recursive_mutex> lock(persistence_mutex_);
#ifndef UNIT_TEST
    if (!recoverFileAtomic(kLastGoodPath)) {
        return false;
    }
    return copyFileAtomic(kLastGoodPath, kConfigPath);
#else
    auto it = g_config_records.find(kLastGoodPath);
    if (it == g_config_records.end()) {
        return false;
    }
    g_config_records[kConfigPath] = it->second;
    return true;
#endif
}

void StorageManager::loadWiFiSettings(String &ssid, String &pass, bool &enabled) {
    ssid = config_.wifi_ssid;
    pass = config_.wifi_pass;
    enabled = config_.wifi_enabled;
}

void StorageManager::loadWiFiSettings(Config::WifiSettings &settings) {
    settings.ssid = config_.wifi_ssid;
    settings.pass = config_.wifi_pass;
    settings.enabled = config_.wifi_enabled;
    settings.auth_mode = config_.wifi_auth_mode;
    settings.eap_method = config_.wifi_eap_method;
    settings.ttls_phase2 = config_.wifi_ttls_phase2;
    settings.identity = config_.wifi_identity;
    settings.username = config_.wifi_username;
    settings.enterprise_password = config_.wifi_enterprise_pass;
    loadWifiEapCaCertificate(settings.ca_cert_pem);
    loadWifiEapClientCertificate(settings.client_cert_pem);
    loadWifiEapClientKey(settings.client_key_pem);
}

bool StorageManager::saveWiFiSettings(const String &ssid, const String &pass, bool enabled) {
    Config::WifiSettings settings{};
    settings.ssid = ssid;
    settings.pass = pass;
    settings.enabled = enabled;
    settings.auth_mode = Config::WifiAuthMode::Personal;
    return saveWiFiSettings(settings);
}

bool StorageManager::saveWiFiSettings(const Config::WifiSettings &settings) {
    const Config::StoredConfig previous = config_;
    const TextSnapshot previous_ca = captureText(*this, kWifiEapCaCertPath);
    const TextSnapshot previous_client_cert = captureText(*this, kWifiEapClientCertPath);
    const TextSnapshot previous_client_key = captureText(*this, kWifiEapClientKeyPath);

    if (!saveOrRemoveText(*this, kWifiEapCaCertPath, settings.ca_cert_pem) ||
        !saveOrRemoveText(*this, kWifiEapClientCertPath, settings.client_cert_pem) ||
        !saveOrRemoveText(*this, kWifiEapClientKeyPath, settings.client_key_pem)) {
        restoreText(*this, kWifiEapCaCertPath, previous_ca);
        restoreText(*this, kWifiEapClientCertPath, previous_client_cert);
        restoreText(*this, kWifiEapClientKeyPath, previous_client_key);
        LOGE("Storage", "failed to persist WiFi EAP certificates");
        return false;
    }

    config_.wifi_ssid = settings.ssid;
    config_.wifi_pass = settings.pass;
    config_.wifi_enabled = settings.enabled;
    config_.wifi_auth_mode = settings.auth_mode;
    config_.wifi_eap_method = settings.eap_method;
    config_.wifi_ttls_phase2 = settings.ttls_phase2;
    config_.wifi_identity = settings.identity;
    config_.wifi_username = settings.username;
    config_.wifi_enterprise_pass = settings.enterprise_password;
    if (!saveConfig(true)) {
        config_ = previous;
        restoreText(*this, kWifiEapCaCertPath, previous_ca);
        restoreText(*this, kWifiEapClientCertPath, previous_client_cert);
        restoreText(*this, kWifiEapClientKeyPath, previous_client_key);
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
    Config::WifiSettings empty_settings{};
    empty_settings.enabled = config_.wifi_enabled;
    if (!saveWiFiSettings(empty_settings)) {
        requestSave();
        LOGE("Storage", "failed to clear WiFi credentials");
    }
}

void StorageManager::loadMqttSettings(String &host, uint16_t &port, String &user, String &pass,
                                      String &base_topic, String &device_name,
                                      bool &user_enabled, bool &discovery, bool &anonymous,
                                      bool &tls_enabled) {
    host = config_.mqtt_host;
    port = config_.mqtt_port;
    user = config_.mqtt_user;
    pass = config_.mqtt_pass;
    base_topic = config_.mqtt_base_topic;
    device_name = config_.mqtt_device_name;
    user_enabled = config_.mqtt_user_enabled;
    discovery = config_.mqtt_discovery;
    anonymous = config_.mqtt_anonymous;
    tls_enabled = config_.mqtt_tls_enabled;
}

bool StorageManager::saveMqttSettings(const String &host, uint16_t port, const String &user,
                                      const String &pass, const String &base_topic,
                                      const String &device_name, bool discovery, bool anonymous,
                                      bool tls_enabled, const String &ca_cert_pem) {
    const Config::StoredConfig previous = config_;
    String previous_ca;
    const bool had_previous_ca = loadMqttCaCertificate(previous_ca);

    bool ca_persisted = false;
    if (ca_cert_pem.length() > 0) {
        ca_persisted = saveMqttCaCertificate(ca_cert_pem);
    } else {
        ca_persisted = removeMqttCaCertificate();
    }
    if (!ca_persisted) {
        LOGE("Storage", "failed to persist MQTT CA certificate");
        return false;
    }

    config_.mqtt_host = host;
    config_.mqtt_port = port;
    config_.mqtt_user = user;
    config_.mqtt_pass = pass;
    config_.mqtt_base_topic = base_topic;
    config_.mqtt_device_name = device_name;
    config_.mqtt_discovery = discovery;
    config_.mqtt_anonymous = anonymous;
    config_.mqtt_tls_enabled = tls_enabled;
    if (!saveConfig(true)) {
        config_ = previous;
        if (had_previous_ca) {
            saveMqttCaCertificate(previous_ca);
        } else {
            removeMqttCaCertificate();
        }
        LOGE("Storage", "failed to persist MQTT settings");
        return false;
    }
    return true;
}

bool StorageManager::loadMqttCaCertificate(String &out) const {
    out = "";
    return loadText(kMqttCaCertPath, out);
}

bool StorageManager::saveMqttCaCertificate(const String &pem) {
    return saveTextAtomic(kMqttCaCertPath, pem);
}

bool StorageManager::removeMqttCaCertificate() {
    String existing;
    if (!loadText(kMqttCaCertPath, existing)) {
        return true;
    }
    return removeBlob(kMqttCaCertPath);
}

bool StorageManager::loadWifiEapCaCertificate(String &out) const {
    out = "";
    return loadText(kWifiEapCaCertPath, out);
}

bool StorageManager::loadWifiEapClientCertificate(String &out) const {
    out = "";
    return loadText(kWifiEapClientCertPath, out);
}

bool StorageManager::loadWifiEapClientKey(String &out) const {
    out = "";
    return loadText(kWifiEapClientKeyPath, out);
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
    if (!path || !out || !recoverFileAtomic(path)) {
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
    if (!out || !recoverFileAtomic(path)) {
        return false;
    }
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

bool StorageManager::blobExists(const char *path) const {
    return recoverFileAtomic(path);
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
    if (g_force_save_failure) {
        return false;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
    const std::string tmp = std::string(path) + ".tmp";
    g_blob_store[tmp] = std::vector<uint8_t>(bytes, bytes + len);
    return replaceFileAtomic(tmp.c_str(), path);
#endif
}

bool StorageManager::removeBlob(const char *path) {
#ifndef UNIT_TEST
    if (!path) {
        return false;
    }
    bool removed = LittleFS.remove(path);
    removed = LittleFS.remove(String(path) + ".tmp") || removed;
    removed = LittleFS.remove(String(path) + ".bak") || removed;
    return removed;
#else
    if (!path) {
        return false;
    }
    const std::string final_key(path);
    bool removed = g_blob_store.erase(final_key) > 0;
    removed = g_blob_store.erase(final_key + ".tmp") > 0 || removed;
    removed = g_blob_store.erase(final_key + ".bak") > 0 || removed;
    return removed;
#endif
}

bool StorageManager::loadText(const char *path, String &out) const {
#ifndef UNIT_TEST
    if (!recoverFileAtomic(path)) {
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
    if (!recoverFileAtomic(path)) {
        return false;
    }
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
    const std::string tmp = std::string(path) + ".tmp";
    g_blob_store[tmp] = std::vector<uint8_t>(chars, chars + text.length());
    return replaceFileAtomic(tmp.c_str(), path);
#endif
}

bool StorageManager::loadConfig() {
#ifndef UNIT_TEST
    if (!recoverFileAtomic(kConfigPath)) {
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
        String auth_mode;
        readString(wifi, "auth_mode", auth_mode);
        loaded.wifi_auth_mode = Config::parseWifiAuthMode(auth_mode);
        String eap_method;
        readString(wifi, "eap_method", eap_method);
        loaded.wifi_eap_method = Config::parseWifiEapMethod(eap_method);
        String ttls_phase2;
        readString(wifi, "ttls_phase2", ttls_phase2);
        loaded.wifi_ttls_phase2 = Config::parseWifiTtlsPhase2(ttls_phase2);
        readString(wifi, "identity", loaded.wifi_identity);
        readString(wifi, "username", loaded.wifi_username);
        readString(wifi, "enterprise_pass", loaded.wifi_enterprise_pass);
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
        readValue(mqtt, "tls_enabled", loaded.mqtt_tls_enabled);
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
        int lang_raw = static_cast<int>(Config::Language::EN);
        readValue(ui, "lang", lang_raw);
        loaded.language = Config::clampLanguage(lang_raw);
        bool legacy_units_mdy = !loaded.units_c;
        readValue(ui, "units_mdy", legacy_units_mdy);
        Config::DateFormat legacy_date_format =
            legacy_units_mdy ? Config::DateFormat::MDY : Config::DateFormat::DMY;
        if (loaded.language == Config::Language::JA) {
            legacy_date_format = Config::DateFormat::ISO;
        }
        int date_format_raw = static_cast<int>(legacy_date_format);
        readValue(ui, "date_format", date_format_raw);
        loaded.date_format = Config::clampDateFormat(date_format_raw);
        readValue(ui, "night_mode", loaded.night_mode);
        readValue(ui, "header_status_enabled", loaded.header_status_enabled);
        readValue(ui, "led_indicators", loaded.led_indicators);
        readValue(ui, "alert_blink", loaded.alert_blink);
        readValue(ui, "screen_flip_180", loaded.screen_flip_180);
        readValue(ui, "asc_enabled", loaded.asc_enabled);
        readValue(ui, "pressure_altitude_set", loaded.pressure_altitude_set);
        readValue(ui, "pressure_altitude_m", loaded.pressure_altitude_m);
        readString(ui, "display_name", loaded.web_display_name);
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
        readString(time, "ntp_server", loaded.ntp_server);
        readString(time, "tz_name", loaded.tz_name);
        readValue(time, "tz_idx", loaded.tz_index);
        readValue(time, "format_24h", loaded.time_format_24h);
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

    const int16_t clamped_pressure_altitude_m = clampPressureAltitudeM(loaded.pressure_altitude_m);
    const bool pressure_altitude_clamped = (clamped_pressure_altitude_m != loaded.pressure_altitude_m);
    loaded.pressure_altitude_m = clamped_pressure_altitude_m;

    config_ = loaded;
    if (pressure_altitude_clamped) {
        markDirty();
    }
    config_loaded_ = true;
    return true;
#else
    const auto it = g_config_records.find(kConfigPath);
    if (it == g_config_records.end()) {
        config_loaded_ = false;
        return false;
    }
    config_ = it->second;
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
    wifi["auth_mode"] = Config::wifiAuthModeToString(config_.wifi_auth_mode);
    wifi["eap_method"] = Config::wifiEapMethodToString(config_.wifi_eap_method);
    wifi["ttls_phase2"] = Config::wifiTtlsPhase2ToString(config_.wifi_ttls_phase2);
    wifi["identity"] = config_.wifi_identity;
    wifi["username"] = config_.wifi_username;
    wifi["enterprise_pass"] = config_.wifi_enterprise_pass;

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
    mqtt["tls_enabled"] = config_.mqtt_tls_enabled;

    ArduinoJson::JsonObject ui = root["ui"].to<ArduinoJson::JsonObject>();
    ui["temp_offset"] = config_.temp_offset;
    ui["hum_offset"] = config_.hum_offset;
    ui["units_c"] = config_.units_c;
    ui["units_mdy"] = config_.date_format == Config::DateFormat::MDY;
    ui["date_format"] = static_cast<uint8_t>(config_.date_format);
    ui["night_mode"] = config_.night_mode;
    ui["header_status_enabled"] = config_.header_status_enabled;
    ui["led_indicators"] = config_.led_indicators;
    ui["alert_blink"] = config_.alert_blink;
    ui["screen_flip_180"] = config_.screen_flip_180;
    ui["asc_enabled"] = config_.asc_enabled;
    ui["pressure_altitude_set"] = config_.pressure_altitude_set;
    config_.pressure_altitude_m = clampPressureAltitudeM(config_.pressure_altitude_m);
    ui["pressure_altitude_m"] = config_.pressure_altitude_m;
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
    time["ntp_server"] = config_.ntp_server;
    time["tz_name"] = config_.tz_name;
    time["tz_idx"] = config_.tz_index;
    time["format_24h"] = config_.time_format_24h;
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
    startLastGoodCandidate();
    return true;
#else
    if (g_force_save_failure) {
        return false;
    }
    g_config_records[kConfigPath] = config_;
    last_save_ms_ = millis();
    dirty_ = false;
    config_loaded_ = true;
    startLastGoodCandidate();
    return true;
#endif
}

#ifdef UNIT_TEST
void StorageManager::resetTestPersistence() {
    g_blob_store.clear();
    g_config_records.clear();
    g_force_save_failure = false;
    g_last_good_commit_failures_remaining = 0;
    g_last_good_commit_attempts = 0;
    g_preserve_test_persistence_for_next_begin = false;
}

void StorageManager::preserveTestPersistenceForNextBegin() {
    g_preserve_test_persistence_for_next_begin = true;
}

void StorageManager::setTestForceSaveFailure(bool enabled) {
    g_force_save_failure = enabled;
}

void StorageManager::setTestLastGoodCommitFailureCount(uint32_t count) {
    g_last_good_commit_failures_remaining = count;
}

uint32_t StorageManager::testLastGoodCommitAttemptCount() {
    return g_last_good_commit_attempts;
}

bool StorageManager::getTestStoredConfig(const char *path,
                                         Config::StoredConfig &out) {
    if (!path) {
        return false;
    }
    const auto it = g_config_records.find(path);
    if (it == g_config_records.end()) {
        return false;
    }
    out = it->second;
    return true;
}
#endif

void StorageManager::markDirty() {
    dirty_ = true;
}

void StorageManager::startLastGoodCandidate() {
    lkg_pending_ = true;
    resetLastGoodHealthProof();
}

void StorageManager::resetLastGoodHealthProof() {
    lkg_health_window_active_ = false;
    lkg_healthy_accumulated_ms_ = 0;
    lkg_last_health_sample_ms_ = 0;
    lkg_retry_started_ms_ = 0;
    lkg_retry_delay_ms_ = 0;
}

bool StorageManager::lastGoodRetryReady(uint32_t now_ms) const {
    return lkg_retry_delay_ms_ == 0 ||
           static_cast<uint32_t>(now_ms - lkg_retry_started_ms_) >=
               lkg_retry_delay_ms_;
}
