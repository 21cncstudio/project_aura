// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/SensorManager.h"

#include <math.h>
#include <stdio.h>
#include "core/BootState.h"
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "modules/PressureHistory.h"
#include "modules/StorageManager.h"

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

uint8_t normalize_ppm_decimals(uint8_t decimals) {
    return decimals <= 2 ? decimals : 1;
}

bool sync_ppm_sensor_fields(bool sensor_present,
                            bool sensor_warmup,
                            bool sensor_valid,
                            float sensor_ppm,
                            float min_ppm,
                            float max_ppm,
                            bool &present_field,
                            bool &warmup_field,
                            bool &valid_field,
                            float &ppm_field) {
    if (!sensor_present) {
        sensor_warmup = false;
        sensor_valid = false;
        sensor_ppm = 0.0f;
    } else if (!sensor_valid || !isfinite(sensor_ppm) || sensor_ppm < min_ppm) {
        sensor_valid = false;
        sensor_ppm = 0.0f;
    } else if (sensor_ppm > max_ppm) {
        sensor_ppm = max_ppm;
    }

    bool changed = false;
    if (present_field != sensor_present) {
        present_field = sensor_present;
        changed = true;
    }
    if (warmup_field != sensor_warmup) {
        warmup_field = sensor_warmup;
        changed = true;
    }
    if (valid_field != sensor_valid) {
        valid_field = sensor_valid;
        changed = true;
    }
    if (!isfinite(ppm_field) || fabsf(ppm_field - sensor_ppm) > 0.01f) {
        ppm_field = sensor_ppm;
        changed = true;
    }

    return changed;
}

bool sync_co_fields(SensorData &data, const Sen0466 &co_sensor) {
    return sync_ppm_sensor_fields(co_sensor.isPresent(),
                                  co_sensor.isWarmupActive(),
                                  co_sensor.isDataValid(),
                                  co_sensor.coPpm(),
                                  Config::SEN0466_CO_MIN_PPM,
                                  Config::SEN0466_CO_MAX_PPM,
                                  data.co_sensor_present,
                                  data.co_warmup,
                                  data.co_valid,
                                  data.co_ppm);
}

bool sync_optional_gas_fields(SensorData &data, const DfrOptionalGasSensor &optional_gas) {
    const DfrOptionalGasSensor::OptionalGasType gas_type = optional_gas.optionalGasType();
    const bool sensor_present = optional_gas.isPresent() &&
                                gas_type != DfrOptionalGasSensor::OptionalGasType::None;
    const bool sensor_warmup = sensor_present && optional_gas.isWarmupActive();
    const bool sensor_valid = sensor_present && optional_gas.isDataValid();
    const float sensor_ppm = sensor_present ? optional_gas.ppm() : 0.0f;
    const uint8_t sensor_decimals = sensor_present
        ? normalize_ppm_decimals(optional_gas.ppmDecimals())
        : 1;
    const float min_ppm = DfrOptionalGasSensor::minPpmForType(gas_type);
    const float max_ppm = DfrOptionalGasSensor::maxPpmForType(gas_type);

    bool changed = sync_ppm_sensor_fields(sensor_present,
                                          sensor_warmup,
                                          sensor_valid,
                                          sensor_ppm,
                                          min_ppm,
                                          max_ppm,
                                          data.optional_gas_sensor_present,
                                          data.optional_gas_warmup,
                                          data.optional_gas_valid,
                                          data.optional_gas_ppm);

    const uint8_t gas_type_raw = static_cast<uint8_t>(gas_type);
    if (data.optional_gas_type != gas_type_raw) {
        data.optional_gas_type = gas_type_raw;
        changed = true;
    }
    if (data.optional_gas_ppm_decimals != sensor_decimals) {
        data.optional_gas_ppm_decimals = sensor_decimals;
        changed = true;
    }

    const bool nh3_present = sensor_present &&
                             gas_type == DfrOptionalGasSensor::OptionalGasType::NH3;
    changed |= sync_ppm_sensor_fields(nh3_present,
                                      nh3_present && sensor_warmup,
                                      nh3_present && sensor_valid,
                                      nh3_present ? sensor_ppm : 0.0f,
                                      Config::SEN0469_NH3_MIN_PPM,
                                      Config::SEN0469_NH3_MAX_PPM,
                                      data.nh3_sensor_present,
                                      data.nh3_warmup,
                                      data.nh3_valid,
                                      data.nh3_ppm);

    return changed;
}

