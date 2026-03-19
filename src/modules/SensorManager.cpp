// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/SensorManager.h"

#include <math.h>
#include <stdio.h>
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "modules/PressureHistory.h"
#include "modules/StorageManager.h"
// [NEW] Only included in the .cpp — not in SensorManager.h — to avoid a
// circular header dependency. Guarded with ARDUINO so the native host test
// build (which has no WiFi.h) compiles without errors.
#ifdef ARDUINO
#include "modules/MqttManager.h"
#endif

namespace {

float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

int clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

bool sync_co_fields(SensorData &data, const Sen0466 &co_sensor) {
    bool co_present = co_sensor.isPresent();
    bool co_warmup = co_sensor.isWarmupActive();
    bool co_valid = co_sensor.isDataValid();
    float co_ppm = co_sensor.coPpm();

    if (!co_present) {
        co_warmup = false;
        co_valid = false;
        co_ppm = 0.0f;
    } else if (!co_valid || !isfinite(co_ppm) || co_ppm < Config::SEN0466_CO_MIN_PPM) {
        co_valid = false;
        co_ppm = 0.0f;
    } else if (co_ppm > Config::SEN0466_CO_MAX_PPM) {
        co_ppm = Config::SEN0466_CO_MAX_PPM;
    }

    bool changed = false;
    if (data.co_sensor_present != co_present) {
        data.co_sensor_present = co_present;
        changed = true;
    }
    if (data.co_warmup != co_warmup) {
        data.co_warmup = co_warmup;
        changed = true;
    }
    if (data.co_valid != co_valid) {
        data.co_valid = co_valid;
        changed = true;
    }
    if (!isfinite(data.co_ppm) || fabsf(data.co_ppm - co_ppm) > 0.01f) {
        data.co_ppm = co_ppm;
        changed = true;
    }

    return changed;
}

bool apply_sanity_filters(SensorData &data) {
    bool changed = false;

    if (data.temp_valid &&
        (!isfinite(data.temperature) ||
         data.temperature < Config::SEN66_TEMP_MIN_C ||
         data.temperature > Config::SEN66_TEMP_MAX_C)) {
        data.temp_valid = false;
        data.temperature = 0.0f;
        changed = true;
    }

    if (data.hum_valid &&
        (!isfinite(data.humidity) ||
         data.humidity < Config::SEN66_HUM_MIN ||
         data.humidity > Config::SEN66_HUM_MAX)) {
        data.hum_valid = false;
        data.humidity = 0.0f;
        changed = true;
    }

    if (data.co2_valid) {
        int clamped = clampi(data.co2, Config::SEN66_CO2_MIN_PPM, Config::SEN66_CO2_MAX_PPM);
        if (clamped != data.co2) {
            data.co2 = clamped;
            changed = true;
        }
    }

    if (data.voc_valid &&
        (data.voc_index < Config::SEN66_VOC_MIN ||
         data.voc_index > Config::SEN66_VOC_MAX)) {
        data.voc_valid = false;
        data.voc_index = 0;
        changed = true;
    }

    if (data.nox_valid &&
        (data.nox_index < Config::SEN66_NOX_MIN ||
         data.nox_index > Config::SEN66_NOX_MAX)) {
        data.nox_valid = false;
        data.nox_index = 0;
        changed = true;
    }

    if (data.pm25_valid) {
        if (!isfinite(data.pm25)) {
            data.pm25_valid = false;
            data.pm25 = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.pm25, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm25) {
                data.pm25 = clamped;
                changed = true;
            }
        }
    }

