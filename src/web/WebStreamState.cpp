// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebStreamState.h"

#include <string.h>

namespace {

constexpr uint32_t kWebTransferMqttPauseMs = 2000;
constexpr uint32_t kWebShellMqttPriorityMs = 15000;
constexpr uint32_t kWebRecentStaConnectPriorityMs = 45000;

}  // namespace

WebStreamState::WebStreamState() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
#endif
}

void WebStreamState::reset() {
    lock();
    stats_ = {};
    transfer_ = {};
    unlock();
}

void WebStreamState::noteShellPriority(uint32_t now_ms, uint32_t wifi_sta_connected_elapsed_ms) {
    uint32_t priority_until_ms = now_ms + kWebShellMqttPriorityMs;
    if (wifi_sta_connected_elapsed_ms > 0 &&
        wifi_sta_connected_elapsed_ms < kWebRecentStaConnectPriorityMs) {
        const uint32_t sta_priority_until_ms =
            now_ms + (kWebRecentStaConnectPriorityMs - wifi_sta_connected_elapsed_ms);
        if (deadlineRemainingMs(now_ms, sta_priority_until_ms) >
            deadlineRemainingMs(now_ms, priority_until_ms)) {
            priority_until_ms = sta_priority_until_ms;
        }
    }

    lock();
    if (deadlineRemainingMs(now_ms, priority_until_ms) >
        deadlineRemainingMs(now_ms, transfer_.shell_priority_until_ms)) {
        transfer_.shell_priority_until_ms = priority_until_ms;
    }
    unlock();
}

bool WebStreamState::shouldPauseMqtt(uint32_t now_ms) const {
    lock();
    const bool paused = transfer_.active_count > 0 ||
                        deadlineRemainingMs(now_ms, transfer_.pause_until_ms) > 0 ||
                        deadlineRemainingMs(now_ms, transfer_.shell_priority_until_ms) > 0;
    unlock();
    return paused;
}

uint32_t WebStreamState::pauseRemainingMs(uint32_t now_ms) const {
    lock();
    const uint32_t transfer_remaining = deadlineRemainingMs(now_ms, transfer_.pause_until_ms);
    const uint32_t shell_remaining = deadlineRemainingMs(now_ms, transfer_.shell_priority_until_ms);
    unlock();
    return transfer_remaining > shell_remaining ? transfer_remaining : shell_remaining;
}

void WebStreamState::beginTransfer(uint32_t now_ms) {
    lock();
    transfer_.active_count++;
    noteTransferActivityLocked(now_ms);
    unlock();
}

void WebStreamState::endTransfer(uint32_t now_ms) {
    lock();
    if (transfer_.active_count > 0) {
        transfer_.active_count--;
    }
    noteTransferActivityLocked(now_ms);
    unlock();
}

void WebStreamState::noteMqttConnectDeferred() {
    lock();
    stats_.mqtt_connect_deferred_count++;
    unlock();
}

void WebStreamState::noteMqttPublishDeferred() {
    lock();
    stats_.mqtt_publish_deferred_count++;
    unlock();
}

void WebStreamState::recordStreamResult(const String &uri,
                                        size_t total_size,
                                        size_t sent,
                                        bool ok,
                                        StreamAbortReason abort_reason,
                                        uint32_t max_write_ms,
                                        int last_socket_errno,
                                        uint32_t slow_write_warn_ms) {
    lock();
    if (ok) {
        stats_.ok_count++;
    } else {
        stats_.abort_count++;
    }
    if (max_write_ms >= slow_write_warn_ms) {
        stats_.slow_count++;
    }

    stats_.last_abort_reason = abort_reason;
    stats_.last_errno = last_socket_errno;
    stats_.last_sent = sent;
    stats_.last_total = total_size;
    stats_.last_max_write_ms = max_write_ms;

    const size_t copy_len = uri.length() < kLastUriMaxLen ? uri.length() : kLastUriMaxLen;
    memcpy(stats_.last_uri, uri.c_str(), copy_len);
    stats_.last_uri[copy_len] = '\0';
    unlock();
}

WebTransferSnapshot WebStreamState::snapshot(uint32_t now_ms) const {
    lock();
    WebTransferSnapshot copy;
    copy.stats.ok_count = stats_.ok_count;
    copy.stats.abort_count = stats_.abort_count;
    copy.stats.slow_count = stats_.slow_count;
    copy.stats.mqtt_connect_deferred_count = stats_.mqtt_connect_deferred_count;
    copy.stats.mqtt_publish_deferred_count = stats_.mqtt_publish_deferred_count;
    copy.stats.last_abort_reason = stats_.last_abort_reason;
    copy.stats.last_errno = stats_.last_errno;
    copy.stats.last_sent = stats_.last_sent;
    copy.stats.last_total = stats_.last_total;
    copy.stats.last_max_write_ms = stats_.last_max_write_ms;
    copy.stats.last_uri = stats_.last_uri;
    copy.active_transfers = transfer_.active_count;
    const uint32_t transfer_remaining = deadlineRemainingMs(now_ms, transfer_.pause_until_ms);
    const uint32_t shell_remaining = deadlineRemainingMs(now_ms, transfer_.shell_priority_until_ms);
    copy.mqtt_pause_remaining_ms =
        transfer_remaining > shell_remaining ? transfer_remaining : shell_remaining;
    unlock();
    return copy;
}

bool WebStreamState::deadlineReached(uint32_t now_ms, uint32_t due_ms) {
    return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

uint32_t WebStreamState::deadlineRemainingMs(uint32_t now_ms, uint32_t deadline_ms) {
    if (deadline_ms == 0 || deadlineReached(now_ms, deadline_ms)) {
        return 0;
    }
    return deadline_ms - now_ms;
}

void WebStreamState::noteTransferActivityLocked(uint32_t now_ms) {
    transfer_.pause_until_ms = now_ms + kWebTransferMqttPauseMs;
}

void WebStreamState::lock() const {
#ifdef UNIT_TEST
    mutex_.lock();
#else
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
#endif
}

void WebStreamState::unlock() const {
#ifdef UNIT_TEST
    mutex_.unlock();
#else
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}
