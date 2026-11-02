// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/WifiScanSession.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include "core/Logger.h"
#include "core/WakePowerGuard.h"

namespace {
constexpr uint32_t kScanDeadlineMs = 10000;
constexpr uint32_t kStopRetryMs = 1000;
constexpr uint32_t kWakeStopRetryMs = 100;
}

WifiScanSession::~WifiScanSession() {
    // Production owns this object for the lifetime of NetworkManager. Also
    // make shorter-lived test owners safe; never destroy a worker's storage.
    if (active_) cancel();
    while (!finished_.load(std::memory_order_acquire)) {
        int ignored = 0;
        poll(ignored);
        delay(1);
    }
}

int WifiScanSession::start() {
    if (active_) return WIFI_SCAN_RUNNING;
    count_ = 0;
    error_ = ESP_OK;
    last_stop_error_ = ESP_OK;
    cancelling_ = false;
    cancel_requested_.store(false, std::memory_order_relaxed);
    finished_.store(false, std::memory_order_relaxed);
    active_ = true;
    started_ms_ = millis();
    // Same core as the network task; keep its event/command processing free.
    if (xTaskCreatePinnedToCore(workerEntry, "wifi_scan", 4096, this,
                                tskIDLE_PRIORITY + 1, nullptr, 0) != pdPASS) {
        finished_.store(true, std::memory_order_release);
        active_ = false;
        LOGW("WiFi", "scan worker allocation failed");
        return WIFI_SCAN_FAILED;
    }
    return WIFI_SCAN_RUNNING;
}

bool WifiScanSession::cancel() {
    if (!active_) return true;
    cancelling_ = true;
    cancel_requested_.store(true, std::memory_order_release);
    if (finished_.load(std::memory_order_acquire)) return true;
    return stopWorker();
}

bool WifiScanSession::stopWorker() {
    last_stop_attempt_ms_ = millis();
    // A stop may run in PreQuiet only while the worker's existing lease
    // prevents Switching. Synchronize stop with final cleanup/lease release.
    const std::lock_guard<std::mutex> lock(worker_mutex_);
    if (!worker_activity_) return true; // Not started yet, or already done.
    const esp_err_t error = esp_wifi_scan_stop();
    if (error != ESP_OK && error != last_stop_error_)
        LOGW("WiFi", "scan stop failed: %d; retaining worker and retrying", error);
    last_stop_error_ = error;
    return error == ESP_OK;
}

void WifiScanSession::serviceWakeQuiescence() {
    if (!active_ || !WakePowerGuard::backgroundPaused(millis())) return;
    if (!cancelling_) cancel();
    else if (!finished_.load(std::memory_order_acquire) &&
             static_cast<uint32_t>(millis() - last_stop_attempt_ms_) >= kWakeStopRetryMs)
        stopWorker();
}

WifiScanSession::Result WifiScanSession::poll(int &count) {
    count = 0;
    if (!active_) return Result::Cancelled;
    if (!finished_.load(std::memory_order_acquire)) {
        const uint32_t now = millis();
        if (!cancelling_ && static_cast<uint32_t>(now - started_ms_) >= kScanDeadlineMs) {
            LOGW("WiFi", "scan driver deadline exceeded; cancelling");
            cancel();
        } else if (cancelling_ && static_cast<uint32_t>(now - last_stop_attempt_ms_) >= kStopRetryMs) {
            // Also covers cancellation racing the worker's SDK start.
            cancel();
        }
        return Result::Pending;
    }
    if (cancelling_) { releaseResults(); return Result::Cancelled; }
    if (error_ != ESP_OK) { releaseResults(); return Result::Failed; }
    count = static_cast<int>(count_);
    return Result::Ready;
}

void WifiScanSession::releaseResults() {
    // Never release a live worker or reuse its slot on a timeout alone.
    if (!finished_.load(std::memory_order_acquire)) return;
    active_ = cancelling_ = false;
    count_ = 0;
}

const WifiScanSession::Record *WifiScanSession::record(size_t index) const {
    return finished_.load(std::memory_order_acquire) && active_ && !cancelling_ &&
           error_ == ESP_OK && index < count_ ? &records_[index] : nullptr;
}

void WifiScanSession::keepRecord(const Record &row) {
    if (!row.ssid[0]) return;
    size_t index = count_;
    for (size_t i = 0; i < count_; ++i) {
        if (strcmp(records_[i].ssid, row.ssid) == 0) {
            if (records_[i].rssi >= row.rssi) return;
            index = i; break;
        }
    }
    if (index == count_) {
        if (count_ < kMaxRecords) ++count_;
        else {
            index = 0;
            for (size_t i = 1; i < count_; ++i)
                if (records_[i].rssi < records_[index].rssi) index = i;
            if (records_[index].rssi >= row.rssi) return;
        }
    }
    records_[index] = row;
}

void WifiScanSession::runScan() {
    if (cancel_requested_.load(std::memory_order_acquire)) return;
    {
        const std::lock_guard<std::mutex> lock(worker_mutex_);
        if (cancel_requested_.load(std::memory_order_acquire)) return;
        worker_activity_ = WakePowerGuard::tryAcquireActivity(millis());
        if (!worker_activity_) { error_ = ESP_ERR_INVALID_STATE; return; }
    }
    wifi_scan_config_t config{};
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min = 100;
    config.scan_time.active.max = 300; // Match the previous Arduino defaults.
    // block=true uses SDK completion, without posting WIFI_EVENT_SCAN_DONE.
    // In particular, Network.postEvent OOM cannot strand this operation.
    error_ = esp_wifi_scan_start(&config, true);
    // Stop cannot race driver-result access or release the lease underneath it.
    const std::lock_guard<std::mutex> lock(worker_mutex_);
    if (error_ == ESP_OK && !cancel_requested_.load(std::memory_order_acquire)) {
        uint16_t found = 0;
        error_ = esp_wifi_scan_get_ap_num(&found);
        for (uint16_t i = 0; error_ == ESP_OK && i < found; ++i) {
            wifi_ap_record_t sdk_record{};
            error_ = esp_wifi_scan_get_ap_record(&sdk_record);
            if (error_ != ESP_OK) break;
            Record row{};
            memcpy(row.ssid, sdk_record.ssid, sizeof(row.ssid)-1);
            row.rssi = sdk_record.rssi;
            row.auth_mode = static_cast<uint8_t>(sdk_record.authmode);
            keepRecord(row);
        }
    }
    // Free any unread records on cancellation/failure, including allocation
    // failures inside the driver. The network owner never touches SDK buffers.
    const esp_err_t cleanup = esp_wifi_clear_ap_list();
    if (cleanup != ESP_OK && cleanup != ESP_ERR_WIFI_NOT_INIT && cleanup != ESP_ERR_WIFI_NOT_STARTED)
        LOGW("WiFi", "scan result cleanup failed: %d", cleanup);
    worker_activity_ = {};
}

void WifiScanSession::workerEntry(void *context) {
    auto *self = static_cast<WifiScanSession *>(context);
    self->runScan();
    self->finished_.store(true, std::memory_order_release);
    // No access to self after publishing completion: the owner may reuse it.
    vTaskDelete(nullptr);
}