    if (data.pm4_valid) {
        if (!isfinite(data.pm4)) {
            data.pm4_valid = false;
            data.pm4 = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.pm4, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm4) {
                data.pm4 = clamped;
                changed = true;
            }
        }
    }

    if (data.pm10_valid) {
        if (!isfinite(data.pm10)) {
            data.pm10_valid = false;
            data.pm10 = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.pm10, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm10) {
                data.pm10 = clamped;
                changed = true;
            }
        }
    }

    if (data.pm05_valid) {
        if (!isfinite(data.pm05)) {
            data.pm05_valid = false;
            data.pm05 = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.pm05, Config::SEN66_PM_NUM_MIN_PPCM3, Config::SEN66_PM_NUM_MAX_PPCM3);
            if (clamped != data.pm05) {
                data.pm05 = clamped;
                changed = true;
            }
        }
    }

    if (data.pm1_valid) {
        if (!isfinite(data.pm1)) {
            data.pm1_valid = false;
            data.pm1 = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.pm1, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm1) {
                data.pm1 = clamped;
                changed = true;
            }
        }
    } else {
        if (data.pm1 != 0.0f) {
            data.pm1 = 0.0f;
            changed = true;
        }
    }
    data.pm_valid = data.pm1_valid || data.pm25_valid || data.pm4_valid || data.pm10_valid;

    if (data.hcho_valid) {
        if (!isfinite(data.hcho)) {
            data.hcho_valid = false;
            data.hcho = 0.0f;
            changed = true;
        } else {
            float clamped = clampf(data.hcho, Config::SFA3X_HCHO_MIN_PPB, Config::SFA3X_HCHO_MAX_PPB);
            if (clamped != data.hcho) {
                data.hcho = clamped;
                changed = true;
            }
        }
    }

    return changed;
}

bool invalidate_sen66_fields(SensorData &data) {
    bool changed = false;

    auto clear_float = [&](bool &valid, float &value) {
        if (valid || value != 0.0f) {
            valid = false;
            value = 0.0f;
            changed = true;
        }
    };

    auto clear_int = [&](bool &valid, int &value) {
        if (valid || value != 0) {
            valid = false;
            value = 0;
            changed = true;
        }
    };

    clear_float(data.temp_valid, data.temperature);
    clear_float(data.hum_valid, data.humidity);
    clear_float(data.pm05_valid, data.pm05);
    clear_float(data.pm1_valid, data.pm1);
    clear_float(data.pm25_valid, data.pm25);
    clear_float(data.pm4_valid, data.pm4);
    clear_float(data.pm10_valid, data.pm10);
    clear_int(data.co2_valid, data.co2);
    clear_int(data.voc_valid, data.voc_index);
    clear_int(data.nox_valid, data.nox_index);

    if (data.pm_valid) {
        data.pm_valid = false;
        changed = true;
    }

    return changed;
}

enum class AlertBand : uint8_t {
    Unknown = 0xFF,
    Good = 0,
    Moderate = 1,
    Bad = 2,
    Critical = 3
};

AlertBand classify_upper_band(float value,
                              float good_max,
                              float moderate_max,
                              float bad_max,
                              bool good_inclusive) {
    if (good_inclusive) {
        if (value <= good_max) return AlertBand::Good;
    } else {
        if (value < good_max) return AlertBand::Good;
    }
    if (value <= moderate_max) return AlertBand::Moderate;
    if (value <= bad_max) return AlertBand::Bad;
    return AlertBand::Critical;
}

// [NEW] alert_band_name() strings form the second token of the MQTT
// event_type (e.g. "co2" + "bad" -> "co2_bad"). Must stay in sync with the
// event_types list in MqttManager::publishDiscoveryEvent().
const char *alert_band_name(AlertBand band) {
    switch (band) {
        case AlertBand::Good:     return "good";
        case AlertBand::Moderate: return "moderate";
        case AlertBand::Bad:      return "bad";
        case AlertBand::Critical: return "critical";
        case AlertBand::Unknown:
        default:                  return "unknown";
    }
}

const char *alert_band_phrase(AlertBand band) {
    switch (band) {
        case AlertBand::Moderate: return "elevated";
        case AlertBand::Bad:      return "high";
        case AlertBand::Critical: return "critical";
        case AlertBand::Good:     return "normal";
        case AlertBand::Unknown:
        default:                  return "unknown";
    }
}

