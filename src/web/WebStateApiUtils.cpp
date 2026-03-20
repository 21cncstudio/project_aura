// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebStateApiUtils.h"

#include <math.h>

#include "core/MathUtils.h"
#include "web/WebApiUtils.h"
#include "web/WebJsonUtils.h"

namespace WebStateApiUtils {

void fillJson(ArduinoJson::JsonObject root, const Payload &payload) {
    const SensorData &data = payload.data;

    root["success"] = true;
    root["ota_busy"] = false;
    root["uptime_s"] = payload.uptime_s;
    root["timestamp_ms"] = payload.timestamp_ms;
    if (payload.has_time_epoch) {
        root["time_epoch_s"] = payload.time_epoch_s;
    } else {
        root["time_epoch_s"] = nullptr;
    }

    ArduinoJson::JsonObject sensors = root["sensors"].to<ArduinoJson::JsonObject>();
    WebJsonUtils::jsonSetFloatOrNull(sensors, "temp", data.temp_valid, data.temperature);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "rh", data.hum_valid, data.humidity);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pressure", data.pressure_valid, data.pressure);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pm05", data.pm05_valid, data.pm05);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pm1", data.pm1_valid, data.pm1);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pm25", data.pm25_valid, data.pm25);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pm4", data.pm4_valid, data.pm4);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "pm10", data.pm10_valid, data.pm10);
    WebJsonUtils::jsonSetIntOrNull(sensors, "co2", data.co2_valid, data.co2);
    WebJsonUtils::jsonSetIntOrNull(sensors, "voc", data.voc_valid, data.voc_index);
    WebJsonUtils::jsonSetIntOrNull(sensors, "nox", data.nox_valid, data.nox_index);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "hcho", data.hcho_valid, data.hcho);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "co", data.co_valid && data.co_sensor_present, data.co_ppm);
    sensors["co_sensor_present"] = data.co_sensor_present;
    sensors["co_warmup"] = data.co_warmup;

    ArduinoJson::JsonObject derived = root["derived"].to<ArduinoJson::JsonObject>();
    const bool climate_valid = data.temp_valid && data.hum_valid;
    const float dew_point =
        climate_valid ? MathUtils::compute_dew_point_c(data.temperature, data.humidity) : NAN;
    const float abs_humidity =
        climate_valid ? MathUtils::compute_absolute_humidity_gm3(data.temperature, data.humidity) : NAN;
    const int mold_risk =
        climate_valid ? MathUtils::compute_mold_risk_index(data.temperature, data.humidity) : -1;
    WebJsonUtils::jsonSetFloatOrNull(derived, "dew_point", climate_valid, dew_point);
    WebJsonUtils::jsonSetFloatOrNull(derived, "ah", climate_valid, abs_humidity);
    if (mold_risk >= 0) {
        derived["mold"] = mold_risk;
    } else {
        derived["mold"] = nullptr;
    }
    WebJsonUtils::jsonSetFloatOrNull(
        derived, "pressure_delta_3h", data.pressure_delta_3h_valid, data.pressure_delta_3h);
    WebJsonUtils::jsonSetFloatOrNull(
        derived, "pressure_delta_24h", data.pressure_delta_24h_valid, data.pressure_delta_24h);
    derived["uptime"] = WebApiUtils::formatUptimeHuman(payload.uptime_s);

    ArduinoJson::JsonObject network = root["network"].to<ArduinoJson::JsonObject>();
    WebNetworkUtils::fillStateJson(network, payload.network);

    ArduinoJson::JsonObject system = root["system"].to<ArduinoJson::JsonObject>();
    system["firmware"] = payload.firmware;
    system["build_date"] = payload.build_date;
    system["build_time"] = payload.build_time;
    system["uptime"] = WebApiUtils::formatUptimeHuman(payload.uptime_s);
    system["dac_available"] = payload.dac_available;

    ArduinoJson::JsonObject settings = root["settings"].to<ArduinoJson::JsonObject>();
    WebSettingsUtils::fillSettingsJson(settings, &payload.settings, nullptr);
}

} // namespace WebStateApiUtils
