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
    for (auto &failure : failures_) failure = {};
    group_internal_free_ = 0;
    group_rssi_ = 0;
    last_failure_log_ms_ = 0;
    suppressed_failure_logs_ = 0;
    failure_head_ = 0;
    failure_count_ = 0;
    consecutive_failure_ = false;
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
        consecutive_failure_ = false;
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

    web_stream_copy_text(stats_.last_uri, sizeof(stats_.last_uri), uri.c_str(), true);
    unlock();
}

namespace {

bool same_failure(const WebStreamFailure &a, const WebStreamFailure &b) {
    const auto &x = a.environment;
    const auto &y = b.environment;
    const uint32_t memory_delta = x.internal_free > y.internal_free
        ? x.internal_free - y.internal_free : y.internal_free - x.internal_free;
    const int rssi_delta = x.rssi - y.rssi;
    return a.reason == b.reason && a.error.sdk_error == b.error.sdk_error &&
        a.error.socket_errno == b.error.socket_errno && a.error.errno_valid == b.error.errno_valid &&
        a.error.sdk_error_valid == b.error.sdk_error_valid &&
        a.error.send_result == b.error.send_result && a.error.stage == b.error.stage &&
        a.sent == b.sent && a.total == b.total &&
        a.error.partial_body_bytes == b.error.partial_body_bytes &&
        (a.duration_ms >= 10000) == (b.duration_ms >= 10000) &&
        (a.max_write_ms >= 200) == (b.max_write_ms >= 200) &&
        strcmp(a.request.path, b.request.path) == 0 &&
        strcmp(a.request.peer, b.request.peer) == 0 &&
        strcmp(a.request.user_agent, b.request.user_agent) == 0 &&
        a.request.path_truncated == b.request.path_truncated &&
        a.request.user_agent_truncated == b.request.user_agent_truncated &&
        x.memory_valid == y.memory_valid && memory_delta < 8192 &&
        (x.internal_free < 32768) == (y.internal_free < 32768) &&
        x.network_valid == y.network_valid &&
        (x.network_age_ms >= 5000) == (y.network_age_ms >= 5000) &&
        x.wifi_state == y.wifi_state && x.wifi_sta_status == y.wifi_sta_status &&
        x.wifi_enabled == y.wifi_enabled && x.wifi_connected == y.wifi_connected &&
        x.ap_mode == y.ap_mode && x.scan_in_progress == y.scan_in_progress &&
        x.wifi_retry_count == y.wifi_retry_count && x.has_rssi == y.has_rssi &&
        rssi_delta > -10 && rssi_delta < 10 &&
        x.mqtt_enabled == y.mqtt_enabled && x.mqtt_connected == y.mqtt_connected &&
        x.mqtt_retry_stage == y.mqtt_retry_stage &&
        x.mqtt_tls_waiting_for_time == y.mqtt_tls_waiting_for_time;
}

} // namespace

bool WebStreamState::recordFailure(WebStreamFailure &failure) {
    lock();
    bool log = true;
    const uint32_t memory_delta = group_internal_free_ > failure.environment.internal_free
        ? group_internal_free_ - failure.environment.internal_free
        : failure.environment.internal_free - group_internal_free_;
    const int rssi_delta = group_rssi_ - failure.environment.rssi;
    if (failure_count_ && consecutive_failure_ && memory_delta < 8192 &&
        rssi_delta > -10 && rssi_delta < 10 && same_failure(failures_[failure_head_], failure)) {
        const auto &previous = failures_[failure_head_];
        failure.first_ms = previous.first_ms;
        failure.count = previous.count == UINT32_MAX ? UINT32_MAX : previous.count + 1;
        // At most one warning per minute for consecutive equivalent failures.
        log = static_cast<uint32_t>(failure.last_ms - last_failure_log_ms_) >= 60000;
        if (!log && suppressed_failure_logs_ != UINT32_MAX) ++suppressed_failure_logs_;
    } else {
        group_internal_free_ = failure.environment.internal_free;
        group_rssi_ = failure.environment.rssi;
        if (failure_count_) failure_head_ = (failure_head_ + 1) % kWebStreamFailureCapacity;
        if (failure_count_ < kWebStreamFailureCapacity) ++failure_count_;
    }
    failures_[failure_head_] = failure;
    consecutive_failure_ = true;
    if (log) last_failure_log_ms_ = failure.last_ms;
    unlock();
    return log;
}

WebTransferSnapshot WebStreamState::snapshot(uint32_t now_ms) const {
    lock();
    WebTransferSnapshot copy;
    copy.failure_count = failure_count_;
    copy.suppressed_failure_logs = suppressed_failure_logs_;
    for (size_t i = 0; i < failure_count_; ++i) {
        copy.failures[i] = failures_[(failure_head_ + kWebStreamFailureCapacity - i) % kWebStreamFailureCapacity];
    }
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