// [NEW] Added mqtt_key parameter for building the MQTT event_type string.
// Added human-readable message building so the HA event payload contains the
// same text that the web dashboard Events tab and serial log already show,
// e.g. "CO2 worsened to bad: 1480 ppm" — not just the raw event_type key.
void log_air_metric_transition(const char *name,
                               const char *mqtt_key,
                               const char *unit,
                               float value,
                               bool valid,
                               float good_max,
                               float moderate_max,
                               float bad_max,
                               bool good_inclusive,
                               const char *value_fmt,
                               AlertBand &previous_band) {
    if (!valid || !isfinite(value)) {
        previous_band = AlertBand::Unknown;
        return;
    }

    const AlertBand current_band =
        classify_upper_band(value, good_max, moderate_max, bad_max, good_inclusive);
    if (current_band == previous_band) {
        return;
    }

    char value_buf[24];
    snprintf(value_buf, sizeof(value_buf), value_fmt, value);

    // [NEW] Build the human-readable message that will appear in HA exactly as
    // it already appears in the serial log and web dashboard Events tab.
    char message[96];

    if (previous_band == AlertBand::Unknown) {
        if (current_band != AlertBand::Good) {
            snprintf(message, sizeof(message), "%s %s: %s %s",
                     name, alert_band_phrase(current_band), value_buf, unit);
            LOGW("Sensors", "%s", message);
#ifdef ARDUINO
            MqttPublishEvent(mqtt_key, alert_band_name(current_band), value, unit, message);
#endif
        }
        previous_band = current_band;
        return;
    }

    if (current_band == AlertBand::Good) {
        snprintf(message, sizeof(message), "%s back to normal: %s %s", name, value_buf, unit);
        LOGI("Sensors", "%s", message);
    } else if (static_cast<uint8_t>(current_band) > static_cast<uint8_t>(previous_band)) {
        snprintf(message, sizeof(message), "%s worsened to %s: %s %s",
                 name, alert_band_name(current_band), value_buf, unit);
        LOGW("Sensors", "%s", message);
    } else {
        snprintf(message, sizeof(message), "%s improved to %s: %s %s",
                 name, alert_band_name(current_band), value_buf, unit);
        LOGI("Sensors", "%s", message);
    }

    // [NEW] Publish the band transition with the full message string so HA
    // displays something meaningful instead of just the event_type key.
#ifdef ARDUINO
    MqttPublishEvent(mqtt_key, alert_band_name(current_band), value, unit, message);
#endif

    previous_band = current_band;
}

