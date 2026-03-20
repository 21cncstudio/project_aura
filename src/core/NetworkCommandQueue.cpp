// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/NetworkCommandQueue.h"

#include "core/ConnectivityRuntime.h"
#include "modules/MqttManager.h"
#include "modules/NetworkManager.h"

NetworkCommandQueue::NetworkCommandQueue() {
    queue_ = xQueueCreateStatic(kCapacity,
                                sizeof(Command),
                                queue_buffer_,
                                &queue_storage_);
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

bool NetworkCommandQueue::enqueue(Type type, bool bool_value) {
    if (!queue_) {
        return false;
    }
    Command command{};
    command.type = type;
    command.bool_value = bool_value;
    return xQueueSendToBack(queue_, &command, 0) == pdTRUE;
}

bool NetworkCommandQueue::publishSavedWifiSettings(const WifiSettingsUpdate &update) {
    if (!mutex_) {
        return false;
    }
    lock();
    pending_wifi_settings_update_ = update;
    pending_wifi_settings_valid_ = true;
    unlock();
    return true;
}

bool NetworkCommandQueue::publishSavedMqttSettings(const MqttSettingsUpdate &update) {
    if (!mutex_) {
        return false;
    }
    lock();
    pending_mqtt_settings_update_ = update;
    pending_mqtt_settings_valid_ = true;
    unlock();
    return true;
}

bool NetworkCommandQueue::publishWifiScanRequest(Type type) {
    if (!mutex_ ||
        (type != Type::RequestWifiScanStart && type != Type::RequestWifiScanStop)) {
        return false;
    }
    lock();
    pending_wifi_scan_request_ = type;
    pending_wifi_scan_request_valid_ = true;
    unlock();
    return true;
}

bool NetworkCommandQueue::tryDequeue(Command &out) {
    if (!queue_) {
        return false;
    }
    return xQueueReceive(queue_, &out, 0) == pdTRUE;
}

void NetworkCommandQueue::processAll(AuraNetworkManager &networkManager,
                                     MqttManager &mqttManager,
                                     ConnectivityRuntime &connectivityRuntime) {
    Command command{};
    bool processed = false;
    while (tryDequeue(command)) {
        processed = true;
        switch (command.type) {
            case Type::ApplyPendingWifiEnabled:
                networkManager.applyEnabledIfDirty();
                mqttManager.syncWithWifi();
                break;
            case Type::SetWifiEnabled:
                networkManager.setEnabled(command.bool_value);
                mqttManager.syncWithWifi();
                break;
            case Type::SetMqttUserEnabled:
                mqttManager.setUserEnabled(command.bool_value);
                mqttManager.syncWithWifi();
                break;
            case Type::SyncMqttWithWifi:
                mqttManager.syncWithWifi();
                break;
            case Type::RequestMqttReconnect:
                mqttManager.requestReconnect();
                break;
            case Type::RequestWifiReconnect:
                if (!networkManager.isEnabled()) {
                    networkManager.setEnabled(true);
                } else if (networkManager.ssid().isEmpty()) {
                    networkManager.startApOnDemand();
                } else {
                    networkManager.connectSta();
                }
                mqttManager.syncWithWifi();
                break;
            case Type::RequestWifiScanStart:
                networkManager.startScan();
                break;
            case Type::RequestWifiScanStop:
                networkManager.stopScan();
                break;
            case Type::ToggleWifiApMode:
                if (networkManager.state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
                    if (!networkManager.ssid().isEmpty()) {
                        networkManager.connectSta();
                    } else {
                        networkManager.setEnabled(false);
                    }
                } else {
                    networkManager.startApOnDemand();
                }
                mqttManager.syncWithWifi();
                break;
            case Type::ClearWifiCredentials:
                networkManager.clearCredentials();
                mqttManager.syncWithWifi();
                break;
        }
    }

    Type wifi_scan_request = Type::RequestWifiScanStart;
    if (takePendingWifiScanRequest(wifi_scan_request)) {
        processed = true;
        if (wifi_scan_request == Type::RequestWifiScanStart) {
            networkManager.startScan();
        } else {
            networkManager.stopScan();
        }
    }

    WifiSettingsUpdate wifi_settings_update;
    if (takePendingWifiSettingsUpdate(wifi_settings_update)) {
        processed = true;
        networkManager.applySavedWiFiSettings(wifi_settings_update.ssid,
                                             wifi_settings_update.pass,
                                             wifi_settings_update.enabled);
    }

    MqttSettingsUpdate mqtt_settings_update;
    if (takePendingMqttSettingsUpdate(mqtt_settings_update)) {
        processed = true;
        mqttManager.applySavedSettings(mqtt_settings_update.host,
                                       mqtt_settings_update.port,
                                       mqtt_settings_update.user,
                                       mqtt_settings_update.pass,
                                       mqtt_settings_update.base_topic,
                                       mqtt_settings_update.device_name,
                                       mqtt_settings_update.discovery,
                                       mqtt_settings_update.anonymous);
    }

    if (processed) {
        connectivityRuntime.update(networkManager, mqttManager);
    }
}

void NetworkCommandQueue::lock() const {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void NetworkCommandQueue::unlock() const {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}

bool NetworkCommandQueue::takePendingWifiScanRequest(Type &out) {
    lock();
    if (!pending_wifi_scan_request_valid_) {
        unlock();
        return false;
    }
    out = pending_wifi_scan_request_;
    pending_wifi_scan_request_valid_ = false;
    unlock();
    return true;
}

bool NetworkCommandQueue::takePendingWifiSettingsUpdate(WifiSettingsUpdate &out) {
    lock();
    if (!pending_wifi_settings_valid_) {
        unlock();
        return false;
    }
    out = pending_wifi_settings_update_;
    pending_wifi_settings_valid_ = false;
    unlock();
    return true;
}

bool NetworkCommandQueue::takePendingMqttSettingsUpdate(MqttSettingsUpdate &out) {
    lock();
    if (!pending_mqtt_settings_valid_) {
        unlock();
        return false;
    }
    out = pending_mqtt_settings_update_;
    pending_mqtt_settings_valid_ = false;
    unlock();
    return true;
}
