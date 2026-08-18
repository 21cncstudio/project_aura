// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include "config/AppData.h"
#include "modules/FanStateSnapshot.h"

struct MqttRuntimeSnapshot {
    SensorData data;
    FanStateSnapshot fan;
    bool gas_warmup = false;
    bool night_mode = false;
    bool alert_blink = false;
    bool backlight_on = false;
    bool backlight_transition_pending = false;
    bool backlight_target_on = false;
    bool auto_night_enabled = false;
};

enum class FanHaMode : uint8_t {
    Auto = 0,
    Stopped,
    Manual,
};

struct MqttPendingCommands {
    bool night_mode = false;
    bool night_mode_value = false;
    bool alert_blink = false;
    bool alert_blink_value = false;
    bool backlight = false;
    bool backlight_value = false;
    bool fan_mode = false;
    FanHaMode fan_mode_value = FanHaMode::Stopped;
    bool fan_manual_speed = false;
    uint8_t fan_manual_speed_value = 1;
    bool fan_timer = false;
    uint32_t fan_timer_seconds = 0;
    bool restart = false;
};

class MqttRuntimeState {
public:
    MqttRuntimeState();

    void update(const SensorData &data,
                const FanStateSnapshot &fan,
                bool gas_warmup,
                bool night_mode,
                bool alert_blink,
                bool backlight_on,
                bool backlight_transition_pending,
                bool backlight_target_on,
                bool auto_night_enabled);
    MqttRuntimeSnapshot snapshot() const;

    void requestPublish();
    bool consumePublishRequest();
    void mergePendingCommands(const MqttPendingCommands &pending);
    bool takePendingCommands(MqttPendingCommands &out);

private:
    void lock() const;
    void unlock() const;

#ifdef UNIT_TEST
    mutable std::mutex mutex_{};
#else
    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
    MqttRuntimeSnapshot snapshot_{};
    MqttPendingCommands pending_commands_{};
    std::atomic<bool> publish_after_update_{false};
    std::atomic<bool> publish_requested_{false};
};
