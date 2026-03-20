// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config/AppData.h"

struct MqttRuntimeSnapshot {
    SensorData data;
    bool night_mode = false;
    bool alert_blink = false;
    bool backlight_on = false;
    bool auto_night_enabled = false;
};

struct MqttPendingCommands {
    bool night_mode = false;
    bool night_mode_value = false;
    bool alert_blink = false;
    bool alert_blink_value = false;
    bool backlight = false;
    bool backlight_value = false;
    bool restart = false;
};

class MqttRuntimeState {
public:
    MqttRuntimeState();

    void update(const SensorData &data,
                bool night_mode,
                bool alert_blink,
                bool backlight_on,
                bool auto_night_enabled);
    MqttRuntimeSnapshot snapshot() const;

    void requestPublish();
    bool consumePublishRequest();
    void mergePendingCommands(const MqttPendingCommands &pending);
    bool takePendingCommands(MqttPendingCommands &out);

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    MqttRuntimeSnapshot snapshot_{};
    MqttPendingCommands pending_commands_{};
    std::atomic<bool> publish_requested_{false};
};
