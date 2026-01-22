// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/NetworkManager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "ui/ThemeManager.h"

namespace {

AuraNetworkManager *g_network = nullptr;
const uint32_t kInitialWifiConnectDelayMs = 1000;

void network_wifi_start_scan() {
    if (g_network) {
        g_network->startScan();
    }
}

void network_wifi_start_sta() {
    if (g_network) {
        g_network->connectSta();
    }
}

bool network_wifi_is_connected() {
    return g_network && g_network->isConnected();
}

} // namespace

void AuraNetworkManager::begin(StorageManager &storage) {
    storage_ = &storage;
    g_network = this;
    WiFi.persistent(false);

    web_ctx_.server = &server_;
    web_ctx_.storage = storage_;
    web_ctx_.wifi_ssid = &wifi_ssid_;
    web_ctx_.wifi_pass = &wifi_pass_;
    web_ctx_.wifi_enabled = &wifi_enabled_;
    web_ctx_.wifi_enabled_dirty = &wifi_enabled_dirty_;
    web_ctx_.wifi_ui_dirty = &wifi_ui_dirty_;
    web_ctx_.wifi_scan_in_progress = &wifi_scan_in_progress_;
    web_ctx_.wifi_scan_options = &wifi_scan_options_;
    web_ctx_.wifi_is_connected = network_wifi_is_connected;
    web_ctx_.wifi_start_scan = network_wifi_start_scan;
    web_ctx_.wifi_start_sta = network_wifi_start_sta;
    web_ctx_.mqtt_ui_open = &mqtt_ui_open_;
    WebHandlersInit(&web_ctx_);

    storage_->loadWiFiSettings(wifi_ssid_, wifi_pass_, wifi_enabled_);
    wifi_enabled_dirty_ = false;
    if (!wifi_ssid_.isEmpty() && !wifi_is_ascii_printable(wifi_ssid_, 32)) {
        LOGW("WiFi", "SSID invalid, clearing saved credentials");
        storage_->clearWiFiCredentials();
        wifi_ssid_ = "";
        wifi_pass_ = "";
    }

    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            wifi_state_ = WIFI_STATE_OFF;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = millis() + kInitialWifiConnectDelayMs;
            wifi_connect_start_ms_ = 0;
            wifi_ui_dirty_ = true;
            Logger::log(Logger::Info, "WiFi",
                        "delaying initial connect %u ms",
                        static_cast<unsigned>(kInitialWifiConnectDelayMs));
        } else {
            startAp();
        }
    } else {
        warmupIfDisabled();
        WiFi.persistent(false);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
    }
    wifi_state_last_ = wifi_state_;
}

void AuraNetworkManager::attachMqttContext(PubSubClient &client,
                                       bool &mqtt_user_enabled,
                                       uint8_t &mqtt_connect_fail_count,
                                       String &mqtt_host,
                                       uint16_t &mqtt_port,
                                       String &mqtt_user,
                                       String &mqtt_pass,
                                       String &mqtt_device_name,
                                       String &mqtt_base_topic,
                                       String &mqtt_device_id,
                                       bool &mqtt_discovery,
                                       bool &mqtt_anonymous,
                                       void (*mqtt_sync_with_wifi)()) {
    web_ctx_.mqtt_client = &client;
    web_ctx_.mqtt_user_enabled = &mqtt_user_enabled;
    web_ctx_.mqtt_connect_fail_count = &mqtt_connect_fail_count;
    web_ctx_.mqtt_host = &mqtt_host;
    web_ctx_.mqtt_port = &mqtt_port;
    web_ctx_.mqtt_user = &mqtt_user;
    web_ctx_.mqtt_pass = &mqtt_pass;
    web_ctx_.mqtt_device_name = &mqtt_device_name;
    web_ctx_.mqtt_base_topic = &mqtt_base_topic;
    web_ctx_.mqtt_device_id = &mqtt_device_id;
    web_ctx_.mqtt_discovery = &mqtt_discovery;
    web_ctx_.mqtt_anonymous = &mqtt_anonymous;
    web_ctx_.mqtt_sync_with_wifi = mqtt_sync_with_wifi;
}

void AuraNetworkManager::attachThemeContext(ThemeManager &themeManager) {
    web_ctx_.theme_manager = &themeManager;
}

void AuraNetworkManager::setStateChangeCallback(StateChangeCallback cb, void *ctx) {
    state_change_cb_ = cb;
    state_change_ctx_ = ctx;
}

