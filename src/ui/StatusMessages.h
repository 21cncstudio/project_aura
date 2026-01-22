// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <cstddef>
#include <cstdint>

#include "config/AppData.h"

namespace StatusMessages {

enum StatusSeverity : uint8_t {
    STATUS_NONE = 0,
    STATUS_YELLOW = 1,
    STATUS_ORANGE = 2,
    STATUS_RED = 3,
};

enum StatusSensor : uint8_t {
    STATUS_SENSOR_NOX = 0,
    STATUS_SENSOR_HCHO,
    STATUS_SENSOR_PM25,
    STATUS_SENSOR_PM10,
    STATUS_SENSOR_VOC,
    STATUS_SENSOR_CO2,
    STATUS_SENSOR_TEMP,
    STATUS_SENSOR_HUM,
    STATUS_SENSOR_DP,
};

struct StatusMessage {
    const char *text;
    StatusSeverity severity;
    StatusSensor sensor;
};

struct StatusMessageResult {
    bool has_valid = false;
    size_t count = 0;
    StatusMessage messages[12] = {};
};

StatusMessageResult build_status_messages(const SensorData &data, bool gas_warmup);

} // namespace StatusMessages
