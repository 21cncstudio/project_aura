// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <atomic>
#include <mutex>
#include "core/WakePowerGuard.h"
#include <stddef.h>
#include <stdint.h>

// A blocking SDK scan runs on a worker, independently of Arduino's lossy event
// queue. Network task is the sole owner of start/cancel/poll/result access.
// The worker publishes completion after releasing driver records. No callback
// or queued event can be mistaken for the completion of a subsequent scan.
class WifiScanSession {
public:
    static constexpr size_t kMaxRecords = 15;
    struct Record { char ssid[33]{}; int rssi = 0; uint8_t auth_mode = 0; };
    enum class Result { Pending, Ready, Cancelled, Failed };
    WifiScanSession() = default;
    ~WifiScanSession();
    WifiScanSession(const WifiScanSession &) = delete;
    WifiScanSession &operator=(const WifiScanSession &) = delete;
    int start();
    bool cancel();
    // Network owner calls this even when new activity admission is closed.
    // Only stops an already leased operation; never starts driver work.
    void serviceWakeQuiescence();
    Result poll(int &count);
    void releaseResults();
    bool active() const { return active_; }
    bool cancelling() const { return cancelling_; }
    const Record *record(size_t index) const;

private:
    static void workerEntry(void *context);
    void runScan();
    bool stopWorker();
    std::mutex worker_mutex_;
    WakePowerGuard::Activity worker_activity_;
    void keepRecord(const Record &record);
    std::atomic<bool> finished_{true};
    std::atomic<bool> cancel_requested_{false};
    Record records_[kMaxRecords]{};
    size_t count_ = 0;
    int error_ = 0;
    int last_stop_error_ = 0;
    uint32_t started_ms_ = 0;
    uint32_t last_stop_attempt_ms_ = 0;
    bool active_ = false;
    bool cancelling_ = false;
};
