// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/DacAutoDemand.h"

#include <math.h>
#include <stdio.h>

#include "config/AppConfig.h"
#include "config/AppData.h"

namespace DacAutoDemand {
namespace {

uint8_t percent_for_band(const DacAutoSensorConfig &sensor, DisplayThresholds::Band band) {
    if (!sensor.enabled) {
        return 0;
    }
    switch (band) {
        case DisplayThresholds::Band::Green:
            return sensor.band.green_percent;
        case DisplayThresholds::Band::Yellow:
            return sensor.band.yellow_percent;
        case DisplayThresholds::Band::Orange:
            return sensor.band.orange_percent;
        case DisplayThresholds::Band::Red:
            return sensor.band.red_percent;
        case DisplayThresholds::Band::Invalid:
        default:
            return 0;
    }
}

uint8_t percent_for_high(const DacAutoSensorConfig &sensor,
                         bool valid,
                         float value,
                         const DisplayThresholds::High &thresholds) {
    if (!valid) {
        return 0;
    }
    return percent_for_band(sensor, DisplayThresholds::classifyHigh(value, thresholds));
}

void consider(Result &result, uint8_t percent, Sensor sensor, float value) {
    if (percent <= result.percent) {
        return;
    }
    result.percent = percent;
    result.sensor = sensor;
    result.value = value;
}

}  // namespace

Result evaluate(const DacAutoConfig &config,
                const SensorData &data,
                bool gas_warmup,
                const DisplayThresholds::Config &thresholds) {
    Result result{};
    if (!config.enabled) {
        return result;
    }

    const bool co2_valid = data.co2_valid && data.co2 > 0;
    consider(result,
             percent_for_high(config.co2, co2_valid, static_cast<float>(data.co2), thresholds.co2),
             Sensor::CO2,
             static_cast<float>(data.co2));

    const bool co_valid = data.co_sensor_present &&
                          data.co_valid &&
                          isfinite(data.co_ppm) &&
                          data.co_ppm >= 0.0f;
    consider(result,
             percent_for_high(config.co, co_valid, data.co_ppm, thresholds.co),
             Sensor::CO,
             data.co_ppm);

    const DisplayThresholds::High pm05_thresholds = {
        Config::AQ_PM05_GREEN_MAX_PPCM3,
        Config::AQ_PM05_YELLOW_MAX_PPCM3,
        Config::AQ_PM05_ORANGE_MAX_PPCM3,
    };
    const bool pm05_valid = data.pm05_valid && isfinite(data.pm05) && data.pm05 >= 0.0f;
    consider(result,
             percent_for_high(config.pm05, pm05_valid, data.pm05, pm05_thresholds),
             Sensor::PM05,
             data.pm05);

    const DisplayThresholds::High pm1_thresholds = {
        Config::AQ_PM1_GREEN_MAX_UGM3,
        Config::AQ_PM1_YELLOW_MAX_UGM3,
        Config::AQ_PM1_ORANGE_MAX_UGM3,
    };
    const bool pm1_valid = data.pm1_valid && isfinite(data.pm1) && data.pm1 >= 0.0f;
    consider(result,
             percent_for_high(config.pm1, pm1_valid, data.pm1, pm1_thresholds),
             Sensor::PM1,
             data.pm1);

    const DisplayThresholds::High pm4_thresholds = {
        Config::AQ_PM4_GREEN_MAX_UGM3,
        Config::AQ_PM4_YELLOW_MAX_UGM3,
        Config::AQ_PM4_ORANGE_MAX_UGM3,
    };
    const bool pm4_valid = data.pm4_valid && isfinite(data.pm4) && data.pm4 >= 0.0f;
    consider(result,
             percent_for_high(config.pm4, pm4_valid, data.pm4, pm4_thresholds),
             Sensor::PM4,
             data.pm4);

    const DisplayThresholds::High pm25_thresholds = {
        Config::AQ_PM25_GREEN_MAX_UGM3,
        Config::AQ_PM25_YELLOW_MAX_UGM3,
        Config::AQ_PM25_ORANGE_MAX_UGM3,
    };
    const bool pm25_valid = data.pm25_valid && isfinite(data.pm25) && data.pm25 >= 0.0f;
    consider(result,
             percent_for_high(config.pm25, pm25_valid, data.pm25, pm25_thresholds),
             Sensor::PM25,
             data.pm25);

    const DisplayThresholds::High pm10_thresholds = {
        Config::AQ_PM10_GREEN_MAX_UGM3,
        Config::AQ_PM10_YELLOW_MAX_UGM3,
        Config::AQ_PM10_ORANGE_MAX_UGM3,
    };
    const bool pm10_valid = data.pm10_valid && isfinite(data.pm10) && data.pm10 >= 0.0f;
    consider(result,
             percent_for_high(config.pm10, pm10_valid, data.pm10, pm10_thresholds),
             Sensor::PM10,
             data.pm10);

    const bool hcho_valid = data.hcho_valid && isfinite(data.hcho) && data.hcho >= 0.0f;
    consider(result,
             percent_for_high(config.hcho, hcho_valid, data.hcho, thresholds.hcho),
             Sensor::HCHO,
             data.hcho);

    const bool voc_valid = !gas_warmup && data.voc_valid && data.voc_index >= 0;
    consider(result,
             percent_for_high(config.voc, voc_valid, static_cast<float>(data.voc_index), thresholds.voc),
             Sensor::VOC,
             static_cast<float>(data.voc_index));

    const bool nox_valid = !gas_warmup && data.nox_valid && data.nox_index >= 0;
    consider(result,
             percent_for_high(config.nox, nox_valid, static_cast<float>(data.nox_index), thresholds.nox),
             Sensor::NOX,
             static_cast<float>(data.nox_index));

    return result;
}

const char *sensorLabel(Sensor sensor) {
    switch (sensor) {
        case Sensor::CO2:
            return "CO2:";
        case Sensor::CO:
            return "CO:";
        case Sensor::PM05:
            return "PM0.5:";
        case Sensor::PM1:
            return "PM1.0:";
        case Sensor::PM4:
            return "PM4.0:";
        case Sensor::PM25:
            return "PM2.5:";
        case Sensor::PM10:
            return "PM10:";
        case Sensor::HCHO:
            return "HCHO:";
        case Sensor::VOC:
            return "VOC:";
        case Sensor::NOX:
            return "NOx:";
        case Sensor::None:
        default:
            return "--";
    }
}

void formatSensorValue(Sensor sensor, float value, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return;
    }
    switch (sensor) {
        case Sensor::CO2:
            snprintf(out, out_len, "%.0f ppm", value);
            break;
        case Sensor::CO:
            snprintf(out, out_len, "%.1f ppm", value);
            break;
        case Sensor::PM05:
            snprintf(out, out_len, "%.0f #/cm3", value);
            break;
        case Sensor::PM1:
        case Sensor::PM4:
        case Sensor::PM25:
        case Sensor::PM10:
            snprintf(out, out_len, "%.1f ug/m3", value);
            break;
        case Sensor::HCHO:
            snprintf(out, out_len, "%.0f ppb", value);
            break;
        case Sensor::VOC:
        case Sensor::NOX:
            snprintf(out, out_len, "%.0f idx", value);
            break;
        case Sensor::None:
        default:
            snprintf(out, out_len, "%s", "--");
            break;
    }
}

}  // namespace DacAutoDemand
