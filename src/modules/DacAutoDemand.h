// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/DacAutoConfig.h"
#include "modules/DisplayThresholds.h"

struct SensorData;

namespace DacAutoDemand {

enum class Sensor : uint8_t {
    None = 0,
    CO2,
    CO,
    PM05,
    PM1,
    PM4,
    PM25,
    PM10,
    HCHO,
    VOC,
    NOX,
};

struct Result {
    uint8_t percent = 0;
    Sensor sensor = Sensor::None;
    float value = 0.0f;
};

Result evaluate(const DacAutoConfig &config,
                const SensorData &data,
                bool gas_warmup,
                const DisplayThresholds::Config &thresholds);

const char *sensorLabel(Sensor sensor);
void formatSensorValue(Sensor sensor, float value, char *out, size_t out_len);

}  // namespace DacAutoDemand
