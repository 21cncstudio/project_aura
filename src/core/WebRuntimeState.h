// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config/AppData.h"
#include "modules/FanControl.h"

struct WebRuntimeSnapshot {
    SensorData data;
    bool gas_warmup = false;
    FanControl::Snapshot fan{};
};

class WebRuntimeState {
public:
    WebRuntimeState();

    void update(const SensorData &data, bool gas_warmup, const FanControl &fan_control);
    WebRuntimeSnapshot snapshot() const;

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    WebRuntimeSnapshot snapshot_{};
};
