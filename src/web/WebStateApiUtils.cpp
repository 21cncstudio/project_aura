// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebStateApiUtils.h"

#include <math.h>

#include "core/MathUtils.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "web/WebApiUtils.h"
#include "web/WebOtaApiUtils.h"
#include "web/WebJsonUtils.h"

namespace WebStateApiUtils {

namespace {

void fill_ota_json(ArduinoJson::JsonObject root, const Payload &payload) {
    const WebOtaSnapshot &ota = payload.ota;
    const uint32_t now_ms = payload.timestamp_ms;

    if (ota.session_id != 0) {
        root["session_id"] = ota.session_id;
    } else {
        root["session_id"] = nullptr;
    }
    root["active"] = ota.active;
    root["reboot_pending"] = ota.reboot_pending;
    root["written"] = static_cast<uint32_t>(ota.written_size);
    root["slot_size"] = static_cast<uint32_t>(ota.slot_size);
    root["chunks"] = ota.chunk_count;
    root["total_ms"] = ota.totalDurationMs(now_ms);
    root["transfer_ms"] = ota.transferPhaseMs();
    if (ota.size_known) {
        root["expected"] = static_cast<uint32_t>(ota.expected_size);
    } else {
        root["expected"] = nullptr;
    }

    if (ota.active) {
        root["status"] = "uploading";
        root["message"] = "Upload in progress";
        root["success"] = false;
        root["error"] = nullptr;
        root["error_code"] = nullptr;
        return;
    }

    if (!ota.hasTerminalResult(now_ms)) {
        root["status"] = payload.ota_busy ? "busy" : "idle";
        root["message"] = nullptr;
        root["success"] = false;
        root["error"] = nullptr;
        root["error_code"] = nullptr;
        return;
    }

    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(ota.upload_seen,
                                          ota.success && !ota.hasError(),
                                          ota.written_size,
                                          ota.slot_size,
                                          ota.size_known,
                                          ota.expected_size,
                                          ota.error,
                                          ota.error_code);
    root["success"] = result.success;
    if (result.success) {
        root["status"] = ota.reboot_pending ? "rebooting" : "success";
        root["message"] = result.message;
        root["error"] = nullptr;
        root["error_code"] = nullptr;
        return;
    }

    root["status"] = "failed";
    root["message"] = nullptr;
    root["error"] = result.error;
    root["error_code"] = result.error_code;
}

}  // namespace

void fillJson(ArduinoJson::JsonObject root, const Payload &payload) {
    const SensorData &data = payload.data;

    root["success"] = true;
    root["ota_busy"] = payload.ota_busy;
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
    WebJsonUtils::jsonSetIntOrNull(sensors, "voc",
                                   !payload.gas_warmup && data.voc_valid,
                                   data.voc_index);
    WebJsonUtils::jsonSetIntOrNull(sensors, "nox",
                                   !payload.gas_warmup && data.nox_valid,
                                   data.nox_index);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "hcho", data.hcho_valid, data.hcho);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "co", data.co_valid && data.co_sensor_present, data.co_ppm);
    const DfrOptionalGasSensor::OptionalGasType optional_gas_type =
        static_cast<DfrOptionalGasSensor::OptionalGasType>(data.optional_gas_type);
    const bool optional_gas_present =
        data.optional_gas_sensor_present &&
        optional_gas_type != DfrOptionalGasSensor::OptionalGasType::None;
    const bool optional_gas_valid =
        optional_gas_present &&
        data.optional_gas_valid &&
        isfinite(data.optional_gas_ppm) &&
        data.optional_gas_ppm >= 0.0f;
    WebJsonUtils::jsonSetFloatOrNull(sensors, "optional_gas", optional_gas_valid, data.optional_gas_ppm);
    if (optional_gas_present) {
        const uint8_t optional_gas_decimals =
            data.optional_gas_ppm_decimals <= 2 ? data.optional_gas_ppm_decimals : 1;
        sensors["optional_gas_decimals"] = optional_gas_decimals;
        sensors["optional_gas_ppm_decimals"] = optional_gas_decimals;
        sensors["optional_gas_unit"] = DfrOptionalGasSensor::unitForType(optional_gas_type);
    } else {
        sensors["optional_gas_decimals"] = nullptr;
        sensors["optional_gas_ppm_decimals"] = nullptr;
        sensors["optional_gas_unit"] = nullptr;
    }
    sensors["optional_gas_type"] = optional_gas_present
        ? DfrOptionalGasSensor::optionalGasLabel(optional_gas_type)
        : nullptr;
    WebJsonUtils::jsonSetFloatOrNull(
        sensors,
        "o2",
        optional_gas_valid && optional_gas_type == DfrOptionalGasSensor::OptionalGasType::O2,
        data.optional_gas_ppm);
    WebJsonUtils::jsonSetFloatOrNull(sensors, "nh3", data.nh3_valid && data.nh3_sensor_present, data.nh3_ppm);
    sensors["co_sensor_present"] = data.co_sensor_present;
    sensors["co_warmup"] = data.co_warmup;
    sensors["hcho_sensor_present"] = data.hcho_sensor_present;
    sensors["hcho_warmup"] = data.hcho_sensor_present && data.hcho_warmup;
    sensors["optional_gas_sensor_present"] = optional_gas_present;
    sensors["optional_gas_warmup"] = optional_gas_present && data.optional_gas_warmup;
    sensors["nh3_sensor_present"] = data.nh3_sensor_present;
    sensors["nh3_warmup"] = data.nh3_warmup;
    sensors["gas_warmup"] = payload.gas_warmup;

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

    ArduinoJson::JsonObject ota = root["ota"].to<ArduinoJson::JsonObject>();
    fill_ota_json(ota, payload);

    ArduinoJson::JsonObject system = root["system"].to<ArduinoJson::JsonObject>();
    system["firmware"] = payload.firmware;
    system["build_date"] = payload.build_date;
    system["build_time"] = payload.build_time;
    system["uptime"] = WebApiUtils::formatUptimeHuman(payload.uptime_s);
    system["dac_available"] = payload.dac_available;
    const char *ntp_status = "off";
    if (payload.ntp_active) {
        if (payload.ntp_syncing) {
            ntp_status = "syncing";
        } else if (!payload.ntp_error && payload.ntp_last_sync_ms != 0) {
            ntp_status = "ok";
        } else {
            ntp_status = "error";
        }
    }
    system["ntp_status"] = ntp_status;
    if (payload.ntp_last_sync_ms != 0) {
        system["ntp_last_sync_ms"] = payload.ntp_last_sync_ms;
    } else {
        system["ntp_last_sync_ms"] = nullptr;
    }

    ArduinoJson::JsonObject settings = root["settings"].to<ArduinoJson::JsonObject>();
    WebSettingsUtils::fillSettingsJson(settings, &payload.settings, nullptr);

    ArduinoJson::JsonObject thresholds = root["thresholds"].to<ArduinoJson::JsonObject>();
    thresholds["version"] = DisplayThresholds::kVersion;
    DisplayThresholds::writeMetricsJson(thresholds["metrics"].to<ArduinoJson::JsonObject>(),
                                        payload.thresholds);
    DisplayThresholds::writeBackgroundAlertsJson(
        thresholds["background_alerts"].to<ArduinoJson::JsonObject>(),
        payload.thresholds.background_alerts);
}

} // namespace WebStateApiUtils