bool apply_sanity_filters(SensorData &data, float hcho_min_ppb, float hcho_max_ppb) {
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
            float clamped = clampf(data.hcho, hcho_min_ppb, hcho_max_ppb);
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

const char *alert_band_name(AlertBand band) {
    switch (band) {
        case AlertBand::Good:
            return "good";
        case AlertBand::Moderate:
            return "moderate";
        case AlertBand::Bad:
            return "bad";
        case AlertBand::Critical:
            return "critical";
        case AlertBand::Unknown:
        default:
            return "unknown";
    }
}

const char *alert_band_phrase(AlertBand band) {
    switch (band) {
        case AlertBand::Moderate:
            return "elevated";
        case AlertBand::Bad:
            return "high";
        case AlertBand::Critical:
            return "critical";
        case AlertBand::Good:
            return "normal";
        case AlertBand::Unknown:
        default:
            return "unknown";
    }
}

void log_air_metric_transition(const char *name,
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

    if (previous_band == AlertBand::Unknown) {
        if (current_band != AlertBand::Good) {
            LOGW("Sensors", "%s %s: %s %s",
                 name,
                 alert_band_phrase(current_band),
                 value_buf,
                 unit);
        }
        previous_band = current_band;
        return;
    }

    if (current_band == AlertBand::Good) {
        if (previous_band != AlertBand::Good) {
            LOGI("Sensors", "%s back to normal: %s %s", name, value_buf, unit);
        }
    } else if (static_cast<uint8_t>(current_band) > static_cast<uint8_t>(previous_band)) {
        LOGW("Sensors", "%s worsened to %s: %s %s",
             name,
             alert_band_name(current_band),
             value_buf,
             unit);
    } else {
        LOGI("Sensors", "%s improved to %s: %s %s",
             name,
             alert_band_name(current_band),
             value_buf,
             unit);
    }

    previous_band = current_band;
}

void log_soft_warnings(const SensorData &data, bool gas_warmup) {
    static bool temp_outside = false;
    static bool hum_outside = false;
    static AlertBand co2_band = AlertBand::Unknown;
    static AlertBand co_band = AlertBand::Unknown;
    static AlertBand voc_band = AlertBand::Unknown;
    static AlertBand nox_band = AlertBand::Unknown;
    static AlertBand hcho_band = AlertBand::Unknown;
    static AlertBand pm05_band = AlertBand::Unknown;
    static AlertBand pm1_band = AlertBand::Unknown;
    static AlertBand pm25_band = AlertBand::Unknown;
    static AlertBand pm4_band = AlertBand::Unknown;
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

    log_air_metric_transition("CO2",
                              "ppm",
                              static_cast<float>(data.co2),
                              data.co2_valid && data.co2 > 0,
                              Config::AQ_CO2_GREEN_MAX_PPM,
                              Config::AQ_CO2_YELLOW_MAX_PPM,
                              Config::AQ_CO2_ORANGE_MAX_PPM,
                              false,
                              "%.0f",
                              co2_band);

    log_air_metric_transition("CO",
                              "ppm",
                              data.co_ppm,
                              data.co_sensor_present && data.co_valid,
                              Config::AQ_CO_GREEN_MAX_PPM,
                              Config::AQ_CO_YELLOW_MAX_PPM,
                              Config::AQ_CO_ORANGE_MAX_PPM,
                              false,
                              "%.1f",
                              co_band);

    log_air_metric_transition("PM0.5",
                              "#/cm3",
                              data.pm05,
                              data.pm05_valid,
                              Config::AQ_PM05_GREEN_MAX_PPCM3,
                              Config::AQ_PM05_YELLOW_MAX_PPCM3,
                              Config::AQ_PM05_ORANGE_MAX_PPCM3,
                              true,
                              "%.0f",
                              pm05_band);

    log_air_metric_transition("PM1.0",
                              "ug/m3",
                              data.pm1,
                              data.pm1_valid,
                              Config::AQ_PM1_GREEN_MAX_UGM3,
                              Config::AQ_PM1_YELLOW_MAX_UGM3,
                              Config::AQ_PM1_ORANGE_MAX_UGM3,
                              true,
                              "%.1f",
                              pm1_band);

    log_air_metric_transition("PM2.5",
                              "ug/m3",
                              data.pm25,
                              data.pm25_valid,
                              Config::AQ_PM25_GREEN_MAX_UGM3,
                              Config::AQ_PM25_YELLOW_MAX_UGM3,
                              Config::AQ_PM25_ORANGE_MAX_UGM3,
                              true,
                              "%.1f",
                              pm25_band);

    log_air_metric_transition("PM4.0",
                              "ug/m3",
                              data.pm4,
                              data.pm4_valid,
                              Config::AQ_PM4_GREEN_MAX_UGM3,
                              Config::AQ_PM4_YELLOW_MAX_UGM3,
                              Config::AQ_PM4_ORANGE_MAX_UGM3,
                              true,
                              "%.1f",
                              pm4_band);

    log_air_metric_transition("PM10",
                              "ug/m3",
                              data.pm10,
                              data.pm10_valid,
                              Config::AQ_PM10_GREEN_MAX_UGM3,
                              Config::AQ_PM10_YELLOW_MAX_UGM3,
                              Config::AQ_PM10_ORANGE_MAX_UGM3,
                              true,
                              "%.1f",
                              pm10_band);

    log_air_metric_transition("HCHO",
                              "ppb",
                              data.hcho,
                              data.hcho_valid,
                              30.0f,
                              60.0f,
                              100.0f,
                              false,
                              "%.1f",
                              hcho_band);

    log_air_metric_transition("VOC",
                              "idx",
                              static_cast<float>(data.voc_index),
                              !gas_warmup && data.voc_valid,
                              static_cast<float>(Config::AQ_VOC_GREEN_MAX_INDEX),
                              static_cast<float>(Config::AQ_VOC_YELLOW_MAX_INDEX),
                              static_cast<float>(Config::AQ_VOC_ORANGE_MAX_INDEX),
                              true,
                              "%.0f",
                              voc_band);

    log_air_metric_transition("NOx",
                              "idx",
                              static_cast<float>(data.nox_index),
                              !gas_warmup && data.nox_valid,
                              static_cast<float>(Config::AQ_NOX_GREEN_MAX_INDEX),
                              static_cast<float>(Config::AQ_NOX_YELLOW_MAX_INDEX),
                              static_cast<float>(Config::AQ_NOX_ORANGE_MAX_INDEX),
                              true,
                              "%.0f",
                              nox_band);
}

} // namespace

