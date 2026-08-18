// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>

#include "config/AppData.h"
#include "modules/FanStateSnapshot.h"

namespace MqttPayloadBuilder {

String buildDiscoveryEntityObjectId(const String &base_topic,
                                    const char *object_id);

String buildDiscoverySensorPayload(const String &device_id,
                                   const String &device_name,
                                   const String &base_topic,
                                   const char *object_id,
                                   const char *entity_object_id,
                                   const char *name,
                                   const char *unit,
                                   const char *device_class,
                                   const char *state_class,
                                   const char *value_template,
                                   const char *icon);

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on);

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on);

String buildStatePayload(const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on);

String buildStatePayload(const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m);

String buildStatePayload(const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on);

String buildStatePayload(const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m);

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m);

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool backlight_transition_pending,
                         bool backlight_target_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m);

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m);

} // namespace MqttPayloadBuilder

