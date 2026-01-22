// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/StatusMessages.h"

#include <math.h>

namespace StatusMessages {

namespace {

constexpr size_t kMaxMessages = 12;

float compute_dew_point_c(float temp_c, float rh) {
    if (!isfinite(temp_c) || !isfinite(rh) || rh <= 0.0f) {
        return NAN;
    }
    float rh_clamped = fminf(fmaxf(rh, 1.0f), 100.0f);
    constexpr float kA = 17.62f;
    constexpr float kB = 243.12f;
    float gamma = logf(rh_clamped / 100.0f) + (kA * temp_c) / (kB + temp_c);
    return (kB * gamma) / (kA - gamma);
}

} // namespace

StatusMessageResult build_status_messages(const SensorData &data, bool gas_warmup) {
    StatusMessageResult result;

    StatusSeverity co2_sev = STATUS_NONE;
    StatusSeverity voc_sev = STATUS_NONE;
    StatusSeverity hcho_sev = STATUS_NONE;
    StatusSeverity temp_sev = STATUS_NONE;
    StatusSeverity pm25_sev = STATUS_NONE;
    StatusSeverity pm10_sev = STATUS_NONE;
    StatusSeverity nox_sev = STATUS_NONE;
    StatusSeverity hum_sev = STATUS_NONE;
    StatusSeverity dp_sev = STATUS_NONE;

    const char *co2_msg = nullptr;
    const char *voc_msg = nullptr;
    const char *hcho_msg = nullptr;
    const char *temp_msg = nullptr;
    const char *pm25_msg = nullptr;
    const char *pm10_msg = nullptr;
    const char *nox_msg = nullptr;
    const char *hum_msg = nullptr;
    const char *dp_msg = nullptr;

    if (data.co2_valid && data.co2 > 0) {
        result.has_valid = true;
        if (data.co2 >= 1500) {
            co2_sev = STATUS_RED;
            co2_msg = "CO2 Very High! Ventilate now";
        } else if (data.co2 >= 1000) {
            co2_sev = STATUS_ORANGE;
            co2_msg = "CO2 High - Ventilate soon";
        } else if (data.co2 >= 800) {
            co2_sev = STATUS_YELLOW;
            co2_msg = "CO2 Rising - Consider ventilation";
        }
    }

    if (data.pm25_valid && isfinite(data.pm25) && data.pm25 >= 0.0f) {
        result.has_valid = true;
        if (data.pm25 >= 55.0f) {
            pm25_sev = STATUS_RED;
            pm25_msg = "PM2.5 Very High! Purify - stop smoke";
        } else if (data.pm25 >= 35.0f) {
            pm25_sev = STATUS_ORANGE;
            pm25_msg = "PM2.5 High - Purify - check source";
        } else if (data.pm25 >= 12.0f) {
            pm25_sev = STATUS_YELLOW;
            pm25_msg = "PM2.5 Elevated - Consider purifier";
        }
    }

    if (data.pm10_valid && isfinite(data.pm10) && data.pm10 >= 0.0f) {
        result.has_valid = true;
        if (data.pm10 >= 254.0f) {
            pm10_sev = STATUS_RED;
            pm10_msg = "PM10 Very High! Purify - check source";
        } else if (data.pm10 >= 154.0f) {
            pm10_sev = STATUS_ORANGE;
            pm10_msg = "PM10 High - Purify - reduce dust";
        } else if (data.pm10 >= 54.0f) {
            pm10_sev = STATUS_YELLOW;
            pm10_msg = "PM10 Elevated - Purify - reduce dust";
        }
    }

    if (data.hcho_valid && isfinite(data.hcho) && data.hcho >= 0.0f) {
        result.has_valid = true;
        if (data.hcho >= 100.0f) {
            hcho_sev = STATUS_RED;
            hcho_msg = "HCHO Very High! Ventilate - remove source";
        } else if (data.hcho >= 60.0f) {
            hcho_sev = STATUS_ORANGE;
            hcho_msg = "HCHO High - Ventilate - remove source";
        } else if (data.hcho >= 30.0f) {
            hcho_sev = STATUS_YELLOW;
            hcho_msg = "HCHO Detected - Check sources";
        }
    }

    if (!gas_warmup && data.voc_valid && data.voc_index > 0) {
        result.has_valid = true;
        if (data.voc_index >= 250) {
            voc_sev = STATUS_RED;
            voc_msg = "VOC Very High! Find source - vent";
        } else if (data.voc_index >= 151) {
            voc_sev = STATUS_ORANGE;
            voc_msg = "VOC High - Increase ventilation";
        }
    }

    if (!gas_warmup && data.nox_valid && data.nox_index > 0) {
        result.has_valid = true;
        if (data.nox_index >= 200) {
            nox_sev = STATUS_RED;
            nox_msg = "NOx Very High! Stop source - ventilate";
        } else if (data.nox_index >= 100) {
            nox_sev = STATUS_ORANGE;
            nox_msg = "NOx High - Use hood - check source";
        } else if (data.nox_index >= 50) {
            nox_sev = STATUS_YELLOW;
            nox_msg = "NOx Elevated - Check stove/outside";
        }
    }

    if (data.temp_valid && isfinite(data.temperature)) {
        result.has_valid = true;
        const float t = data.temperature;
        if (t < 19.0f) {
            temp_sev = STATUS_RED;
            temp_msg = "Temp Too Cold! Heat now";
        } else if (t < 20.0f) {
            temp_sev = STATUS_ORANGE;
            temp_msg = "Temp Cold - Heat the room";
        } else if (t < 21.0f) {
            temp_sev = STATUS_YELLOW;
            temp_msg = "Temp Slightly Cool - Add heat";
        } else if (t > 27.0f) {
            temp_sev = STATUS_RED;
            temp_msg = "Temp Too Hot! Cool now";
        } else if (t > 26.0f) {
            temp_sev = STATUS_ORANGE;
            temp_msg = "Temp Warm - Ventilate / cool";
        } else if (t > 25.0f) {
            temp_sev = STATUS_YELLOW;
            temp_msg = "Temp Slightly Warm - Increase airflow";
        }
    }

    float dew_c = NAN;
    bool dp_low = false;
    bool dp_high = false;
    if (data.temp_valid && data.hum_valid) {
        dew_c = compute_dew_point_c(data.temperature, data.humidity);
        if (isfinite(dew_c)) {
            if (dew_c < 5.0f) {
                dp_sev = STATUS_RED;
                dp_msg = "DP Very Low! Humidify";
                dp_low = true;
            } else if (dew_c < 10.0f) {
                dp_sev = STATUS_YELLOW;
                dp_msg = "DP Low - Air may feel dry";
                dp_low = true;
            } else if (dew_c > 21.0f) {
                dp_sev = STATUS_RED;
                dp_msg = "DP Muggy! Dehumidify now";
                dp_high = true;
            } else if (dew_c > 18.0f) {
                dp_sev = STATUS_ORANGE;
                dp_msg = "DP Very High - Vent/dehumidify";
                dp_high = true;
            } else if (dew_c > 16.0f) {
                dp_sev = STATUS_YELLOW;
                dp_msg = "DP High - Air may feel muggy";
                dp_high = true;
            }
        }
    }

    if (data.hum_valid && isfinite(data.humidity)) {
        result.has_valid = true;
        const float h = data.humidity;
        if (h < 20.0f) {
            hum_sev = STATUS_RED;
            hum_msg = "Humidity Extremely Low! Humidify";
        } else if (h < 30.0f) {
            hum_sev = STATUS_ORANGE;
            hum_msg = "Humidity Very Low - Use humidifier";
        } else if (h < 40.0f) {
            hum_sev = STATUS_YELLOW;
            hum_msg = "Humidity Low - Consider humidifier";
        } else if (h > 70.0f) {
            hum_sev = STATUS_RED;
            hum_msg = "Humidity Extremely High! Dehumidify";
        } else if (h > 65.0f) {
            hum_sev = STATUS_ORANGE;
            hum_msg = "Humidity Very High - Dehumidify";
        } else if (h > 60.0f) {
            hum_sev = STATUS_YELLOW;
            hum_msg = "Humidity High - Increase airflow";
        }
    }

    if (dp_high && data.hum_valid && data.humidity > 60.0f) {
        hum_sev = STATUS_NONE;
        hum_msg = nullptr;
    }
    if (dp_low && data.hum_valid && data.humidity < 40.0f) {
        dp_sev = STATUS_NONE;
        dp_msg = nullptr;
    }

    auto add_msg = [&](StatusSeverity target_sev, StatusSeverity sensor_sev, StatusSensor sensor, const char *text) {
        if (sensor_sev != target_sev || sensor_sev == STATUS_NONE || !text) return;
        if (result.count >= kMaxMessages) return;
        result.messages[result.count++] = { text, sensor_sev, sensor };
    };

    auto add_by_severity = [&](StatusSeverity sev) {
        add_msg(sev, nox_sev, STATUS_SENSOR_NOX, nox_msg);
        add_msg(sev, hcho_sev, STATUS_SENSOR_HCHO, hcho_msg);
        add_msg(sev, pm25_sev, STATUS_SENSOR_PM25, pm25_msg);
        add_msg(sev, pm10_sev, STATUS_SENSOR_PM10, pm10_msg);
        add_msg(sev, voc_sev, STATUS_SENSOR_VOC, voc_msg);
        add_msg(sev, co2_sev, STATUS_SENSOR_CO2, co2_msg);
        add_msg(sev, temp_sev, STATUS_SENSOR_TEMP, temp_msg);
        add_msg(sev, hum_sev, STATUS_SENSOR_HUM, hum_msg);
        add_msg(sev, dp_sev, STATUS_SENSOR_DP, dp_msg);
    };

    const bool has_red = (co2_sev == STATUS_RED) || (voc_sev == STATUS_RED) || (hcho_sev == STATUS_RED) ||
                         (temp_sev == STATUS_RED) || (pm25_sev == STATUS_RED) || (pm10_sev == STATUS_RED) ||
                         (nox_sev == STATUS_RED) || (hum_sev == STATUS_RED) || (dp_sev == STATUS_RED);
    if (has_red) {
        add_by_severity(STATUS_RED);
    } else {
        add_by_severity(STATUS_ORANGE);
        add_by_severity(STATUS_YELLOW);
    }

    return result;
}

} // namespace StatusMessages