SensorManager::SharedI2cLease::SharedI2cLease(SensorManager &owner)
    : owner_(owner), acquired_(owner_.tryAcquireSharedI2c()) {}

SensorManager::SharedI2cLease::~SharedI2cLease() {
    if (acquired_) {
        owner_.releaseSharedI2c();
    }
}

bool SensorManager::tryAcquireSharedI2c() {
    if (!shared_i2c_available_.load(std::memory_order_acquire)) {
        return false;
    }

    shared_i2c_active_users_.fetch_add(1U, std::memory_order_acq_rel);
    if (!shared_i2c_available_.load(std::memory_order_acquire)) {
        shared_i2c_active_users_.fetch_sub(1U, std::memory_order_acq_rel);
        return false;
    }
    return true;
}

void SensorManager::releaseSharedI2c() {
    shared_i2c_active_users_.fetch_sub(1U, std::memory_order_release);
}

void SensorManager::disableSharedI2c() {
    shared_i2c_available_.store(false, std::memory_order_release);
}

bool SensorManager::waitForSharedI2cIdle(uint32_t timeout_ms) {
    const uint32_t wait_started_ms = millis();
    while (shared_i2c_active_users_.load(std::memory_order_acquire) != 0U) {
        if (static_cast<uint32_t>(millis() - wait_started_ms) >= timeout_ms) {
            return false;
        }
        delay(1);
    }
    return true;
}

