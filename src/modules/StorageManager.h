// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>
#include <mutex>
#include "config/AppConfig.h"
#include "core/OperationalHealth.h"

class StorageManager {
public:
    enum class BootAction {
        Normal,
        SafeRollback,
        SafeFactoryReset
    };

    void begin(BootAction action = BootAction::Normal);
    const Config::StoredConfig &config() const;
    Config::StoredConfig &config();
    bool saveConfig(bool force = false);
    void requestSave();
    void observeLastGoodHealth(uint32_t now_ms, OperationalHealth health);
    void poll(uint32_t now_ms);
    void clearAll();
    bool isMounted() const { return mounted_; }
    bool isConfigLoaded() const { return config_loaded_; }

    void loadWiFiSettings(String &ssid, String &pass, bool &enabled);
    void loadWiFiSettings(Config::WifiSettings &settings);
    bool saveWiFiSettings(const String &ssid, const String &pass, bool enabled);
    bool saveWiFiSettings(const Config::WifiSettings &settings);
    void saveWiFiEnabled(bool enabled);
    void clearWiFiCredentials();

    void loadMqttSettings(String &host, uint16_t &port, String &user, String &pass,
                          String &base_topic, String &device_name,
                          bool &user_enabled, bool &discovery, bool &anonymous,
                          bool &tls_enabled);
    bool saveMqttSettings(const String &host, uint16_t port, const String &user, const String &pass,
                          const String &base_topic, const String &device_name, bool discovery,
                          bool anonymous, bool tls_enabled, const String &ca_cert_pem);
    void saveMqttEnabled(bool enabled);
    bool loadMqttCaCertificate(String &out) const;
    bool saveMqttCaCertificate(const String &pem);
    bool removeMqttCaCertificate();
    bool loadWifiEapCaCertificate(String &out) const;
    bool loadWifiEapClientCertificate(String &out) const;
    bool loadWifiEapClientKey(String &out) const;
    bool saveRtcMode(Config::RtcMode mode);
    bool saveDacAutoState(bool auto_mode, bool auto_armed);

    bool loadVocState(uint8_t *out, size_t len) const;
    bool saveVocState(const uint8_t *data, size_t len);
    void clearVocState();

    bool loadBlob(const char *path, void *out, size_t len) const;
    bool blobExists(const char *path) const;
    bool saveBlobAtomic(const char *path, const void *data, size_t len);
    bool removeBlob(const char *path);
    bool loadText(const char *path, String &out) const;
    bool saveTextAtomic(const char *path, const String &text);

#ifdef UNIT_TEST
    static void resetTestPersistence();
    static void preserveTestPersistenceForNextBegin();
    static void setTestForceSaveFailure(bool enabled);
    static void setTestLastGoodCommitFailureCount(uint32_t count);
    static uint32_t testLastGoodCommitAttemptCount();
    static bool getTestStoredConfig(const char *path, Config::StoredConfig &out);
#endif

    static constexpr const char *kConfigPath = "/config.json";
    static constexpr const char *kLastGoodPath = "/config.last_good.json";
    static constexpr const char *kVocStatePath = "/voc_state.bin";
    static constexpr const char *kPressurePath = "/pressure.bin";
    static constexpr const char *kChartsPath = "/charts.bin";
    static constexpr const char *kDacAutoPath = "/dac_auto.json";
    static constexpr const char *kDisplayThresholdsPath = "/display_thresholds.json";
    static constexpr const char *kMqttCaCertPath = "/mqtt_ca.pem";
    static constexpr const char *kWifiEapCaCertPath = "/wifi_eap_ca.pem";
    static constexpr const char *kWifiEapClientCertPath = "/wifi_eap_client.crt";
    static constexpr const char *kWifiEapClientKeyPath = "/wifi_eap_client.key";

private:
    bool loadConfig();
    bool saveConfigInternal();
    void markDirty();
    void startLastGoodCandidate();
    void resetLastGoodHealthProof();
    bool lastGoodRetryReady(uint32_t now_ms) const;
    bool commitLastGood();
    bool restoreLastGood();

    Config::StoredConfig config_{}; 
    bool dirty_ = false;
    uint32_t last_save_ms_ = 0;
    uint32_t debounce_ms_ = 1000;
    bool lkg_pending_ = false;
    bool lkg_health_window_active_ = false;
    uint32_t lkg_healthy_accumulated_ms_ = 0;
    uint32_t lkg_last_health_sample_ms_ = 0;
    uint32_t lkg_retry_started_ms_ = 0;
    uint32_t lkg_retry_delay_ms_ = 0;
    // The LKG candidate is the successfully persisted kConfigPath bytes, not
    // the current config() reference. This mutex serializes durable writes,
    // candidate resets, and promotion. Callers of config() still own
    // synchronization for the returned reference.
    mutable std::recursive_mutex persistence_mutex_;
    bool mounted_ = false;
    bool config_loaded_ = false;
};
