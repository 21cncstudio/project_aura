// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/MqttRuntimeState.h"

MqttRuntimeState::MqttRuntimeState() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
#endif
}

void MqttRuntimeState::update(const SensorData &data,
                              const FanStateSnapshot &fan,
                              bool gas_warmup,
                              bool night_mode,
                              bool alert_blink,
                              bool backlight_on,
                              bool backlight_transition_pending,
                              bool backlight_target_on,
                              bool auto_night_enabled) {
    lock();
    snapshot_.data = data;
    snapshot_.fan = fan;
    snapshot_.gas_warmup = gas_warmup;
    snapshot_.night_mode = night_mode;
    snapshot_.alert_blink = alert_blink;
    snapshot_.backlight_on = backlight_on;
    snapshot_.backlight_transition_pending =
        backlight_transition_pending;
    snapshot_.backlight_target_on = backlight_target_on;
    snapshot_.auto_night_enabled = auto_night_enabled;
    unlock();

    // Publish requests are released only after a fresh runtime snapshot is stored.
    // This prevents the network task from publishing stale state immediately after
    // a command was applied on the UI thread.
    if (publish_after_update_.exchange(false, std::memory_order_acq_rel)) {
        publish_requested_.store(true, std::memory_order_release);
    }
}

MqttRuntimeSnapshot MqttRuntimeState::snapshot() const {
    lock();
    MqttRuntimeSnapshot copy = snapshot_;
    unlock();
    return copy;
}

void MqttRuntimeState::requestPublish() {
    publish_after_update_.store(true, std::memory_order_release);
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
    if (pending.fan_mode) {
        pending_commands_.fan_mode = true;
        pending_commands_.fan_mode_value = pending.fan_mode_value;
    }
    if (pending.fan_manual_speed) {
        pending_commands_.fan_manual_speed = true;
        pending_commands_.fan_manual_speed_value = pending.fan_manual_speed_value;
    }
    if (pending.fan_timer) {
        pending_commands_.fan_timer = true;
        pending_commands_.fan_timer_seconds = pending.fan_timer_seconds;
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
        !pending_commands_.fan_mode &&
        !pending_commands_.fan_manual_speed &&
        !pending_commands_.fan_timer &&
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
#ifdef UNIT_TEST
    mutex_.lock();
#else
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
#endif
}

void MqttRuntimeState::unlock() const {
#ifdef UNIT_TEST
    mutex_.unlock();
#else
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}