void SensorManager::begin(StorageManager &storage, float temp_offset, float hum_offset) {
    // begin() is the only operation allowed to reopen the one-way runtime gate.
    // A fresh startup cannot have callers left from the previous runtime.
    shared_i2c_active_users_.store(0U, std::memory_order_release);
    shared_i2c_available_.store(true, std::memory_order_release);
    initialized_ = true;
    late_probe_kind_ = LateProbeKind::None;
    late_driver_started_ = false;
    late_probe_cursor_ = 0;
    sen66_.begin();
    sen66_.setOffsets(temp_offset, hum_offset);
    sen66_.loadVocState(storage);

    bmp580_.begin();
    bmp3xx_.begin();
    dps310_.begin();
    pressure_probe_.reset(millis());
    pressure_probe_.recordAttempt(detectPressureSensor());

    hcho_sensor_type_ = HCHO_SENSOR_NONE;
    sfa30_.begin();
    sfa40_.begin();
    hcho_probe_.reset(millis());
    hcho_probe_.recordAttempt(detectHchoSensor());
    sfa_warmup_active_last_ = currentHchoWarmupActive();
    sfa_status_last_ = currentHchoStatus();

    sen0466_.begin();
    if (sen0466_.start()) {
        Logger::log(Logger::Info, "Sensors", "%s OK at 0x%02X",
                    sen0466_.label(),
                    static_cast<unsigned>(sen0466_.address()));
    } else {
        Logger::log(Logger::Info, "Sensors", "%s not installed", sen0466_.label());
    }

    optional_gas_.begin();
    if (optional_gas_.start()) {
        Logger::log(Logger::Info, "Sensors", "%s slot detected at 0x%02X, validating gas type",
                    optional_gas_.label(),
                    static_cast<unsigned>(optional_gas_.address()));
    } else {
        Logger::log(Logger::Info, "Sensors", "%s slot not installed", optional_gas_.label());
    }

    const uint32_t sen66_probe_start_ms = millis() + Config::SEN66_STARTUP_GRACE_MS;
    sen66_probe_.reset(sen66_probe_start_ms);
    Logger::log(Logger::Info, "Sensors",
                "SEN66 startup delay %u ms",
                static_cast<unsigned>(Config::SEN66_STARTUP_GRACE_MS));
}

