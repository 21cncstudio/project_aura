// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

#include "modules/DacAutoConfig.h"

namespace WebDacApiUtils {

struct DacStateSnapshot {
    bool available = false;
    bool faulted = false;
    bool running = false;
    bool manual_override = false;
    bool auto_resume_blocked = false;
    bool output_known = false;
    bool auto_mode = false;
    uint8_t manual_step = 1;
    uint32_t selected_timer_s = 0;
    uint16_t output_mv = 0;
    uint32_t stop_at_ms = 0;
    DacAutoConfig auto_config{};
};

struct SensorSnapshot {
    int co2 = 0;
    bool co2_valid = false;
    float co_ppm = 0.0f;
    bool co_valid = false;
    float pm05 = 0.0f;
    bool pm05_valid = false;
    float pm1 = 0.0f;
    bool pm1_valid = false;
    float pm4 = 0.0f;
    bool pm4_valid = false;
    float pm25 = 0.0f;
    bool pm25_valid = false;
    float pm10 = 0.0f;
    bool pm10_valid = false;
    float hcho = 0.0f;
    bool hcho_valid = false;
    int voc_index = 0;
    bool voc_valid = false;
    int nox_index = 0;
    bool nox_valid = false;
};

struct StatePayload {
    DacStateSnapshot dac{};
    SensorSnapshot sensors{};
    bool gas_warmup = false;
    uint32_t now_ms = 0;
};

void fillStateJson(ArduinoJson::JsonObject root, const StatePayload &payload);

struct DacActionUpdate {
    enum class Type : uint8_t {
        SetMode = 0,
        SetManualStep,
        SetTimerSeconds,
        Start,
        Stop,
        StartAuto,
    };

    Type type = Type::Start;
    bool auto_mode = false;
    uint8_t manual_step = 1;
    uint32_t timer_seconds = 0;
};

struct DacActionParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    DacActionUpdate update{};
};

DacActionParseResult parseActionRequestBody(const String &body);
DacActionParseResult parseActionUpdate(ArduinoJson::JsonVariantConst root);

struct DacAutoUpdate {
    DacAutoConfig config{};
    bool rearm = false;
};

struct DacAutoParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    DacAutoUpdate update{};
};

DacAutoParseResult parseAutoRequestBody(const String &body, const DacAutoConfig &current_config);
DacAutoParseResult parseAutoUpdate(ArduinoJson::JsonVariantConst root,
                                   const DacAutoConfig &current_config);
void fillSuccessJson(ArduinoJson::JsonObject root);

} // namespace WebDacApiUtils
