// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/MqttRuntimeState.h"

MqttRuntimeState::MqttRuntimeState() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

void MqttRuntimeState::update(const SensorData &data,
                              bool night_mode,
                              bool alert_blink,
                              bool backlight_on,
                              bool auto_night_enabled) {
    lock();
    snapshot_.data = data;
    snapshot_.night_mode = night_mode;
    snapshot_.alert_blink = alert_blink;
    snapshot_.backlight_on = backlight_on;
    snapshot_.auto_night_enabled = auto_night_enabled;
    unlock();
}

MqttRuntimeSnapshot MqttRuntimeState::snapshot() const {
    lock();
    MqttRuntimeSnapshot copy = snapshot_;
    unlock();
    return copy;
}

void MqttRuntimeState::requestPublish() {
    publish_requested_.store(true, std::memory_order_release);
}

bool MqttRuntimeState::consumePublishRequest() {
    return publish_requested_.exchange(false, std::memory_order_acq_rel);
}

void MqttRuntimeState::mergePendingCommands(const MqttPendingCommands &pending) {
    lock();
    if (pending.night_mode) {
        pending_commands_.night_mode = true;
        pending_commands_.night_mode_value = pending.night_mode_value;
    }
    if (pending.alert_blink) {
        pending_commands_.alert_blink = true;
        pending_commands_.alert_blink_value = pending.alert_blink_value;
    }
    if (pending.backlight) {
        pending_commands_.backlight = true;
        pending_commands_.backlight_value = pending.backlight_value;
    }
    if (pending.restart) {
        pending_commands_.restart = true;
    }
    unlock();
}

bool MqttRuntimeState::takePendingCommands(MqttPendingCommands &out) {
    lock();
    if (!pending_commands_.night_mode &&
        !pending_commands_.alert_blink &&
        !pending_commands_.backlight &&
        !pending_commands_.restart) {
        unlock();
        return false;
    }
    out = pending_commands_;
    pending_commands_ = MqttPendingCommands{};
    unlock();
    return true;
}

void MqttRuntimeState::lock() const {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void MqttRuntimeState::unlock() const {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}