SensorManager::PollResult SensorManager::poll(SensorData &data,
                                              StorageManager &storage,
                                              PressureHistory &pressure_history,
                                              bool co2_asc_enabled) {
    PollResult result;
    if (!initialized_) {
        return result;
    }
    SharedI2cLease shared_i2c(*this);
    if (!shared_i2c) {
        return result;
    }
    const uint32_t now = millis();

    const LateProbeKind late_probe_at_entry = late_probe_kind_;
    if (late_probe_kind_ != LateProbeKind::None) {
        pollActiveLateProbe(now, result);
    } else {
        startNextLateProbe(now, co2_asc_enabled);
    }
    const bool pressure_late_this_poll =
        late_probe_at_entry == LateProbeKind::Pressure ||
        late_probe_kind_ == LateProbeKind::Pressure;
    const bool hcho_late_this_poll =
        late_probe_at_entry == LateProbeKind::Hcho ||
        late_probe_kind_ == LateProbeKind::Hcho;
    const bool sen66_late_this_poll =
        late_probe_at_entry == LateProbeKind::Sen66 ||
        late_probe_kind_ == LateProbeKind::Sen66;

    bool sen66_changed = false;
    if (!sen66_late_this_poll) {
        sen66_.poll(data, sen66_changed);
    }
    if (sen66_changed) {
        result.data_changed = true;
    }
    if (!sen66_late_this_poll) {
        sen66_.saveVocState(storage);
    }

    if (!hcho_late_this_poll && hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        sfa40_.poll();
    } else if (!hcho_late_this_poll && hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        sfa30_.poll();
    }
    const bool sfa_warmup_now = currentHchoWarmupActive();
    const SfaStatus sfa_status_now = currentHchoStatus();
    const bool hcho_sensor_present_now = (sfa_status_now != SfaStatus::Absent);
    const bool hcho_warmup_now = hcho_sensor_present_now && sfa_warmup_now;
    if (sfa_warmup_now != sfa_warmup_active_last_ || sfa_status_now != sfa_status_last_) {
        sfa_warmup_active_last_ = sfa_warmup_now;
        sfa_status_last_ = sfa_status_now;
        result.data_changed = true;
    }
    if (data.hcho_sensor_present != hcho_sensor_present_now) {
        data.hcho_sensor_present = hcho_sensor_present_now;
        result.data_changed = true;
    }
    if (data.hcho_warmup != hcho_warmup_now) {
        data.hcho_warmup = hcho_warmup_now;
        result.data_changed = true;
    }
    if (sfa_status_now == SfaStatus::Fault && data.hcho_valid) {
        data.hcho_valid = false;
        currentHchoInvalidate();
        result.data_changed = true;
    }
    if (sfa_warmup_now && data.hcho_valid) {
        data.hcho_valid = false;
        result.data_changed = true;
    }
    float hcho_ppb = 0.0f;
    if (currentHchoTakeNewData(hcho_ppb)) {
        data.hcho = hcho_ppb;
        data.hcho_valid = !sfa_warmup_now;
        result.data_changed = true;
    }

    sen0466_.poll();
    optional_gas_.poll();

    float pressure_hpa = 0.0f;
    float temperature_c = 0.0f;
    bool pressure_valid = false;
    bool pressure_new = false;
    float pressure_min_hpa = Config::DPS310_PRESSURE_MIN_HPA;
    float pressure_max_hpa = Config::DPS310_PRESSURE_MAX_HPA;
    if (!pressure_late_this_poll && pressure_sensor_ == PRESSURE_BMP58X) {
        bmp580_.poll();
        if (bmp580_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = bmp580_.isPressureValid();
    } else if (!pressure_late_this_poll && pressure_sensor_ == PRESSURE_BMP3XX) {
        bmp3xx_.poll();
        if (bmp3xx_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = bmp3xx_.isPressureValid();
        pressure_min_hpa = Config::BMP3XX_PRESSURE_MIN_HPA;
        pressure_max_hpa = Config::BMP3XX_PRESSURE_MAX_HPA;
    } else if (!pressure_late_this_poll && pressure_sensor_ == PRESSURE_DPS310) {
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

    bool warmup_now = sen66_.isWarmupActive();
    if (warmup_now != warmup_active_last_) {
        warmup_active_last_ = warmup_now;
        result.warmup_changed = true;
    }

    if (sync_co_fields(data, sen0466_)) {
        result.data_changed = true;
    }
    if (sync_optional_gas_fields(data, optional_gas_)) {
        result.data_changed = true;
    }

    uint32_t sen66_last_ms = sen66_.lastDataMs();
    if (sen66_last_ms != 0 && (now - sen66_last_ms > Config::SEN66_STALE_MS)) {
        if (invalidate_sen66_fields(data)) {
            result.data_changed = true;
        }
    }
    uint32_t sfa_last_ms = currentHchoLastDataMs();
    if (data.hcho_valid && sfa_last_ms != 0 &&
        (now - sfa_last_ms > Config::SFA3X_STALE_MS)) {
        data.hcho_valid = false;
        currentHchoInvalidate();
        result.data_changed = true;
    }

    if (apply_sanity_filters(data, currentHchoMinPpb(), currentHchoMaxPpb())) {
        result.data_changed = true;
    }
    log_soft_warnings(data, warmup_now);

    return result;
}

bool SensorManager::detectPressureSensor() {
    pressure_sensor_ = PRESSURE_NONE;
    if (bmp580_.start()) {
        pressure_sensor_ = PRESSURE_BMP58X;
        Logger::log(Logger::Info, "Sensors", "%s OK", bmp580_.variantLabel());
        return true;
    }
    if (bmp3xx_.start()) {
        pressure_sensor_ = PRESSURE_BMP3XX;
        Logger::log(Logger::Info, "Sensors", "%s OK", bmp3xx_.variantLabel());
        return true;
    }
    if (dps310_.start()) {
        pressure_sensor_ = PRESSURE_DPS310;
        LOGI("Sensors", "DPS310 OK");
        return true;
    }
    return false;
}

bool SensorManager::detectHchoSensor() {
    hcho_sensor_type_ = HCHO_SENSOR_NONE;
    const bool hcho_warm_restart = (boot_reset_reason != ESP_RST_POWERON);
    bool sfa30_identified = false;

    if (hcho_warm_restart) {
        sfa30_identified = sfa30_.probe();
        if (sfa30_identified) {
            sfa30_.start();
            if (sfa30_.status() == Sfa30::Status::Ok) {
                hcho_sensor_type_ = HCHO_SENSOR_SFA30;
                Logger::log(Logger::Info, "Sensors", "%s OK", sfa30_.label());
                return true;
            }
        }
    }

    if (!sfa30_identified) {
        sfa40_.start();
        if (sfa40_.status() == Sfa40::Status::Ok) {
            hcho_sensor_type_ = HCHO_SENSOR_SFA40;
            if (sfa40_.isWarmupActive()) {
                Logger::log(Logger::Info, "Sensors", "%s starting", sfa40_.label());
            } else {
                Logger::log(Logger::Info, "Sensors", "%s OK", sfa40_.label());
            }
            return true;
        }
    }

    if (!sfa30_identified &&
        (sfa40_.status() == Sfa40::Status::Absent || sfa40_.shouldFallbackToSfa30())) {
        sfa30_.start();
        if (sfa30_.status() == Sfa30::Status::Ok) {
            hcho_sensor_type_ = HCHO_SENSOR_SFA30;
            Logger::log(Logger::Info, "Sensors", "%s OK", sfa30_.label());
            return true;
        }
    }

    return false;
}

void SensorManager::startNextLateProbe(uint32_t now_ms, bool co2_asc_enabled) {
    const bool due[3] = {
        pressure_sensor_ == PRESSURE_NONE && pressure_probe_.shouldAttempt(now_ms),
        hcho_sensor_type_ == HCHO_SENSOR_NONE && hcho_probe_.shouldAttempt(now_ms),
        !sen66_.isOk() && !sen66_.isBusy() && sen66_probe_.shouldAttempt(now_ms),
    };

    for (uint8_t offset = 0; offset < 3U; ++offset) {
        const uint8_t candidate = static_cast<uint8_t>((late_probe_cursor_ + offset) % 3U);
        if (!due[candidate]) {
            continue;
        }
        late_probe_cursor_ = static_cast<uint8_t>((candidate + 1U) % 3U);
        late_driver_started_ = false;
        if (candidate == 0U) {
            pressure_late_stage_ = PressureLateStage::Bmp580;
            late_probe_kind_ = LateProbeKind::Pressure;
        } else if (candidate == 1U) {
            hcho_late_stage_ = (boot_reset_reason != ESP_RST_POWERON)
                ? HchoLateStage::Sfa30Warm
                : HchoLateStage::Sfa40;
            late_probe_kind_ = LateProbeKind::Hcho;
        } else {
            late_sen66_asc_enabled_ = co2_asc_enabled;
            late_probe_kind_ = LateProbeKind::Sen66;
        }
        return;
    }
}

void SensorManager::pollActiveLateProbe(uint32_t now_ms, PollResult &result) {
    if (!late_driver_started_) {
        switch (late_probe_kind_) {
            case LateProbeKind::Pressure:
                if (pressure_late_stage_ == PressureLateStage::Bmp580) {
                    bmp580_.beginLateStart();
                } else if (pressure_late_stage_ == PressureLateStage::Bmp3xx) {
                    bmp3xx_.beginLateStart();
                } else {
                    dps310_.beginLateStart();
                }
                break;
            case LateProbeKind::Hcho:
                if (hcho_late_stage_ == HchoLateStage::Sfa40) {
                    sfa40_.beginLateStart();
                } else {
                    sfa30_.beginLateStart();
                }
                break;
            case LateProbeKind::Sen66:
                sen66_.beginLateStart(late_sen66_asc_enabled_);
                break;
            case LateProbeKind::None:
            default:
                return;
        }
        late_driver_started_ = true;
        return;
    }

    CooperativeStart::Result start_result = CooperativeStart::Result::Failed;
    if (late_probe_kind_ == LateProbeKind::Pressure) {
        if (pressure_late_stage_ == PressureLateStage::Bmp580) {
            start_result = bmp580_.pollLateStart(now_ms);
        } else if (pressure_late_stage_ == PressureLateStage::Bmp3xx) {
            start_result = bmp3xx_.pollLateStart(now_ms);
        } else {
            start_result = dps310_.pollLateStart(now_ms);
        }
        if (start_result == CooperativeStart::Result::InProgress) {
            return;
        }
        if (start_result == CooperativeStart::Result::Success) {
            pressure_sensor_ = pressure_late_stage_ == PressureLateStage::Bmp580
                ? PRESSURE_BMP58X
                : (pressure_late_stage_ == PressureLateStage::Bmp3xx
                       ? PRESSURE_BMP3XX
                       : PRESSURE_DPS310);
            if (pressure_sensor_ == PRESSURE_BMP58X) {
                Logger::log(Logger::Info, "Sensors", "%s OK", bmp580_.variantLabel());
            } else if (pressure_sensor_ == PRESSURE_BMP3XX) {
                Logger::log(Logger::Info, "Sensors", "%s OK", bmp3xx_.variantLabel());
            } else {
                LOGI("Sensors", "DPS310 OK");
            }
            finishPressureLateProbe(true, result);
            return;
        }
        if (pressure_late_stage_ == PressureLateStage::Bmp580) {
            pressure_late_stage_ = PressureLateStage::Bmp3xx;
            late_driver_started_ = false;
            return;
        }
        if (pressure_late_stage_ == PressureLateStage::Bmp3xx) {
            pressure_late_stage_ = PressureLateStage::Dps310;
            late_driver_started_ = false;
            return;
        }
        finishPressureLateProbe(false, result);
        return;
    }

    if (late_probe_kind_ == LateProbeKind::Hcho) {
        if (hcho_late_stage_ == HchoLateStage::Sfa40) {
            start_result = sfa40_.pollLateStart(now_ms);
        } else {
            start_result = sfa30_.pollLateStart(now_ms);
        }
        if (start_result == CooperativeStart::Result::InProgress) {
            return;
        }
        if (start_result == CooperativeStart::Result::Success) {
            hcho_sensor_type_ = hcho_late_stage_ == HchoLateStage::Sfa40
                ? HCHO_SENSOR_SFA40
                : HCHO_SENSOR_SFA30;
            if (hcho_sensor_type_ == HCHO_SENSOR_SFA40 && sfa40_.isWarmupActive()) {
                Logger::log(Logger::Info, "Sensors", "%s starting", sfa40_.label());
            } else {
                Logger::log(Logger::Info, "Sensors", "%s OK",
                            hcho_sensor_type_ == HCHO_SENSOR_SFA40
                                ? sfa40_.label()
                                : sfa30_.label());
            }
            finishHchoLateProbe(true, result);
            return;
        }
        if (hcho_late_stage_ == HchoLateStage::Sfa30Warm) {
            if (sfa30_.lateStartIdentified()) {
                finishHchoLateProbe(false, result);
            } else {
                hcho_late_stage_ = HchoLateStage::Sfa40;
                late_driver_started_ = false;
            }
            return;
        }
        if (hcho_late_stage_ == HchoLateStage::Sfa40 &&
            (sfa40_.status() == Sfa40::Status::Absent ||
             sfa40_.shouldFallbackToSfa30())) {
            hcho_late_stage_ = HchoLateStage::Sfa30Fallback;
            late_driver_started_ = false;
            return;
        }
        finishHchoLateProbe(false, result);
        return;
    }

    if (late_probe_kind_ == LateProbeKind::Sen66) {
        start_result = sen66_.pollLateStart(now_ms);
        if (start_result == CooperativeStart::Result::InProgress) {
            return;
        }
        finishSen66LateProbe(start_result == CooperativeStart::Result::Success, result);
    }
}

void SensorManager::finishPressureLateProbe(bool success, PollResult &result) {
    pressure_probe_.recordAttempt(success);
    late_probe_kind_ = LateProbeKind::None;
    late_driver_started_ = false;
    if (success) {
        result.data_changed = true;
    } else if (pressure_probe_.exhausted()) {
        result.data_changed = true;
        LOGI("Sensors", "Pressure sensor not found after startup probes");
    }
}

void SensorManager::finishHchoLateProbe(bool success, PollResult &result) {
    hcho_probe_.recordAttempt(success);
    late_probe_kind_ = LateProbeKind::None;
    late_driver_started_ = false;
    if (success) {
        result.data_changed = true;
    }
    if (!success && hcho_probe_.exhausted()) {
        result.data_changed = true;
        const bool any_fault =
            (sfa40_.status() == Sfa40::Status::Fault) ||
            (sfa30_.status() == Sfa30::Status::Fault);
        Logger::log(any_fault ? Logger::Warn : Logger::Info,
                    "Sensors",
                    any_fault ? "HCHO sensor init failed after startup probes"
                              : "HCHO sensor not installed after startup probes");
    }
}

void SensorManager::finishSen66LateProbe(bool success, PollResult &result) {
    sen66_probe_.recordAttempt(success);
    late_probe_kind_ = LateProbeKind::None;
    late_driver_started_ = false;
    if (success) {
        result.data_changed = true;
        LOGI("Sensors", "SEN66 OK");
        return;
    }

    LOGW("Sensors", "SEN66 not found (%u/%u)",
         static_cast<unsigned>(sen66_probe_.attempts()),
         static_cast<unsigned>(StartupProbePolicy::kMaxAttempts));
    if (!sen66_probe_.pending()) {
        result.data_changed = true;
        LOGW("Sensors", "SEN66 start attempts exhausted, stop probing until reboot");
    }
}

bool SensorManager::isPressureOk() const {
    if (!initialized_) {
        return false;
    }
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

SensorManager::SfaStatus SensorManager::currentHchoStatus() const {
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        return sfa40_.status();
    }
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        return static_cast<SfaStatus>(sfa30_.status());
    }
    if (sfa40_.status() == Sfa40::Status::Fault || sfa30_.status() == Sfa30::Status::Fault) {
        return SfaStatus::Fault;
    }
    return SfaStatus::Absent;
}

bool SensorManager::currentHchoWarmupActive() const {
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        return sfa40_.isWarmupActive();
    }
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        return sfa30_.isWarmupActive();
    }
    return false;
}

