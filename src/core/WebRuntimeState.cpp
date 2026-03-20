// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/WebRuntimeState.h"

WebRuntimeState::WebRuntimeState() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

void WebRuntimeState::update(const SensorData &data, bool gas_warmup, const FanControl &fan_control) {
    const FanControl::Snapshot fan_snapshot = fan_control.snapshot();
    lock();
    snapshot_.data = data;
    snapshot_.gas_warmup = gas_warmup;
    snapshot_.fan = fan_snapshot;
    unlock();
}

WebRuntimeSnapshot WebRuntimeState::snapshot() const {
    lock();
    WebRuntimeSnapshot copy = snapshot_;
    unlock();
    return copy;
}

void WebRuntimeState::lock() const {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void WebRuntimeState::unlock() const {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}