void log_soft_warnings(const SensorData &data, bool gas_warmup) {
    static bool temp_outside = false;
    static bool hum_outside = false;
    static AlertBand co2_band  = AlertBand::Unknown;
    static AlertBand co_band   = AlertBand::Unknown;
    static AlertBand voc_band  = AlertBand::Unknown;
    static AlertBand nox_band  = AlertBand::Unknown;
    static AlertBand hcho_band = AlertBand::Unknown;
    static AlertBand pm05_band = AlertBand::Unknown;
    static AlertBand pm1_band  = AlertBand::Unknown;
    static AlertBand pm25_band = AlertBand::Unknown;
    static AlertBand pm4_band  = AlertBand::Unknown;
    static AlertBand pm10_band = AlertBand::Unknown;

    if (data.temp_valid) {
        bool temp_now_outside =
            (data.temperature < Config::SEN66_TEMP_RECOMM_MIN_C ||
             data.temperature > Config::SEN66_TEMP_RECOMM_MAX_C);
        if (temp_now_outside && !temp_outside) {
            LOGW("Sensors", "Temperature outside recommended range: %.1f C", data.temperature);
        } else if (!temp_now_outside && temp_outside) {
            LOGI("Sensors", "Temperature back within recommended range: %.1f C", data.temperature);
        }
        temp_outside = temp_now_outside;
    } else {
        temp_outside = false;
    }

    if (data.hum_valid) {
        bool hum_now_outside =
            (data.humidity < Config::SEN66_HUM_RECOMM_MIN ||
             data.humidity > Config::SEN66_HUM_RECOMM_MAX);
        if (hum_now_outside && !hum_outside) {
            LOGW("Sensors", "Humidity outside recommended range: %.0f%%", data.humidity);
        } else if (!hum_now_outside && hum_outside) {
            LOGI("Sensors", "Humidity back within recommended range: %.0f%%", data.humidity);
        }
        hum_outside = hum_now_outside;
    } else {
        hum_outside = false;
    }

    // [NEW] Each call now passes a mqtt_key as the second argument.
    log_air_metric_transition("CO2", "co2", "ppm",
                              static_cast<float>(data.co2),
                              data.co2_valid && data.co2 > 0,
                              Config::AQ_CO2_GREEN_MAX_PPM,
                              Config::AQ_CO2_YELLOW_MAX_PPM,
                              Config::AQ_CO2_ORANGE_MAX_PPM,
                              false, "%.0f", co2_band);

    log_air_metric_transition("CO", "co", "ppm",
                              data.co_ppm,
                              data.co_sensor_present && data.co_valid,
                              Config::AQ_CO_GREEN_MAX_PPM,
                              Config::AQ_CO_YELLOW_MAX_PPM,
                              Config::AQ_CO_ORANGE_MAX_PPM,
                              false, "%.1f", co_band);

    log_air_metric_transition("PM0.5", "pm05", "#/cm3",
                              data.pm05, data.pm05_valid,
                              Config::AQ_PM05_GREEN_MAX_PPCM3,
                              Config::AQ_PM05_YELLOW_MAX_PPCM3,
                              Config::AQ_PM05_ORANGE_MAX_PPCM3,
                              true, "%.0f", pm05_band);

    log_air_metric_transition("PM1.0", "pm1", "ug/m3",
                              data.pm1, data.pm1_valid,
                              Config::AQ_PM1_GREEN_MAX_UGM3,
                              Config::AQ_PM1_YELLOW_MAX_UGM3,
                              Config::AQ_PM1_ORANGE_MAX_UGM3,
                              true, "%.1f", pm1_band);

    log_air_metric_transition("PM2.5", "pm25", "ug/m3",
                              data.pm25, data.pm25_valid,
                              Config::AQ_PM25_GREEN_MAX_UGM3,
                              Config::AQ_PM25_YELLOW_MAX_UGM3,
                              Config::AQ_PM25_ORANGE_MAX_UGM3,
                              true, "%.1f", pm25_band);

    log_air_metric_transition("PM4.0", "pm4", "ug/m3",
                              data.pm4, data.pm4_valid,
                              Config::AQ_PM4_GREEN_MAX_UGM3,
                              Config::AQ_PM4_YELLOW_MAX_UGM3,
                              Config::AQ_PM4_ORANGE_MAX_UGM3,
                              true, "%.1f", pm4_band);

    log_air_metric_transition("PM10", "pm10", "ug/m3",
                              data.pm10, data.pm10_valid,
                              Config::AQ_PM10_GREEN_MAX_UGM3,
                              Config::AQ_PM10_YELLOW_MAX_UGM3,
                              Config::AQ_PM10_ORANGE_MAX_UGM3,
                              true, "%.1f", pm10_band);

    log_air_metric_transition("HCHO", "hcho", "ppb",
                              data.hcho, data.hcho_valid,
                              30.0f, 60.0f, 100.0f,
                              false, "%.1f", hcho_band);

    log_air_metric_transition("VOC", "voc", "idx",
                              static_cast<float>(data.voc_index),
                              !gas_warmup && data.voc_valid,
                              static_cast<float>(Config::AQ_VOC_GREEN_MAX_INDEX),
                              static_cast<float>(Config::AQ_VOC_YELLOW_MAX_INDEX),
                              static_cast<float>(Config::AQ_VOC_ORANGE_MAX_INDEX),
                              true, "%.0f", voc_band);

    log_air_metric_transition("NOx", "nox", "idx",
                              static_cast<float>(data.nox_index),
                              !gas_warmup && data.nox_valid,
                              static_cast<float>(Config::AQ_NOX_GREEN_MAX_INDEX),
                              static_cast<float>(Config::AQ_NOX_YELLOW_MAX_INDEX),
                              static_cast<float>(Config::AQ_NOX_ORANGE_MAX_INDEX),
                              true, "%.0f", nox_band);
}

} // namespace