bool SensorManager::currentHchoTakeNewData(float &hcho_ppb) {
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        return sfa40_.takeNewData(hcho_ppb);
    }
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        return sfa30_.takeNewData(hcho_ppb);
    }
    return false;
}

void SensorManager::currentHchoInvalidate() {
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        sfa40_.invalidate();
    } else if (hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        sfa30_.invalidate();
    }
}

uint32_t SensorManager::currentHchoLastDataMs() const {
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA40) {
        return sfa40_.lastDataMs();
    }
    if (hcho_sensor_type_ == HCHO_SENSOR_SFA30) {
        return sfa30_.lastDataMs();
    }
    return 0;
}

float SensorManager::currentHchoMinPpb() const {
    return hcho_sensor_type_ == HCHO_SENSOR_SFA30
               ? Config::SFA30_HCHO_MIN_PPB
               : Config::SFA40_HCHO_MIN_PPB;
}

float SensorManager::currentHchoMaxPpb() const {
    return hcho_sensor_type_ == HCHO_SENSOR_SFA30
               ? Config::SFA30_HCHO_MAX_PPB
               : Config::SFA40_HCHO_MAX_PPB;
}

const char *SensorManager::pressureSensorLabel() const {
    switch (pressure_sensor_) {
        case PRESSURE_BMP58X:
            switch (bmp580_.variant()) {
                case Bmp580::Variant::BMP585:
                    return "BMP585:";
                case Bmp580::Variant::BMP580_581:
                    return "BMP580/581:";
                default:
                    return "BMP58x:";
            }
        case PRESSURE_BMP3XX:
            switch (bmp3xx_.variant()) {
                case Bmp3xx::Variant::BMP388:
                    return "BMP388:";
                case Bmp3xx::Variant::BMP390:
                    return "BMP390:";
                default:
                    return "BMP3xx:";
            }
        case PRESSURE_DPS310:
            return "DPS310:";
        default:
            return "PRESS:";
    }
}

