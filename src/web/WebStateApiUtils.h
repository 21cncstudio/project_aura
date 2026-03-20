// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

#include "config/AppData.h"
#include "web/WebNetworkUtils.h"
#include "web/WebSettingsUtils.h"

namespace WebStateApiUtils {

struct Payload {
    SensorData data{};
    uint32_t uptime_s = 0;
    uint32_t timestamp_ms = 0;
    bool has_time_epoch = false;
    int64_t time_epoch_s = 0;
    WebNetworkUtils::Snapshot network{};
    WebSettingsUtils::SettingsSnapshot settings{};
    bool dac_available = false;
    String firmware;
    String build_date;
    String build_time;
};

void fillJson(ArduinoJson::JsonObject root, const Payload &payload);

} // namespace WebStateApiUtils
