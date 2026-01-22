// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>
#include "config/AppConfig.h"

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
    void poll(uint32_t now_ms);
    void clearAll();
    bool commitLastGood();
    bool restoreLastGood();
    bool isMounted() const { return mounted_; }
    bool isConfigLoaded() const { return config_loaded_; }

    void loadWiFiSettings(String &ssid, String &pass, bool &enabled);
    void saveWiFiSettings(const String &ssid, const String &pass, bool enabled);
    void saveWiFiEnabled(bool enabled);
    void clearWiFiCredentials();

    void loadMqttSettings(String &host, uint16_t &port, String &user, String &pass,
                          String &base_topic, String &device_name,
                          bool &user_enabled, bool &discovery, bool &anonymous);
    void saveMqttSettings(const String &host, uint16_t port, const String &user, const String &pass,
                          const String &base_topic, const String &device_name, bool discovery,
                          bool anonymous);
    void saveMqttEnabled(bool enabled);

    bool loadVocState(uint8_t *out, size_t len) const;
    bool saveVocState(const uint8_t *data, size_t len);
    void clearVocState();

    bool loadBlob(const char *path, void *out, size_t len) const;
    bool saveBlobAtomic(const char *path, const void *data, size_t len);
    bool removeBlob(const char *path);

    static constexpr const char *kConfigPath = "/config.json";
    static constexpr const char *kLastGoodPath = "/config.last_good.json";
    static constexpr const char *kVocStatePath = "/voc_state.bin";
    static constexpr const char *kPressurePath = "/pressure.bin";

private:
    bool loadConfig();
    bool saveConfigInternal();
    void markDirty();

    Config::StoredConfig config_{}; 
    bool dirty_ = false;
    uint32_t last_save_ms_ = 0;
    uint32_t debounce_ms_ = 1000;
    bool lkg_pending_ = false;
    uint32_t lkg_start_ms_ = 0;
    bool mounted_ = false;
    bool config_loaded_ = false;
};
