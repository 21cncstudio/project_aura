// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/NetworkManager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "ui/ThemeManager.h"
#include "web/PrometheusExporter.h"

namespace {

AuraNetworkManager *g_network = nullptr;
const uint32_t kInitialWifiConnectDelayMs = 1000;
constexpr uint32_t kWifiInternalHeapMinFreeForStart = 32UL * 1024UL;
constexpr uint32_t kWifiInternalHeapMinLargestForStart = 16UL * 1024UL;

bool has_internal_heap_for_wifi_start(uint32_t &free_bytes, uint32_t &largest_block_bytes) {
    free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    largest_block_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return (free_bytes >= kWifiInternalHeapMinFreeForStart) &&
           (largest_block_bytes >= kWifiInternalHeapMinLargestForStart);
}

uint32_t mac_suffix_24bit() {
    return static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
}

String build_wifi_hostname() {
    char hostname[16];
    snprintf(hostname, sizeof(hostname), "aura-%06x", static_cast<unsigned>(mac_suffix_24bit()));
    return String(hostname);
}

String build_ap_ssid() {
    char ssid[20];
    snprintf(ssid, sizeof(ssid), "Aura-%06X-AP", static_cast<unsigned>(mac_suffix_24bit()));
    return String(ssid);
}

void network_wifi_start_scan() {
    if (g_network) {
        g_network->startScan();
    }
}

void network_wifi_stop_scan() {
    if (g_network) {
        g_network->stopScan();
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

bool network_wifi_is_ap_mode() {
    return g_network && g_network->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG;
}

} // namespace

void AuraNetworkManager::begin(StorageManager &storage) {
    storage_ = &storage;
    g_network = this;
    mdns_started_ = false;
    WiFi.persistent(false);
    hostname_ = build_wifi_hostname();
    if (hostname_.isEmpty()) {
        hostname_ = "aura";
    }
    ap_ssid_ = build_ap_ssid();
    if (ap_ssid_.isEmpty()) {
        ap_ssid_ = Config::WIFI_AP_SSID;
    }
    LOGI("WiFi", "hostname: %s", hostname_.c_str());
    LOGI("WiFi", "AP SSID: %s", ap_ssid_.c_str());

    web_ctx_.server = &server_;
    web_ctx_.storage = storage_;
    web_ctx_.hostname = &hostname_;
    web_ctx_.wifi_ssid = &wifi_ssid_;
    web_ctx_.wifi_pass = &wifi_pass_;
    web_ctx_.wifi_enabled = &wifi_enabled_;
    web_ctx_.wifi_enabled_dirty = &wifi_enabled_dirty_;
    web_ctx_.wifi_ui_dirty = &wifi_ui_dirty_;
    web_ctx_.wifi_scan_in_progress = &wifi_scan_in_progress_;
    web_ctx_.wifi_scan_options = &wifi_scan_options_;
    web_ctx_.wifi_is_connected = network_wifi_is_connected;
    web_ctx_.wifi_is_ap_mode = network_wifi_is_ap_mode;
    web_ctx_.wifi_start_scan = network_wifi_start_scan;
    web_ctx_.wifi_stop_scan = network_wifi_stop_scan;
    web_ctx_.wifi_start_sta = network_wifi_start_sta;
    web_ctx_.mqtt_ui_open = &mqtt_ui_open_;
    web_ctx_.theme_ui_open = &theme_ui_open_;
    WebHandlersInit(&web_ctx_);
    PrometheusExporterInit(&web_ctx_);
    registerServerRoutes();

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
        stopMdns();
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

void AuraNetworkManager::attachChartsContext(ChartsHistory &chartsHistory) {
    web_ctx_.charts_history = &chartsHistory;
}

void AuraNetworkManager::attachDacContext(FanControl &fanControl,
                                          SensorManager &sensorManager,
                                          SensorData &sensorData) {
    web_ctx_.fan_control = &fanControl;
    web_ctx_.sensor_manager = &sensorManager;
    web_ctx_.sensor_data = &sensorData;
}

void AuraNetworkManager::attachUiContext(UiController &uiController) {
    web_ctx_.ui_controller = &uiController;
}

void AuraNetworkManager::registerServerRoutes() {
    if (server_routes_registered_) {
        return;
    }

    server_.on("/", HTTP_GET, dashboard_handle_root);
    server_.on("/dashboard", HTTP_GET, dashboard_handle_root);
    server_.on("/wifi", HTTP_GET, wifi_handle_root);
    server_.on("/save", HTTP_POST, wifi_handle_save);
    server_.on("/mqtt", HTTP_GET, mqtt_handle_root);
    server_.on("/mqtt", HTTP_POST, mqtt_handle_save);
    server_.on("/theme", HTTP_GET, theme_handle_root);
    server_.on("/theme/apply", HTTP_POST, theme_handle_apply);
    server_.on("/dac", HTTP_GET, dac_handle_root);
    server_.on("/dac/state", HTTP_GET, dac_handle_state);
    server_.on("/dac/action", HTTP_POST, dac_handle_action);
    server_.on("/dac/auto", HTTP_POST, dac_handle_auto);
    server_.on("/api/charts", HTTP_GET, charts_handle_data);
    server_.on("/api/state", HTTP_GET, state_handle_data);
    server_.on("/api/events", HTTP_GET, events_handle_data);
    server_.on("/api/settings", HTTP_POST, settings_handle_update);
    server_.on("/api/ota", HTTP_POST, ota_handle_update, ota_handle_upload);
    server_.on("/metrics", HTTP_GET, prometheus_handle_metrics);
    server_.onNotFound(wifi_handle_not_found);
    server_routes_registered_ = true;
}

String AuraNetworkManager::localUrl(const char *path) const {
    String url = "http://";
    if (hostname_.isEmpty()) {
        url += "aura";
    } else {
        url += hostname_;
    }
    url += ".local";
    if (path && path[0] != '\0') {
        if (path[0] != '/') {
            url += '/';
        }
        url += path;
    }
    return url;
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
        stopMdns();
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
        stopMdns();
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
    stopMdns();
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
    if (WebHandlersIsOtaBusy()) {
        return;
    }
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

void AuraNetworkManager::stopScan() {
    if (!wifi_scan_in_progress_) {
        return;
    }
    WiFi.scanDelete();
    wifi_scan_in_progress_ = false;
    wifi_scan_started_ms_ = 0;
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
    const bool ota_busy = WebHandlersIsOtaBusy();
    const uint8_t server_poll_passes = ota_busy ? 3 : 1;
    auto handle_server_client = [this, server_poll_passes]() {
        for (uint8_t i = 0; i < server_poll_passes; ++i) {
            server_.handleClient();
        }
    };

    if (wifi_state_ == WIFI_STATE_STA_CONNECTING) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            wifi_state_ = WIFI_STATE_STA_CONNECTED;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            wifi_ui_dirty_ = true;
            startMdns();
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
            if (ota_busy) {
                stopScan();
            } else {
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
        }
        handle_server_client();
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
                stopMdns();
                server_.stop();
                wifi_state_ = WIFI_STATE_OFF;
                wifi_retry_at_ms_ = now + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_retry_count_ = 0;
                wifi_ui_dirty_ = true;
            }
        }
        if (wifi_state_ == WIFI_STATE_STA_CONNECTED) {
            handle_server_client();
        }
    }
    WebHandlersPollDeferred();
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

    uint32_t internal_free = 0;
    uint32_t internal_largest = 0;
    if (!has_internal_heap_for_wifi_start(internal_free, internal_largest)) {
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        wifi_ui_dirty_ = true;
        Logger::log(Logger::Warn, "WiFi",
                    "defer STA start: low internal heap free=%u largest=%u (need free>=%u largest>=%u)",
                    internal_free,
                    internal_largest,
                    static_cast<unsigned>(kWifiInternalHeapMinFreeForStart),
                    static_cast<unsigned>(kWifiInternalHeapMinLargestForStart));
        return;
    }

    stopMdns();
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
        // Keep radio started in STA mode; reconnect should not power WiFi off.
        WiFi.disconnect(false);
    } else {
        WiFi.disconnect();
    }
    delay(50);
    if (!hostname_.isEmpty() && !WiFi.setHostname(hostname_.c_str())) {
        LOGW("WiFi", "setHostname failed");
    }
    WiFi.begin(wifi_ssid_.c_str(), wifi_pass_.c_str());
    wifi_state_ = WIFI_STATE_STA_CONNECTING;
    wifi_connect_start_ms_ = millis();
    wifi_ui_dirty_ = true;
    String safe_ssid = wifi_label_safe(wifi_ssid_);
    Logger::log(Logger::Info, "WiFi",
                "connecting to: %s",
                safe_ssid.c_str());
}