void AuraNetworkManager::notifyStateChangeIfNeeded() {
    if (wifi_state_ == wifi_state_last_) {
        return;
    }
    Logger::log(Logger::Info, "WiFi",
                "state changed: %d -> %d (connected=%s)",
                static_cast<int>(wifi_state_last_),
                static_cast<int>(wifi_state_),
                isConnected() ? "YES" : "NO");
    if (state_change_cb_) {
        state_change_cb_(wifi_state_last_, wifi_state_, isConnected(), state_change_ctx_);
    }
    wifi_state_last_ = wifi_state_;
}

bool AuraNetworkManager::setEnabled(bool enabled) {
    if (enabled == wifi_enabled_) {
        return false;
    }
    wifi_enabled_ = enabled;
    wifi_enabled_dirty_ = true;
    wifi_ui_dirty_ = true;
    if (storage_) {
        storage_->saveWiFiEnabled(wifi_enabled_);
        wifi_enabled_dirty_ = false;
    }
    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            startSta();
        } else {
            startAp();
        }
    } else {
        stopAp();
        WiFi.scanDelete();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_count_ = 0;
        wifi_retry_at_ms_ = 0;
        wifi_connect_start_ms_ = 0;
    }
    return true;
}

bool AuraNetworkManager::applyEnabledIfDirty() {
    if (!wifi_enabled_dirty_) {
        return false;
    }
    if (storage_) {
        storage_->saveWiFiEnabled(wifi_enabled_);
    }
    wifi_enabled_dirty_ = false;
    wifi_ui_dirty_ = true;
    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            startSta();
        } else {
            startAp();
        }
    } else {
        stopAp();
        WiFi.scanDelete();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_count_ = 0;
        wifi_retry_at_ms_ = 0;
        wifi_connect_start_ms_ = 0;
    }
    return true;
}

void AuraNetworkManager::clearCredentials() {
    if (storage_) {
        storage_->clearWiFiCredentials();
    }
    wifi_ssid_ = "";
    wifi_pass_ = "";
    wifi_retry_count_ = 0;
    wifi_retry_at_ms_ = 0;
    wifi_connect_start_ms_ = 0;
    wifi_scan_options_.clear();
    wifi_scan_in_progress_ = false;
    stopAp();
    WiFi.scanDelete();
    WiFi.disconnect(true, true);
    if (wifi_enabled_) {
        startAp();
    } else {
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
    }
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::startScan() {
    if (wifi_scan_in_progress_) {
        return;
    }
    WiFi.scanDelete();
    int ret = WiFi.scanNetworks(true);
    if (ret == WIFI_SCAN_RUNNING) {
        wifi_scan_in_progress_ = true;
        wifi_scan_started_ms_ = millis();
    } else if (ret >= 0) {
        wifi_build_scan_items(ret);
        WiFi.scanDelete();
        wifi_scan_in_progress_ = false;
    }
}

void AuraNetworkManager::connectSta() {
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 1;
    startSta();
    wifi_retry_count_ = 0;
}

void AuraNetworkManager::startApOnDemand() {
    if (!wifi_enabled_) {
        wifi_enabled_ = true;
        wifi_enabled_dirty_ = true;
        if (storage_) {
            storage_->saveWiFiEnabled(wifi_enabled_);
            wifi_enabled_dirty_ = false;
        }
    }
    startAp();
}

void AuraNetworkManager::poll() {
    if (wifi_state_ == WIFI_STATE_STA_CONNECTING) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            wifi_state_ = WIFI_STATE_STA_CONNECTED;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            wifi_ui_dirty_ = true;
            if (MDNS.begin("aura")) {
                LOGI("mDNS", "responder started: aura.local");
                MDNS.addService("http", "tcp", 80);
            } else {
                LOGW("mDNS", "start failed");
            }
            server_.on("/mqtt", HTTP_GET, mqtt_handle_root);
            server_.on("/mqtt", HTTP_POST, mqtt_handle_save);
            server_.on("/theme", HTTP_GET, theme_handle_root);
            server_.on("/theme/apply", HTTP_POST, theme_handle_apply);
            server_.onNotFound(wifi_handle_not_found);
            server_.begin();
            Logger::log(Logger::Info, "WiFi",
                        "connected, IP: %s",
                        WiFi.localIP().toString().c_str());
        } else if (st == WL_CONNECT_FAILED ||
                   (millis() - wifi_connect_start_ms_ > Config::WIFI_CONNECT_TIMEOUT_MS)) {
            esp_wifi_disconnect();
            if (wifi_retry_count_ < Config::WIFI_CONNECT_MAX_RETRIES) {
                wifi_retry_count_++;
                wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_state_ = WIFI_STATE_OFF;
                wifi_ui_dirty_ = true;
                Logger::log(Logger::Warn, "WiFi",
                            "connect failed, retry %u/%u",
                            static_cast<unsigned>(wifi_retry_count_),
                            static_cast<unsigned>(Config::WIFI_CONNECT_MAX_RETRIES));
            } else {
                LOGW("WiFi", "connect failed, enter error state");
                wifi_state_ = WIFI_STATE_OFF;
                wifi_retry_at_ms_ = 0;
                wifi_ui_dirty_ = true;
            }
        }
    } else if (wifi_state_ == WIFI_STATE_OFF && wifi_enabled_ && wifi_retry_at_ms_ != 0) {
        if (millis() >= wifi_retry_at_ms_) {
            wifi_retry_at_ms_ = 0;
            startSta();
        }
    }

    if (wifi_state_ == WIFI_STATE_AP_CONFIG) {
        if (wifi_scan_in_progress_) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                wifi_build_scan_items(n);
                WiFi.scanDelete();
                wifi_scan_in_progress_ = false;
            } else if (n == WIFI_SCAN_FAILED) {
                wifi_scan_options_.clear();
                wifi_scan_in_progress_ = false;
            }
        }
        server_.handleClient();
    } else if (wifi_state_ == WIFI_STATE_STA_CONNECTED) {
        // Periodic check of actual WiFi link while connected.
        static uint32_t last_check_ms = 0;
        uint32_t now = millis();
        if (now - last_check_ms >= 5000) {
            last_check_ms = now;
            wl_status_t st = WiFi.status();
            if (st != WL_CONNECTED) {
                const char *rssi_text = "n/a";
                Logger::log(Logger::Warn, "WiFi",
                            "connection lost detected (status=%d, was connected for %u seconds, RSSI was %s)",
                            static_cast<int>(st),
                            static_cast<unsigned>((now - wifi_connect_start_ms_) / 1000),
                            rssi_text);
                MDNS.end();
                server_.stop();
                wifi_state_ = WIFI_STATE_OFF;
                wifi_retry_at_ms_ = now + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_retry_count_ = 0;
                wifi_ui_dirty_ = true;
            }
        }
        server_.handleClient();
    }
    notifyStateChangeIfNeeded();
}

