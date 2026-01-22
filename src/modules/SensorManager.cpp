#include "modules/SensorManager.h"

#include <math.h>
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

    data.pm_valid = data.pm25_valid || data.pm10_valid;
    if (data.pm_valid) {
        if (isfinite(data.pm1)) {
            float clamped = clampf(data.pm1, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm1) {
                data.pm1 = clamped;
                changed = true;
            }
        } else if (data.pm1 != 0.0f) {
            data.pm1 = 0.0f;
            changed = true;
        }
        if (isfinite(data.pm4)) {
            float clamped = clampf(data.pm4, Config::SEN66_PM_MIN_UGM3, Config::SEN66_PM_MAX_UGM3);
            if (clamped != data.pm4) {
                data.pm4 = clamped;
                changed = true;
            }
        } else if (data.pm4 != 0.0f) {
            data.pm4 = 0.0f;
            changed = true;
        }
    } else {
        if (data.pm1 != 0.0f) {
            data.pm1 = 0.0f;
            changed = true;
        }
        if (data.pm4 != 0.0f) {
            data.pm4 = 0.0f;
            changed = true;
        }
    }

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

void log_soft_warnings(const SensorData &data) {
    static bool temp_outside = false;
    static bool hum_outside = false;

    bool temp_now = data.temp_valid &&
                    (data.temperature < Config::SEN66_TEMP_RECOMM_MIN_C ||
                     data.temperature > Config::SEN66_TEMP_RECOMM_MAX_C);
    if (temp_now && !temp_outside) {
        LOGW("Sensors", "Temperature outside recommended range: %.1f C", data.temperature);
    }
    temp_outside = temp_now;

    bool hum_now = data.hum_valid &&
                   (data.humidity < Config::SEN66_HUM_RECOMM_MIN ||
                    data.humidity > Config::SEN66_HUM_RECOMM_MAX);
    if (hum_now && !hum_outside) {
        LOGW("Sensors", "Humidity outside recommended range: %.0f%%", data.humidity);
    }
    hum_outside = hum_now;
}

} // namespace

void SensorManager::begin(StorageManager &storage, float temp_offset, float hum_offset) {
    sen66_.begin();
    sen66_.setOffsets(temp_offset, hum_offset);
    sen66_.loadVocState(storage);

    bmp580_.begin();
    if (bmp580_.start()) {
        pressure_sensor_ = PRESSURE_BMP580;
        LOGI("Sensors", "BMP580 OK");
    } else {
        dps310_.begin();
        if (dps310_.start()) {
            pressure_sensor_ = PRESSURE_DPS310;
            LOGI("Sensors", "DPS310 OK");
        } else {
            pressure_sensor_ = PRESSURE_NONE;
            LOGW("Sensors", "Pressure sensor not found");
        }
    }

    sfa3x_.begin();
    sfa3x_.start();
    if (sfa3x_.isOk()) {
        LOGI("Sensors", "SFA30 OK");
    } else {
        LOGW("Sensors", "SFA30 not found");
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

    float pressure_hpa = 0.0f;
    float temperature_c = 0.0f;
    bool pressure_valid = false;
    bool pressure_new = false;
    if (pressure_sensor_ == PRESSURE_BMP580) {
        bmp580_.poll();
        if (bmp580_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = bmp580_.isPressureValid();
    } else if (pressure_sensor_ == PRESSURE_DPS310) {
        dps310_.poll();
        if (dps310_.takeNewData(pressure_hpa, temperature_c)) {
            pressure_new = true;
        }
        pressure_valid = dps310_.isPressureValid();
    }
    if (pressure_new) {
        if (!isfinite(pressure_hpa) ||
            pressure_hpa < Config::DPS310_PRESSURE_MIN_HPA ||
            pressure_hpa > Config::DPS310_PRESSURE_MAX_HPA) {
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
    if (!sen66_.isOk() && !sen66_.isBusy() && now >= sen66_.retryAtMs()) {
        if (sen66_.start(co2_asc_enabled)) {
            LOGI("Sensors", "SEN66 OK");
        } else {
            LOGW("Sensors", "SEN66 not found");
            sen66_.scheduleRetry(Config::SEN66_START_RETRY_MS);
        }
    }

    if (apply_sanity_filters(data)) {
        result.data_changed = true;
    }
    log_soft_warnings(data);

    bool warmup_now = sen66_.isWarmupActive();
    if (warmup_now != warmup_active_last_) {
        warmup_active_last_ = warmup_now;
        result.warmup_changed = true;
    }

    uint32_t sen66_last_ms = sen66_.lastDataMs();
    if (sen66_last_ms != 0 && (now - sen66_last_ms > Config::SEN66_STALE_MS)) {
        data = SensorData();
        result.data_changed = true;
    }
    uint32_t sfa_last_ms = sfa3x_.lastDataMs();
    if (data.hcho_valid && sfa_last_ms != 0 &&
        (now - sfa_last_ms > Config::SFA3X_STALE_MS)) {
        data.hcho_valid = false;
        sfa3x_.invalidate();
        result.data_changed = true;
    }

    return result;
}

bool SensorManager::isPressureOk() const {
    if (pressure_sensor_ == PRESSURE_BMP580) {
        return bmp580_.isOk();
    }
    if (pressure_sensor_ == PRESSURE_DPS310) {
        return dps310_.isOk();
    }
    return false;
}

const char *SensorManager::pressureSensorLabel() const {
    switch (pressure_sensor_) {
        case PRESSURE_BMP580:
            return "BMP580:";
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
