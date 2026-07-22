// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drivers/DfrOptionalGasSensor.h"

namespace UiOptionalGasProfile {

using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;

enum class ClassificationMode : uint8_t {
    Increasing = 0,
    NormalRange,
};

enum class Band : uint8_t {
    Green = 0,
    Yellow,
    Orange,
    Red,
    Inactive,
};

struct Profile {
    OptionalGasType type = OptionalGasType::None;
    const char *label = "Gas";
    const char *title = "Optional gas";
    uint8_t value_decimals = 1;
    uint8_t threshold_decimals = 1;
    float green_max_ppm = 0.0f;
    float yellow_max_ppm = 0.0f;
    float orange_max_ppm = 0.0f;
    float graph_fallback_ppm = 0.0f;
    float graph_min_span_ppm = 1.0f;
    float graph_scale = 100.0f;
    const char *unit = "ppm";
    ClassificationMode classification = ClassificationMode::Increasing;
    float normal_min = 0.0f;
    float normal_max = 0.0f;
    const char *description = "Optional DFRobot electrochemical gas sensor.";
};

const Profile &forType(OptionalGasType type);
bool isKnown(OptionalGasType type);
Band classify(const Profile &profile, float value, bool valid);
void formatValue(const Profile &profile, float ppm, char *buf, size_t buf_size);
void formatValue(const Profile &profile, float ppm, uint8_t decimals, char *buf, size_t buf_size);
void formatThreshold(const Profile &profile, float ppm, char *buf, size_t buf_size);
void formatBandLabel(const Profile &profile, uint8_t band, char *buf, size_t buf_size);

} // namespace UiOptionalGasProfile