void AuraNetworkManager::warmupIfDisabled() {
    if (wifi_enabled_) {
        return;
    }
    LOGD("WiFi", "warmup (disabled mode)");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    delay(50);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void AuraNetworkManager::startSta() {
    if (wifi_ssid_.isEmpty()) {
        return;
    }
    stopAp();
    WiFi.persistent(false);
    const bool force_reset = (wifi_retry_count_ > 0);
    if (force_reset) {
        LOGI("WiFi", "forcing STA reset before retry");
        WiFi.mode(WIFI_OFF);
        delay(200);
    }
    wifi_mode_t mode = WiFi.getMode();
    if ((mode & WIFI_STA) == 0) {
        if (!WiFi.mode(WIFI_STA)) {
            LOGW("WiFi", "failed to enter STA mode, retrying");
            wifi_state_ = WIFI_STATE_OFF;
            wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
            wifi_ui_dirty_ = true;
            return;
        }
        delay(100);
    }
    if (force_reset) {
        WiFi.disconnect(true);
    } else {
        WiFi.disconnect();
    }
    delay(50);
    WiFi.begin(wifi_ssid_.c_str(), wifi_pass_.c_str());
    wifi_state_ = WIFI_STATE_STA_CONNECTING;
    wifi_connect_start_ms_ = millis();
    wifi_ui_dirty_ = true;
    Logger::log(Logger::Info, "WiFi",
                "connecting to: %s",
                wifi_label_safe(wifi_ssid_));
}

void AuraNetworkManager::startAp() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(Config::WIFI_AP_SSID);
    IPAddress ip = WiFi.softAPIP();
    startScan();
    server_.on("/", HTTP_GET, wifi_handle_root);
    server_.on("/save", HTTP_POST, wifi_handle_save);
    server_.on("/theme", HTTP_GET, theme_handle_root);
    server_.on("/theme/apply", HTTP_POST, theme_handle_apply);
    server_.onNotFound(wifi_handle_not_found);
    server_.begin();
    wifi_state_ = WIFI_STATE_AP_CONFIG;
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 0;
    wifi_ui_dirty_ = true;
    Logger::log(Logger::Info, "WiFi", "AP started: %s", Config::WIFI_AP_SSID);
    Logger::log(Logger::Info, "WiFi", "AP IP: %s", ip.toString().c_str());
}

void AuraNetworkManager::stopAp() {
    server_.stop();
    if (wifi_state_ == WIFI_STATE_AP_CONFIG) {
        WiFi.enableAP(false);
    }
    wifi_ui_dirty_ = true;
}
