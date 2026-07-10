// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/StatusMessages.h"
#include "ui/UiStrings.h"
#include "core/MathUtils.h"

#include <math.h>

namespace StatusMessages {

namespace {

constexpr size_t kMaxMessages = 12;

StatusSeverity severity_from_band(DisplayThresholds::Band band) {
    switch (band) {
        case DisplayThresholds::Band::Yellow:
            return STATUS_YELLOW;
        case DisplayThresholds::Band::Orange:
            return STATUS_ORANGE;
        case DisplayThresholds::Band::Red:
            return STATUS_RED;
        case DisplayThresholds::Band::Green:
        case DisplayThresholds::Band::Invalid:
        default:
            return STATUS_NONE;
    }
}

StatusSeverity classify_high_severity(float value, const DisplayThresholds::High &thresholds) {
    return severity_from_band(DisplayThresholds::classifyHigh(value, thresholds));
}

StatusSeverity classify_range_severity(float value, const DisplayThresholds::Range &thresholds) {
    return severity_from_band(DisplayThresholds::classifyRange(value, thresholds));
}

bool is_low_side(float value, const DisplayThresholds::Range &thresholds) {
    return isfinite(value) && value < thresholds.good_min;
}

} // namespace

using UiStrings::TextId;
using UiStrings::text;

StatusMessageResult build_status_messages(const SensorData &data,
                                          bool gas_warmup,
                                          const DisplayThresholds::Config &thresholds) {
    StatusMessageResult result;

    StatusSeverity co2_sev = STATUS_NONE;
    StatusSeverity co_sev = STATUS_NONE;
    StatusSeverity voc_sev = STATUS_NONE;
    StatusSeverity hcho_sev = STATUS_NONE;
    StatusSeverity temp_sev = STATUS_NONE;
    StatusSeverity pm25_sev = STATUS_NONE;
    StatusSeverity pm1_sev = STATUS_NONE;
    StatusSeverity pm10_sev = STATUS_NONE;
    StatusSeverity nox_sev = STATUS_NONE;
    StatusSeverity hum_sev = STATUS_NONE;
    StatusSeverity dp_sev = STATUS_NONE;
    StatusSeverity ah_sev = STATUS_NONE;

    const char *co2_msg = nullptr;
    const char *co_msg = nullptr;
    const char *voc_msg = nullptr;
    const char *hcho_msg = nullptr;
    const char *temp_msg = nullptr;
    const char *pm25_msg = nullptr;
    const char *pm1_msg = nullptr;
    const char *pm10_msg = nullptr;
    const char *nox_msg = nullptr;
    const char *hum_msg = nullptr;
    const char *dp_msg = nullptr;
    const char *ah_msg = nullptr;

    if (data.co2_valid && data.co2 > 0) {
        result.has_valid = true;
        co2_sev = classify_high_severity(static_cast<float>(data.co2), thresholds.co2);
        if (co2_sev == STATUS_RED) {
            co2_msg = text(TextId::MsgCo2VeryHigh);
        } else if (co2_sev == STATUS_ORANGE) {
            co2_msg = text(TextId::MsgCo2High);
        } else if (co2_sev == STATUS_YELLOW) {
            co2_msg = text(TextId::MsgCo2Rising);
        }
    }

    if (data.co_sensor_present &&
        data.co_valid &&
        isfinite(data.co_ppm) &&
        data.co_ppm >= 0.0f) {
        result.has_valid = true;
        co_sev = classify_high_severity(data.co_ppm, thresholds.co);
        if (co_sev == STATUS_RED) {
            co_msg = text(TextId::MsgCoDanger);
        } else if (co_sev == STATUS_ORANGE) {
            co_msg = text(TextId::MsgCoElevated);
        } else if (co_sev == STATUS_YELLOW) {
            co_msg = text(TextId::MsgCoDetected);
        }
    }

    // Safety override: when CO warning is active, show only CO status.
    if (co_sev != STATUS_NONE && co_msg) {
        result.count = 1;
        result.messages[0] = {co_msg, co_sev, STATUS_SENSOR_CO};
        return result;
    }

    if (data.pm25_valid && isfinite(data.pm25) && data.pm25 >= 0.0f) {
        result.has_valid = true;
        if (data.pm25 >= 55.0f) {
            pm25_sev = STATUS_RED;
            pm25_msg = text(TextId::MsgPm25VeryHigh);
        } else if (data.pm25 >= 35.0f) {
            pm25_sev = STATUS_ORANGE;
            pm25_msg = text(TextId::MsgPm25High);
        } else if (data.pm25 >= 12.0f) {
            pm25_sev = STATUS_YELLOW;
            pm25_msg = text(TextId::MsgPm25Elevated);
        }
    }

    if (data.pm1_valid && isfinite(data.pm1) && data.pm1 >= 0.0f) {
        result.has_valid = true;
        if (data.pm1 >= 50.0f) {
            pm1_sev = STATUS_RED;
            pm1_msg = text(TextId::MsgPm1VeryHigh);
        } else if (data.pm1 >= 25.0f) {
            pm1_sev = STATUS_ORANGE;
            pm1_msg = text(TextId::MsgPm1High);
        } else if (data.pm1 >= 10.0f) {
            pm1_sev = STATUS_YELLOW;
            pm1_msg = text(TextId::MsgPm1Elevated);
        }
    }

    if (data.pm10_valid && isfinite(data.pm10) && data.pm10 >= 0.0f) {
        result.has_valid = true;
        if (data.pm10 >= 254.0f) {
            pm10_sev = STATUS_RED;
            pm10_msg = text(TextId::MsgPm10VeryHigh);
        } else if (data.pm10 >= 154.0f) {
            pm10_sev = STATUS_ORANGE;
            pm10_msg = text(TextId::MsgPm10High);
        } else if (data.pm10 >= 54.0f) {
            pm10_sev = STATUS_YELLOW;
            pm10_msg = text(TextId::MsgPm10Elevated);
        }
    }

    if (data.hcho_valid && isfinite(data.hcho) && data.hcho >= 0.0f) {
        result.has_valid = true;
        hcho_sev = classify_high_severity(data.hcho, thresholds.hcho);
        if (hcho_sev == STATUS_RED) {
            hcho_msg = text(TextId::MsgHchoVeryHigh);
        } else if (hcho_sev == STATUS_ORANGE) {
            hcho_msg = text(TextId::MsgHchoHigh);
        } else if (hcho_sev == STATUS_YELLOW) {
            hcho_msg = text(TextId::MsgHchoDetected);
        }
    }

    if (!gas_warmup && data.voc_valid && data.voc_index > 0) {
        result.has_valid = true;
        voc_sev = classify_high_severity(static_cast<float>(data.voc_index), thresholds.voc);
        if (voc_sev == STATUS_RED) {
            voc_msg = text(TextId::MsgVocVeryHigh);
        } else if (voc_sev == STATUS_ORANGE) {
            voc_msg = text(TextId::MsgVocHigh);
        } else if (voc_sev == STATUS_YELLOW) {
            voc_msg = text(TextId::MsgVocHigh);
        }
    }

    if (!gas_warmup && data.nox_valid && data.nox_index > 0) {
        result.has_valid = true;
        nox_sev = classify_high_severity(static_cast<float>(data.nox_index), thresholds.nox);
        if (nox_sev == STATUS_RED) {
            nox_msg = text(TextId::MsgNoxVeryHigh);
        } else if (nox_sev == STATUS_ORANGE) {
            nox_msg = text(TextId::MsgNoxHigh);
        } else if (nox_sev == STATUS_YELLOW) {
            nox_msg = text(TextId::MsgNoxElevated);
        }
    }

    if (data.temp_valid && isfinite(data.temperature)) {
        result.has_valid = true;
        const float t = data.temperature;
        temp_sev = classify_range_severity(t, thresholds.temp);
        const bool low = is_low_side(t, thresholds.temp);
        if (temp_sev == STATUS_RED) {
            temp_msg = text(low ? TextId::MsgTempTooCold : TextId::MsgTempTooHot);
        } else if (temp_sev == STATUS_ORANGE) {
            temp_msg = text(low ? TextId::MsgTempCold : TextId::MsgTempWarm);
        } else if (temp_sev == STATUS_YELLOW) {
            temp_msg = text(low ? TextId::MsgTempSlightlyCool : TextId::MsgTempSlightlyWarm);
        }
    }

    float dew_c = NAN;
    bool dp_low = false;
    bool dp_high = false;
    if (data.temp_valid && data.hum_valid) {
        dew_c = MathUtils::compute_dew_point_c(data.temperature, data.humidity);
        if (isfinite(dew_c)) {
            dp_sev = classify_range_severity(dew_c, thresholds.dew_point);
            dp_low = dp_sev != STATUS_NONE && is_low_side(dew_c, thresholds.dew_point);
            dp_high = dp_sev != STATUS_NONE && !dp_low;
            if (dp_low) {
                dp_msg = text(dp_sev == STATUS_RED ? TextId::MsgDewPointVeryLow : TextId::MsgDewPointLow);
            } else if (dp_high) {
                if (dp_sev == STATUS_RED) {
                    dp_msg = text(TextId::MsgDewPointMuggy);
                } else if (dp_sev == STATUS_ORANGE) {
                    dp_msg = text(TextId::MsgDewPointVeryHigh);
                } else if (dp_sev == STATUS_YELLOW) {
                    dp_msg = text(TextId::MsgDewPointHigh);
                }
            }
        }
    }

    if (data.hum_valid && isfinite(data.humidity)) {
        result.has_valid = true;
        const float h = data.humidity;
        hum_sev = classify_range_severity(h, thresholds.rh);
        const bool low = is_low_side(h, thresholds.rh);
        if (hum_sev == STATUS_RED) {
            hum_msg = text(low ? TextId::MsgHumidityExtremelyLow : TextId::MsgHumidityExtremelyHigh);
        } else if (hum_sev == STATUS_ORANGE) {
            hum_msg = text(low ? TextId::MsgHumidityVeryLow : TextId::MsgHumidityVeryHigh);
        } else if (hum_sev == STATUS_YELLOW) {
            hum_msg = text(low ? TextId::MsgHumidityLow : TextId::MsgHumidityHigh);
        }
    }

    if (data.temp_valid && data.hum_valid) {
        float ah_gm3 = MathUtils::compute_absolute_humidity_gm3(data.temperature, data.humidity);
        if (isfinite(ah_gm3)) {
            result.has_valid = true;
            ah_sev = classify_range_severity(ah_gm3, thresholds.ah);
            const bool low = is_low_side(ah_gm3, thresholds.ah);
            if (ah_sev == STATUS_RED) {
                ah_msg = text(low ? TextId::MsgAhVeryLow : TextId::MsgAhVeryHigh);
            } else if (ah_sev == STATUS_ORANGE || ah_sev == STATUS_YELLOW) {
                ah_msg = text(low ? TextId::MsgAhLow : TextId::MsgAhHigh);
            }
        }
    }

    if (dp_high && data.hum_valid && data.humidity > thresholds.rh.good_max) {
        hum_sev = STATUS_NONE;
        hum_msg = nullptr;
    }
    if (dp_low && data.hum_valid && data.humidity < thresholds.rh.good_min) {
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
        add_msg(sev, pm1_sev, STATUS_SENSOR_PM1, pm1_msg);
        add_msg(sev, pm10_sev, STATUS_SENSOR_PM10, pm10_msg);
        add_msg(sev, voc_sev, STATUS_SENSOR_VOC, voc_msg);
        add_msg(sev, co_sev, STATUS_SENSOR_CO, co_msg);
        add_msg(sev, co2_sev, STATUS_SENSOR_CO2, co2_msg);
        add_msg(sev, temp_sev, STATUS_SENSOR_TEMP, temp_msg);
        add_msg(sev, hum_sev, STATUS_SENSOR_HUM, hum_msg);
        add_msg(sev, ah_sev, STATUS_SENSOR_AH, ah_msg);
        add_msg(sev, dp_sev, STATUS_SENSOR_DP, dp_msg);
    };

    const bool has_red = (co_sev == STATUS_RED) || (co2_sev == STATUS_RED) ||
                         (voc_sev == STATUS_RED) || (hcho_sev == STATUS_RED) ||
                         (temp_sev == STATUS_RED) || (pm25_sev == STATUS_RED) || (pm1_sev == STATUS_RED) ||
                         (pm10_sev == STATUS_RED) || (nox_sev == STATUS_RED) || (hum_sev == STATUS_RED) ||
                         (dp_sev == STATUS_RED) || (ah_sev == STATUS_RED);
    if (has_red) {
        add_by_severity(STATUS_RED);
    } else {
        add_by_severity(STATUS_ORANGE);
        add_by_severity(STATUS_YELLOW);
    }

    return result;
}

} // namespace StatusMessages
