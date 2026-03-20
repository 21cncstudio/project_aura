// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include "web/WebStreamPolicy.h"

struct WebStreamStatsSnapshot {
    uint32_t ok_count = 0;
    uint32_t abort_count = 0;
    uint32_t slow_count = 0;
    uint32_t mqtt_connect_deferred_count = 0;
    uint32_t mqtt_publish_deferred_count = 0;
    StreamAbortReason last_abort_reason = StreamAbortReason::None;
    int last_errno = 0;
    size_t last_sent = 0;
    size_t last_total = 0;
    uint32_t last_max_write_ms = 0;
    String last_uri;
};

struct WebTransferSnapshot {
    WebStreamStatsSnapshot stats;
    uint16_t active_transfers = 0;
    uint32_t mqtt_pause_remaining_ms = 0;
};

class WebStreamState {
public:
    WebStreamState();

    void reset();
    void noteShellPriority(uint32_t now_ms, uint32_t wifi_sta_connected_elapsed_ms);
    bool shouldPauseMqtt(uint32_t now_ms) const;
    uint32_t pauseRemainingMs(uint32_t now_ms) const;
    void beginTransfer(uint32_t now_ms);
    void endTransfer(uint32_t now_ms);
    void noteMqttConnectDeferred();
    void noteMqttPublishDeferred();
    void recordStreamResult(const String &uri,
                            size_t total_size,
                            size_t sent,
                            bool ok,
                            StreamAbortReason abort_reason,
                            uint32_t max_write_ms,
                            int last_socket_errno,
                            uint32_t slow_write_warn_ms);
    WebTransferSnapshot snapshot(uint32_t now_ms) const;

private:
    static constexpr size_t kLastUriMaxLen = 95;

    struct StatsState {
        uint32_t ok_count = 0;
        uint32_t abort_count = 0;
        uint32_t slow_count = 0;
        uint32_t mqtt_connect_deferred_count = 0;
        uint32_t mqtt_publish_deferred_count = 0;
        StreamAbortReason last_abort_reason = StreamAbortReason::None;
        int last_errno = 0;
        size_t last_sent = 0;
        size_t last_total = 0;
        uint32_t last_max_write_ms = 0;
        char last_uri[kLastUriMaxLen + 1] = {};
    };

    struct TransferState {
        uint16_t active_count = 0;
        uint32_t pause_until_ms = 0;
        uint32_t shell_priority_until_ms = 0;
    };

    static bool deadlineReached(uint32_t now_ms, uint32_t due_ms);
    static uint32_t deadlineRemainingMs(uint32_t now_ms, uint32_t deadline_ms);
    void noteTransferActivityLocked(uint32_t now_ms);
    void lock() const;
    void unlock() const;

#ifdef UNIT_TEST
    mutable std::mutex mutex_;
#else
    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
    StatsState stats_{};
    TransferState transfer_{};
};