void SensorManager::begin(StorageManager &storage, float temp_offset, float hum_offset) {
    sen66_.begin();
    sen66_.setOffsets(temp_offset, hum_offset);
    sen66_.loadVocState(storage);
    sen66_start_attempts_ = 0;
    sen66_retry_exhausted_logged_ = false;

    bmp580_.begin();
    if (bmp580_.start()) {
        pressure_sensor_ = PRESSURE_BMP58X;
        Logger::log(Logger::Info, "Sensors", "%s OK", bmp580_.variantLabel());
    } else {
        bmp3xx_.begin();
        if (bmp3xx_.start()) {
            pressure_sensor_ = PRESSURE_BMP3XX;
            Logger::log(Logger::Info, "Sensors", "%s OK", bmp3xx_.variantLabel());
        } else {
            dps310_.begin();
            if (dps310_.start()) {
                pressure_sensor_ = PRESSURE_DPS310;
                LOGI("Sensors", "DPS310 OK");
            } else {
                pressure_sensor_ = PRESSURE_NONE;
                LOGW("Sensors", "Pressure sensor not found");
                // [NEW] Mirror this system warning to HA.
#ifdef ARDUINO
                MqttPublishSystemEvent("SENSORS", "warning", "Pressure sensor not found");
#endif
            }
        }
    }

    sfa3x_.begin();
    sfa3x_.start();
    switch (sfa3x_.status()) {
        case Sfa3x::Status::Ok:
            LOGI("Sensors", "SFA30 OK");
            // [NEW] SFA30 found and running — info event.
#ifdef ARDUINO
            MqttPublishSystemEvent("SENSORS", "info", "SFA30 OK");
#endif
            break;
        case Sfa3x::Status::Absent:
            LOGI("Sensors", "SFA30 not installed");
            break;
        case Sfa3x::Status::Fault:
            LOGW("Sensors", "SFA30 init failed");
            // [NEW] SFA30 hardware fault — warning event.
#ifdef ARDUINO
            MqttPublishSystemEvent("SENSORS", "warning", "SFA30 init failed");
#endif
            break;
    }

    sen0466_.begin();
    if (sen0466_.start()) {
        Logger::log(Logger::Info, "Sensors", "SEN0466 CO OK at 0x%02X",
                    static_cast<unsigned>(Config::SEN0466_ADDR));
    } else {
        LOGI("Sensors", "SEN0466 CO not installed");
    }

    sen66_.scheduleRetry(Config::SEN66_STARTUP_GRACE_MS);
    Logger::log(Logger::Info, "Sensors",
                "SEN66 startup delay %u ms",
                static_cast<unsigned>(Config::SEN66_STARTUP_GRACE_MS));
}