const char *SensorManager::hchoSensorLabel() const {
    switch (hcho_sensor_type_) {
        case HCHO_SENSOR_SFA30:
            return sfa30_.label();
        case HCHO_SENSOR_SFA40:
            return sfa40_.label();
        case HCHO_SENSOR_NONE:
        default:
            break;
    }

    if (sfa30_.status() == Sfa30::Status::Fault) {
        return sfa30_.label();
    }
    if (sfa40_.status() == Sfa40::Status::Fault) {
        return sfa40_.label();
    }
    return "SFA30/40";
}

void SensorManager::setOffsets(float temp_offset, float hum_offset) {
    if (!initialized_) {
        return;
    }
    SharedI2cLease shared_i2c(*this);
    if (!shared_i2c) {
        return;
    }
    sen66_.setOffsets(temp_offset, hum_offset);
}

bool SensorManager::deviceReset() {
    if (!initialized_) {
        return false;
    }
    SharedI2cLease shared_i2c(*this);
    return shared_i2c && sen66_.deviceReset();
}

void SensorManager::scheduleRetry(uint32_t delay_ms) {
    if (!initialized_ || !isSharedI2cAvailable()) {
        return;
    }
    sen66_probe_.reset(millis() + delay_ms);
}

bool SensorManager::start(bool asc_enabled) {
    if (!initialized_) {
        return false;
    }
    SharedI2cLease shared_i2c(*this);
    return shared_i2c && sen66_.start(asc_enabled);
}

bool SensorManager::setAscEnabled(bool enabled) {
    if (!initialized_) {
        return false;
    }
    SharedI2cLease shared_i2c(*this);
    return shared_i2c && sen66_.setAscEnabled(enabled);
}

bool SensorManager::calibrateFrc(uint16_t ref_ppm,
                                 bool has_pressure,
                                 float pressure_hpa,
                                 uint16_t &correction) {
    if (!initialized_) {
        return false;
    }
    SharedI2cLease shared_i2c(*this);
    return shared_i2c &&
           sen66_.calibrateFRC(ref_ppm, has_pressure, pressure_hpa, correction);
}

void SensorManager::clearVocState(StorageManager &storage) {
    if (!initialized_) {
        return;
    }
    SharedI2cLease shared_i2c(*this);
    if (!shared_i2c) {
        return;
    }
    sen66_.clearVocState(storage);
}