void AuraNetworkManager::startAp() {
    stopMdns();
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    const char *ap_ssid = ap_ssid_.isEmpty() ? Config::WIFI_AP_SSID : ap_ssid_.c_str();
    if (!WiFi.softAP(ap_ssid)) {
        LOGW("WiFi", "failed to start AP: %s", ap_ssid);
        if (WiFi.status() == WL_CONNECTED) {
            wifi_state_ = WIFI_STATE_STA_CONNECTED;
            server_.begin();
        } else {
            wifi_state_ = WIFI_STATE_OFF;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
        }
        wifi_ui_dirty_ = true;
        return;
    }
    IPAddress ip = WiFi.softAPIP();
    startScan();
    server_.begin();
    wifi_state_ = WIFI_STATE_AP_CONFIG;
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 0;
    wifi_ui_dirty_ = true;
    Logger::log(Logger::Info, "WiFi", "AP started: %s", ap_ssid);
    Logger::log(Logger::Info, "WiFi", "AP IP: %s", ip.toString().c_str());
}

void AuraNetworkManager::stopAp() {
    server_.stop();
    if (wifi_state_ == WIFI_STATE_AP_CONFIG) {
        WiFi.enableAP(false);
    }
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::startMdns() {
    if (mdns_started_) {
        return;
    }
    if (MDNS.begin(hostname_.c_str())) {
        Logger::log(Logger::Info, "mDNS",
                    "responder started: %s.local",
                    hostname_.c_str());
        MDNS.addService("http", "tcp", 80);
        mdns_started_ = true;
    } else {
        LOGW("mDNS", "start failed");
    }
}

void AuraNetworkManager::stopMdns() {
    if (!mdns_started_) {
        return;
    }
    MDNS.end();
    mdns_started_ = false;
}