SensorManager::PollResult SensorManager::poll(SensorData &data,
                                              StorageManager &storage,
                                              PressureHistory &pressure_history,
                                              bool co2_asc_enabled) {
    PollResult result;
    bool sen66_changed = false;
    sen66_.poll(data, sen66_changed);
    if (sen66_changed) {
        result.data_changed = true;
    }
    sen66_.saveVocState(storage);

    sfa3x_.poll();
    float hcho_ppb = 0.0f;
    if (sfa3x_.takeNewData(hcho_ppb)) {
        data.hcho = hcho_ppb;
        data.hcho_valid = true;
        result.data_changed = true;
    }

    sen0466_.poll();

    float pressure_hpa = 0.0f;
    float temperature_c = 0.0f;
    bool pressure_valid = false;
    bool pressure_new = false;
    float pressure_min_hpa = Config::DPS310_PRESSURE_MIN_HPA;
    float pressure_max_hpa = Config::DPS310_PRESSURE_MAX_HPA;
    if (pressure_sensor_ == PRESSURE_BMP58X) {
        bmp580_.poll();
        if (bmp580_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = bmp580_.isPressureValid();
    } else if (pressure_sensor_ == PRESSURE_BMP3XX) {
        bmp3xx_.poll();
        if (bmp3xx_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = bmp3xx_.isPressureValid();
        pressure_min_hpa = Config::BMP3XX_PRESSURE_MIN_HPA;
        pressure_max_hpa = Config::BMP3XX_PRESSURE_MAX_HPA;
    } else if (pressure_sensor_ == PRESSURE_DPS310) {
        dps310_.poll();
        if (dps310_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = dps310_.isPressureValid();
    }
    if (pressure_new) {
        if (!isfinite(pressure_hpa) ||
            pressure_hpa < pressure_min_hpa ||
            pressure_hpa > pressure_max_hpa) {
            data.pressure = 0.0f;
            data.pressure_valid = false;
            data.pressure_delta_3h_valid = false;
            data.pressure_delta_24h_valid = false;
        } else {
            data.pressure = pressure_hpa;
            data.pressure_valid = true;
            pressure_history.update(pressure_hpa, data, storage);
            sen66_.updatePressure(pressure_hpa);
        }
        result.data_changed = true;
    }

    if (data.pressure_valid && pressure_sensor_ != PRESSURE_NONE && !pressure_valid) {
        data.pressure_valid = false;
        data.pressure_delta_3h_valid = false;
        data.pressure_delta_24h_valid = false;
        result.data_changed = true;
    }

    uint32_t now = millis();
    if (!sen66_.isOk() &&
        !sen66_.isBusy() &&
        sen66_start_attempts_ < Config::SEN66_MAX_START_ATTEMPTS &&
        now >= sen66_.retryAtMs()) {
        if (sen66_.start(co2_asc_enabled)) {
            LOGI("Sensors", "SEN66 OK");
            sen66_start_attempts_ = 0;
            sen66_retry_exhausted_logged_ = false;
            // [NEW] SEN66 recovered — send info event so HA knows the sensor is back.
#ifdef ARDUINO
            MqttPublishSystemEvent("SENSORS", "info", "SEN66 OK");
#endif
        } else {
            if (sen66_start_attempts_ < UINT8_MAX) {
                ++sen66_start_attempts_;
            }
            // [NEW] Build the same "SEN66 not found (x/y)" message the web
            // dashboard Events tab shows and forward it to HA.
            char msg[48];
            snprintf(msg, sizeof(msg), "SEN66 not found (%u/%u)",
                     static_cast<unsigned>(sen66_start_attempts_),
                     static_cast<unsigned>(Config::SEN66_MAX_START_ATTEMPTS));
            LOGW("Sensors", "%s", msg);
#ifdef ARDUINO
            MqttPublishSystemEvent("SENSORS", "warning", msg);
#endif
            if (sen66_start_attempts_ < Config::SEN66_MAX_START_ATTEMPTS) {
                sen66_.scheduleRetry(Config::SEN66_START_RETRY_MS);
            } else if (!sen66_retry_exhausted_logged_) {
                // [NEW] Exhausted all start attempts — send a final warning event.
                LOGW("Sensors", "SEN66 start attempts exhausted, stop probing until reboot");
#ifdef ARDUINO
                MqttPublishSystemEvent("SENSORS", "warning",
                                       "SEN66 start attempts exhausted, stop probing until reboot");
#endif
                sen66_retry_exhausted_logged_ = true;
            }
        }
    }

    bool warmup_now = sen66_.isWarmupActive();
    if (warmup_now != warmup_active_last_) {
        warmup_active_last_ = warmup_now;
        result.warmup_changed = true;
    }

    if (sync_co_fields(data, sen0466_)) {
        result.data_changed = true;
    }

    uint32_t sen66_last_ms = sen66_.lastDataMs();
    if (sen66_last_ms != 0 && (now - sen66_last_ms > Config::SEN66_STALE_MS)) {
        if (invalidate_sen66_fields(data)) {
            result.data_changed = true;
        }
    }
    uint32_t sfa_last_ms = sfa3x_.lastDataMs();
    if (data.hcho_valid && sfa_last_ms != 0 &&
        (now - sfa_last_ms > Config::SFA3X_STALE_MS)) {
        data.hcho_valid = false;
        sfa3x_.invalidate();
        result.data_changed = true;
    }

    if (apply_sanity_filters(data)) {
        result.data_changed = true;
    }
    log_soft_warnings(data, warmup_now);

    return result;
}

bool SensorManager::isPressureOk() const {
    if (pressure_sensor_ == PRESSURE_BMP58X) {
        return bmp580_.isOk();
    }
    if (pressure_sensor_ == PRESSURE_BMP3XX) {
        return bmp3xx_.isOk();
    }
    if (pressure_sensor_ == PRESSURE_DPS310) {
        return dps310_.isOk();
    }
    return false;
}

const char *SensorManager::pressureSensorLabel() const {
    switch (pressure_sensor_) {
        case PRESSURE_BMP58X:
            switch (bmp580_.variant()) {
                case Bmp580::Variant::BMP585:    return "BMP585:";
                case Bmp580::Variant::BMP580_581: return "BMP580/581:";
                default:                          return "BMP58x:";
            }
        case PRESSURE_BMP3XX:
            switch (bmp3xx_.variant()) {
                case Bmp3xx::Variant::BMP388: return "BMP388:";
                case Bmp3xx::Variant::BMP390: return "BMP390:";
                default:                      return "BMP3xx:";
            }
        case PRESSURE_DPS310:
            return "DPS310:";
        default:
            return "PRESS:";
    }
}

void SensorManager::setOffsets(float temp_offset, float hum_offset) {
    sen66_.setOffsets(temp_offset, hum_offset);
}

void SensorManager::clearVocState(StorageManager &storage) {
    sen66_.clearVocState(storage);
}