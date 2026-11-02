// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDeferredActionsState.h"

WebDeferredActionsState::WebDeferredActionsState() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
#endif
}

void WebDeferredActionsState::reset() {
    lock();
    state_ = {};
    unlock();
}

void WebDeferredActionsState::beginWifiSave(uint64_t receipt) {
    lock();
    state_.wifi_start_sta = false;
    state_.wifi_grace_started = false;
    state_.wifi_receipt = receipt;
    state_.wifi_receipt_armed = false;
    state_.wifi_receipt_confirmed = false;
    unlock();
}

void WebDeferredActionsState::cancelWifiSave() { beginWifiSave(0); }

void WebDeferredActionsState::armWifiSave(uint64_t receipt, uint32_t now_ms) {
    lock();
    if (receipt && state_.wifi_receipt == receipt && !state_.wifi_receipt_confirmed) {
        state_.wifi_receipt_armed = true;
        state_.wifi_receipt_due_ms = now_ms + 600000;
    }
    unlock();
}

bool WebDeferredActionsState::confirmWifiSave(uint64_t receipt, uint32_t now_ms, uint32_t delay_ms) {
    lock();
    const bool valid = receipt && receipt == state_.wifi_receipt && state_.wifi_receipt_armed &&
        !deadlineReached(now_ms, state_.wifi_receipt_due_ms);
    if (valid && !state_.wifi_receipt_confirmed) {
        state_.wifi_receipt_confirmed = true;
        state_.wifi_start_sta = true;
        state_.wifi_grace_started = false;
        state_.wifi_grace_ms = delay_ms;
        state_.wifi_start_sta_due_ms = 0;
    }
    unlock();
    return valid;
}

void WebDeferredActionsState::scheduleWifiStartSta(uint32_t delay_ms) {
    lock();
    state_.wifi_start_sta = true;
    state_.wifi_grace_started = false;
    state_.wifi_grace_ms = delay_ms;
    state_.wifi_start_sta_due_ms = 0;
    unlock();
}

void WebDeferredActionsState::scheduleMqttSync(uint32_t now_ms, uint32_t delay_ms) {
    lock();
    state_.mqtt_sync = true;
    state_.mqtt_sync_due_ms = now_ms + delay_ms;
    unlock();
}

WebDeferredActionsDue WebDeferredActionsState::pollDue(uint32_t now_ms, bool (*prepare_wifi)()) {
    lock();
    WebDeferredActionsDue due{};
    if (state_.wifi_start_sta) {
        // Re-check on every poll: a later Rescan invalidates elapsed grace.
        // The callback requests cancellation and reports actual worker state.
        if (!prepare_wifi || !prepare_wifi()) {
            state_.wifi_grace_started = false;
        } else {
            if (!state_.wifi_grace_started) {
                state_.wifi_start_sta_due_ms = now_ms + state_.wifi_grace_ms;
                state_.wifi_grace_started = true;
            }
            if (deadlineReached(now_ms, state_.wifi_start_sta_due_ms)) {
                state_.wifi_start_sta = false;
                state_.wifi_grace_started = false;
                state_.wifi_start_sta_due_ms = 0;
                due.wifi_start_sta = true;
            }
        }
    }
    if (state_.mqtt_sync && deadlineReached(now_ms, state_.mqtt_sync_due_ms)) {
        state_.mqtt_sync = false;
        state_.mqtt_sync_due_ms = 0;
        due.mqtt_sync = true;
    }
    unlock();
    return due;
}

bool WebDeferredActionsState::deadlineReached(uint32_t now_ms, uint32_t due_ms) {
    return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

void WebDeferredActionsState::lock() const {
#ifdef UNIT_TEST
    mutex_.lock();
#else
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
#endif
}

void WebDeferredActionsState::unlock() const {
#ifdef UNIT_TEST
    mutex_.unlock();
#else
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}
