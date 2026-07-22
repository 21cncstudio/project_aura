// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/MqttPayloadBuilder.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "core/MathUtils.h"
#include "core/AirQualityEngine.h"
#include "config/AppConfig.h"
#include "drivers/DfrOptionalGasSensor.h"

namespace MqttPayloadBuilder {

namespace {

class BufferWriter {
public:
    BufferWriter(char *out, size_t out_size) : out_(out), out_size_(out_size) {
        if (out_ && out_size_ > 0) {
            out_[0] = '\0';
        }
    }

    bool appendf(const char *fmt, ...) {
        if (!out_ || out_size_ == 0 || failed_) {
            return false;
        }
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(out_ + used_, out_size_ - used_, fmt, args);
        va_end(args);
        if (written < 0 || static_cast<size_t>(written) >= (out_size_ - used_)) {
            failed_ = true;
            out_[out_size_ - 1] = '\0';
            return false;
        }
        used_ += static_cast<size_t>(written);
        return true;
    }

    size_t size() const {
        return failed_ ? 0 : used_;
    }

private:
    char *out_ = nullptr;
    size_t out_size_ = 0;
    size_t used_ = 0;
    bool failed_ = false;
};

void append_json_escaped(String &out, const char *value) {
    if (!value) {
        return;
    }
    const uint8_t *p = reinterpret_cast<const uint8_t *>(value);
    while (*p) {
        switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(*p));
                    out += buf;
                } else {
                    out += static_cast<char>(*p);
                }
                break;
        }
        ++p;
    }
}

void append_json_escaped(String &out, const String &value) {
    append_json_escaped(out, value.c_str());
}

void build_state_topic(char *out, size_t out_size, const String &base) {
    snprintf(out, out_size, "%s/state", base.c_str());
}

void build_availability_topic(char *out, size_t out_size, const String &base) {
    snprintf(out, out_size, "%s/status", base.c_str());
}

bool string_is_empty(const String &text) {
    return text.length() == 0;
}

bool string_ends_with_underscore(const String &text) {
    return text.length() > 0 && text[text.length() - 1] == '_';
}

void string_pop_back(String &text) {
    if (text.length() == 0) {
        return;
    }
#ifdef UNIT_TEST
    text.pop_back();
#else
    text.remove(text.length() - 1);
#endif
}

void append_slug_component(String &out, const char *text) {
    if (!text || text[0] == '\0') {
        return;
    }

    bool previous_was_separator = string_is_empty(out) || string_ends_with_underscore(out);
    const uint8_t *p = reinterpret_cast<const uint8_t *>(text);
    while (*p) {
        const char c = static_cast<char>(*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
            previous_was_separator = false;
        } else if (c >= 'A' && c <= 'Z') {
            out += static_cast<char>(c - 'A' + 'a');
            previous_was_separator = false;
        } else if (!previous_was_separator) {
            out += '_';
            previous_was_separator = true;
        }
        ++p;
    }

    while (string_ends_with_underscore(out)) {
        string_pop_back(out);
    }
}

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

uint8_t compute_fan_output_percent(const FanStateSnapshot &fan) {
    if (!fan.output_known || Config::DAC_VOUT_FULL_SCALE_MV == 0) {
        return 0;
    }
    uint32_t percent = static_cast<uint32_t>(fan.output_mv) * 100u;
    percent = (percent + (Config::DAC_VOUT_FULL_SCALE_MV / 2u)) / Config::DAC_VOUT_FULL_SCALE_MV;
    if (percent > 100u) {
        percent = 100u;
    }
    return static_cast<uint8_t>(percent);
}

uint8_t compute_fan_manual_speed(const FanStateSnapshot &fan) {
    uint8_t step = fan.manual_step;
    if (step < 1u) {
        step = 1u;
    } else if (step > 10u) {
        step = 10u;
    }
    return step;
}

uint8_t compute_fan_manual_percent(const FanStateSnapshot &fan) {
    return static_cast<uint8_t>(compute_fan_manual_speed(fan) * 10u);
}

const char *fan_mode_text(FanMode mode) {
    return mode == FanMode::Auto ? "auto" : "manual";
}

bool fan_manual_running(const FanStateSnapshot &fan) {
    return fan.running && fan.manual_override_active;
}

bool fan_auto_enabled(const FanStateSnapshot &fan) {
    return fan.mode == FanMode::Auto && !fan.auto_resume_blocked;
}

bool fan_stopped(const FanStateSnapshot &fan) {
    return !fan_auto_enabled(fan) && !fan_manual_running(fan);
}

bool pressure_altitude_configured(bool pressure_altitude_set) {
    return pressure_altitude_set;
}

float pressure_absolute_to_msl_hpa(float pressure_hpa, int altitude_m) {
    if (!isfinite(pressure_hpa)) {
        return pressure_hpa;
    }
    const float base = 1.0f - (static_cast<float>(altitude_m) / 44330.0f);
    if (!isfinite(base) || base <= 0.0f) {
        return pressure_hpa;
    }
    const float corrected = pressure_hpa / powf(base, 5.255f);
    return isfinite(corrected) ? corrected : pressure_hpa;
}

float pressure_to_publish(float pressure_hpa, bool pressure_altitude_set, int altitude_m) {
    return pressure_altitude_configured(pressure_altitude_set)
               ? pressure_absolute_to_msl_hpa(pressure_hpa, altitude_m)
               : pressure_hpa;
}

float pressure_delta_to_publish(float pressure_delta_hpa,
                                bool pressure_altitude_set,
                                int altitude_m) {
    // At a fixed altitude, MSL correction is a constant multiplier, so deltas scale the same way.
    return pressure_altitude_configured(pressure_altitude_set)
               ? pressure_absolute_to_msl_hpa(pressure_delta_hpa, altitude_m)
               : pressure_delta_hpa;
}

const char *fan_control_mode_text(const FanStateSnapshot &fan) {
    if (fan_auto_enabled(fan)) {
        return "Auto";
    }
    if (fan_manual_running(fan)) {
        return "Manual";
    }
    return "Stopped";
}

const char *fan_timer_text(uint32_t seconds) {
    if (seconds == Config::DAC_TIMER_NONE_S) {
        return "Off";
    }
    if (seconds == 600U) {
        return "10 min";
    }
    if (seconds == 1800U) {
        return "30 min";
    }
    if (seconds == 3600U) {
        return "1 h";
    }
    if (seconds == 7200U) {
        return "2 h";
    }
    if (seconds == 14400U) {
        return "4 h";
    }
    if (seconds == 28800U) {
        return "8 h";
    }
    return "Off";
}

bool fan_timer_running(const FanStateSnapshot &fan, uint32_t now_ms) {
    return fan.running &&
           fan.manual_override_active &&
           fan.stop_at_ms != 0 &&
           static_cast<int32_t>(now_ms - fan.stop_at_ms) < 0;
}

uint32_t fan_timer_remaining_seconds(const FanStateSnapshot &fan, uint32_t now_ms) {
    if (!fan_timer_running(fan, now_ms)) {
        return 0;
    }
    return (fan.stop_at_ms - now_ms + 999UL) / 1000UL;
}

void format_fan_timer_remaining(char *out, size_t out_size, uint32_t seconds) {
    if (!out || out_size == 0) {
        return;
    }
    if (seconds == 0) {
        snprintf(out, out_size, "Off");
        return;
    }
    const uint32_t hours = seconds / 3600UL;
    const uint32_t minutes = (seconds % 3600UL) / 60UL;
    const uint32_t secs = seconds % 60UL;
    if (hours > 0) {
        if (minutes > 0) {
            snprintf(out, out_size, "%lu h %02lu min",
                     static_cast<unsigned long>(hours),
                     static_cast<unsigned long>(minutes));
        } else {
            snprintf(out, out_size, "%lu h", static_cast<unsigned long>(hours));
        }
        return;
    }
    if (minutes > 0) {
        snprintf(out, out_size, "%lu min", static_cast<unsigned long>(minutes));
        return;
    }
    snprintf(out, out_size, "%lu s", static_cast<unsigned long>(secs));
}

const char *fan_status_text(const FanStateSnapshot &fan) {
    if (fan.faulted) {
        return "FAULT";
    }
    if (!fan.present) {
        return "OFFLINE";
    }
    if (!fan.available) {
        return "OFFLINE";
    }
    return fan.running ? "RUNNING" : "STOPPED";
}

const char *air_status_text(const AirQualityEngine::Result &aqi) {
    if (!aqi.valid) {
        return "Unknown";
    }

    switch (aqi.band) {
        case AirQualityEngine::Band::Excellent:
            return "Excellent";
        case AirQualityEngine::Band::Good:
            return "Good";
        case AirQualityEngine::Band::Moderate:
            return "Fair";
        case AirQualityEngine::Band::Poor:
            return "Poor";
        case AirQualityEngine::Band::Invalid:
        default:
            return "Unknown";
    }
}

const char *main_issue_text(const AirQualityEngine::Result &aqi) {
    if (!aqi.valid || aqi.band == AirQualityEngine::Band::Invalid) {
        return "Unknown";
    }
    if (aqi.band == AirQualityEngine::Band::Excellent ||
        aqi.band == AirQualityEngine::Band::Good) {
        return "Clear";
    }

    switch (aqi.dominant_metric) {
        case AirQualityEngine::Metric::PM05:
        case AirQualityEngine::Metric::PM1:
        case AirQualityEngine::Metric::PM25:
        case AirQualityEngine::Metric::PM4:
        case AirQualityEngine::Metric::PM10:
            return "Particles";
        case AirQualityEngine::Metric::CO2:
            return "CO2";
        case AirQualityEngine::Metric::VOC:
            return "VOC";
        case AirQualityEngine::Metric::NOX:
            return "NOx";
        case AirQualityEngine::Metric::HCHO:
            return "HCHO";
        case AirQualityEngine::Metric::CO:
            return "CO";
        case AirQualityEngine::Metric::None:
        default:
            return "Unknown";
    }
}

using OptionalGasType = DfrOptionalGasSensor::OptionalGasType;

OptionalGasType optional_gas_type_from_data(const SensorData &data) {
    return static_cast<OptionalGasType>(data.optional_gas_type);
}

bool optional_gas_type_known(const SensorData &data) {
    return data.optional_gas_sensor_present &&
           optional_gas_type_from_data(data) != OptionalGasType::None;
}

bool optional_gas_value_valid(const SensorData &data) {
    return data.optional_gas_sensor_present &&
           data.optional_gas_valid &&
           optional_gas_type_from_data(data) != OptionalGasType::None &&
           isfinite(data.optional_gas_ppm) &&
           data.optional_gas_ppm >= 0.0f;
}

bool optional_gas_value_valid_for_type(const SensorData &data, OptionalGasType type) {
    return optional_gas_value_valid(data) &&
           optional_gas_type_from_data(data) == type;
}

const char *optional_gas_type_text(const SensorData &data) {
    return optional_gas_type_known(data)
               ? DfrOptionalGasSensor::optionalGasLabel(optional_gas_type_from_data(data))
               : nullptr;
}

const char *optional_gas_unit_text(const SensorData &data) {
    return optional_gas_type_known(data)
               ? DfrOptionalGasSensor::unitForType(optional_gas_type_from_data(data))
               : nullptr;
}

int optional_gas_ppm_decimals(const SensorData &data, float value) {
    const int decimals = data.optional_gas_ppm_decimals <= 2 ? data.optional_gas_ppm_decimals : 1;
    if (decimals == 2 && isfinite(value) && value >= 1.0f) {
        return 1;
    }
    return decimals;
}

} // namespace

String buildDiscoveryEntityObjectId(const String &base_topic,
                                    const char *object_id) {
    String entity_object_id;
    entity_object_id.reserve(base_topic.length() + (object_id ? strlen(object_id) : 0) + 4);
    append_slug_component(entity_object_id, base_topic.c_str());
    if (object_id && object_id[0] != '\0') {
        if (!string_is_empty(entity_object_id) && !string_ends_with_underscore(entity_object_id)) {
            entity_object_id += "_";
        }
        append_slug_component(entity_object_id, object_id);
    }
    if (string_is_empty(entity_object_id)) {
        entity_object_id = "project_aura";
    }
    return entity_object_id;
}

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
                                   const char *icon) {
    String payload;
    payload.reserve(520); // Discovery sensor payload ~450 bytes; keep headroom for long IDs.
    payload = "{";
    payload += "\"name\":\"";
    append_json_escaped(payload, name);
    payload += "\",\"unique_id\":\"";
    append_json_escaped(payload, device_id);
    payload += "_";
    append_json_escaped(payload, object_id);
    payload += "\",\"state_topic\":\"";
    char topic[256];
    build_state_topic(topic, sizeof(topic), base_topic);
    append_json_escaped(payload, topic);
    if (entity_object_id && entity_object_id[0] != '\0') {
        payload += "\",\"object_id\":\"";
        append_json_escaped(payload, entity_object_id);
    }
    payload += "\",\"availability_topic\":\"";
    build_availability_topic(topic, sizeof(topic), base_topic);
    append_json_escaped(payload, topic);
    payload += "\",\"payload_available\":\"";
    payload += Config::MQTT_AVAIL_ONLINE;
    payload += "\",\"payload_not_available\":\"";
    payload += Config::MQTT_AVAIL_OFFLINE;
    payload += "\"";
    if (value_template && value_template[0] != '\0') {
        payload += ",\"value_template\":\"";
        append_json_escaped(payload, value_template);
        payload += "\"";
    }
    if (unit && unit[0] != '\0') {
        payload += ",\"unit_of_measurement\":\"";
        payload += unit;
        payload += "\"";
    }
    if (device_class && device_class[0] != '\0') {
        payload += ",\"device_class\":\"";
        payload += device_class;
        payload += "\"";
    }
    if (state_class && state_class[0] != '\0') {
        payload += ",\"state_class\":\"";
        payload += state_class;
        payload += "\"";
    }
    if (icon && icon[0] != '\0') {
        payload += ",\"icon\":\"";
        append_json_escaped(payload, icon);
        payload += "\"";
    }
    payload += ",\"device\":{\"identifiers\":[\"";
    append_json_escaped(payload, device_id);
    payload += "\"],\"name\":\"";
    append_json_escaped(payload, device_name);
    payload += "\",\"manufacturer\":\"21CNCStudio\",\"model\":\"Project Aura\"}";
    payload += "}";
    return payload;
}

String buildStatePayload(const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on) {
    return buildStatePayload(data,
                             fan,
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             false,
                             0);
}

String buildStatePayload(const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m) {
    char payload[Config::MQTT_BUFFER_SIZE] = {};
    if (buildStatePayload(payload,
                          sizeof(payload),
                          data,
                          fan,
                          gas_warmup,
                          night_mode,
                          alert_blink,
                          backlight_on,
                          pressure_altitude_set,
                          pressure_altitude_m) == 0) {
        return String();
    }
    return String(payload);
}

String buildStatePayload(const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on) {
    return buildStatePayload(data,
                             FanStateSnapshot{},
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             false,
                             0);
}

String buildStatePayload(const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m) {
    return buildStatePayload(data,
                             FanStateSnapshot{},
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             pressure_altitude_set,
                             pressure_altitude_m);
}

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on) {
    return buildStatePayload(out,
                             out_size,
                             data,
                             fan,
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             false,
                             0);
}

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         const FanStateSnapshot &fan,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m) {
    BufferWriter payload(out, out_size);
    if (!payload.appendf("{")) {
        return 0;
    }
    bool first = true;
    auto add_int = [&](const char *key, bool valid, int value) {
        if (!payload.appendf("%s\"%s\":", first ? "" : ",", key)) {
            return false;
        }
        first = false;
        if (valid) {
            return payload.appendf("%d", value);
        } else {
            return payload.appendf("null");
        }
    };
    auto add_float = [&](const char *key, bool valid, float value, int decimals) {
        if (!payload.appendf("%s\"%s\":", first ? "" : ",", key)) {
            return false;
        }
        first = false;
        if (valid) {
            return payload.appendf("%.*f", decimals, static_cast<double>(value));
        } else {
            return payload.appendf("null");
        }
    };
    auto add_bool = [&](const char *key, bool value) {
        if (!payload.appendf("%s\"%s\":\"%s\"",
                             first ? "" : ",",
                             key,
                             value ? "ON" : "OFF")) {
            return false;
        }
        first = false;
        return true;
    };
    auto add_cstr = [&](const char *key, const char *value) {
        if (!payload.appendf("%s\"%s\":\"%s\"",
                             first ? "" : ",",
                             key,
                             value ? value : "")) {
            return false;
        }
        first = false;
        return true;
    };
    auto add_nullable_cstr = [&](const char *key, const char *value) {
        if (!payload.appendf("%s\"%s\":", first ? "" : ",", key)) {
            return false;
        }
        first = false;
        if (!value || value[0] == '\0') {
            return payload.appendf("null");
        }
        return payload.appendf("\"%s\"", value);
    };

    float dew_c = NAN;
    bool dew_valid = data.temp_valid && data.hum_valid;
    if (dew_valid) {
        dew_c = compute_dew_point_c(data.temperature, data.humidity);
        dew_valid = isfinite(dew_c);
    }
    float ah_gm3 = NAN;
    bool ah_valid = data.temp_valid && data.hum_valid;
    if (ah_valid) {
        ah_gm3 = MathUtils::compute_absolute_humidity_gm3(data.temperature, data.humidity);
        ah_valid = isfinite(ah_gm3);
    }
    const AirQualityEngine::Result aqi = AirQualityEngine::evaluate(data, gas_warmup);
    const float pressure_published =
        pressure_to_publish(data.pressure, pressure_altitude_set, pressure_altitude_m);
    const float pressure_delta_3h_published =
        pressure_delta_to_publish(data.pressure_delta_3h,
                                  pressure_altitude_set,
                                  pressure_altitude_m);
    const float pressure_delta_24h_published =
        pressure_delta_to_publish(data.pressure_delta_24h,
                                  pressure_altitude_set,
                                  pressure_altitude_m);

    if (!add_float("temp", data.temp_valid, data.temperature, 1) ||
        !add_float("humidity", data.hum_valid, data.humidity, 1) ||
        !add_float("dew_point", dew_valid, dew_c, 1) ||
        !add_float("absolute_humidity", ah_valid, ah_gm3, 1) ||
        !add_int("co2", data.co2_valid, data.co2) ||
        !add_int("aqi", aqi.valid, aqi.score)) {
        return 0;
    }
    const bool co_valid = data.co_sensor_present &&
                          data.co_valid &&
                          isfinite(data.co_ppm) &&
                          data.co_ppm >= 0.0f;
    const bool nh3_valid = optional_gas_value_valid_for_type(data, OptionalGasType::NH3);
    const bool so2_valid = optional_gas_value_valid_for_type(data, OptionalGasType::SO2);
    const bool no2_valid = optional_gas_value_valid_for_type(data, OptionalGasType::NO2);
    const bool h2s_valid = optional_gas_value_valid_for_type(data, OptionalGasType::H2S);
    const bool o3_valid = optional_gas_value_valid_for_type(data, OptionalGasType::O3);
    const bool o2_valid = optional_gas_value_valid_for_type(data, OptionalGasType::O2);
    const bool optional_gas_valid = optional_gas_value_valid(data);
    const bool optional_gas_known = optional_gas_type_known(data);
    const bool voc_publish_valid = !gas_warmup && data.voc_valid;
    const bool nox_publish_valid = !gas_warmup && data.nox_valid;
    const bool fan_output_valid = fan.present && fan.output_known;
    const bool fan_manual_speed_valid = fan.present;
    const uint8_t fan_manual_speed = compute_fan_manual_speed(fan);
    const uint8_t fan_manual_percent = compute_fan_manual_percent(fan);
    const uint8_t fan_output_percent = compute_fan_output_percent(fan);
    const uint32_t fan_timer_remaining = fan_timer_remaining_seconds(fan, millis());
    char fan_timer_remaining_text[24];
    format_fan_timer_remaining(fan_timer_remaining_text,
                               sizeof(fan_timer_remaining_text),
                               fan_timer_remaining);
    const int optional_decimals = optional_gas_ppm_decimals(data, data.optional_gas_ppm);
    const int nh3_decimals = optional_gas_ppm_decimals(data, data.nh3_ppm);
    if (!add_float("co", co_valid, data.co_ppm, 1) ||
        !add_float("optional_gas", optional_gas_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_nullable_cstr("optional_gas_type", optional_gas_type_text(data)) ||
        !add_nullable_cstr("optional_gas_unit", optional_gas_unit_text(data)) ||
        !add_int("optional_gas_decimals", optional_gas_known, optional_decimals) ||
        !add_int("optional_gas_ppm_decimals", optional_gas_known, optional_decimals) ||
        !add_float("nh3", nh3_valid, data.nh3_ppm, nh3_decimals) ||
        !add_float("o3", o3_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_float("so2", so2_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_float("no2", no2_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_float("h2s", h2s_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_float("o2", o2_valid, data.optional_gas_ppm, optional_decimals) ||
        !add_int("voc_index", voc_publish_valid, data.voc_index) ||
        !add_int("nox_index", nox_publish_valid, data.nox_index) ||
        !add_float("hcho", data.hcho_valid, data.hcho, 1) ||
        !add_float("pm05", data.pm05_valid, data.pm05, 1) ||
        !add_float("pm1", data.pm1_valid, data.pm1, 1) ||
        !add_float("pm4", data.pm4_valid, data.pm4, 1) ||
        !add_float("pm25", data.pm25_valid, data.pm25, 1) ||
        !add_float("pm10", data.pm10_valid, data.pm10, 1) ||
        !add_float("pressure", data.pressure_valid, pressure_published, 1) ||
        !add_float("pressure_absolute", data.pressure_valid, data.pressure, 1) ||
        !add_float("pressure_delta_3h", data.pressure_delta_3h_valid, pressure_delta_3h_published, 1) ||
        !add_float("pressure_delta_24h", data.pressure_delta_24h_valid, pressure_delta_24h_published, 1) ||
        !add_bool("fan_present", fan.present) ||
        !add_bool("fan_available", fan.available) ||
        !add_bool("fan_running", fan.running) ||
        !add_bool("fan_manual_running", fan_manual_running(fan)) ||
        !add_bool("fan_fault", fan.faulted) ||
        !add_bool("fan_auto", fan_auto_enabled(fan)) ||
        !add_bool("fan_stopped", fan_stopped(fan)) ||
        !add_cstr("fan_mode", fan_mode_text(fan.mode)) ||
        !add_cstr("fan_control_mode", fan_control_mode_text(fan)) ||
        !add_cstr("fan_timer", fan_timer_text(fan.selected_timer_s)) ||
        !add_cstr("fan_timer_remaining", fan_timer_remaining_text) ||
        !add_int("fan_manual_speed", fan_manual_speed_valid, fan_manual_speed) ||
        !add_int("fan_manual_percent", fan_manual_speed_valid, fan_manual_percent) ||
        !add_cstr("fan_status", fan_status_text(fan)) ||
        !add_int("fan_output_percent", fan_output_valid, fan_output_percent) ||
        !add_int("fan_output_mv", fan_output_valid, fan.output_mv) ||
        !add_bool("night_mode", night_mode) ||
        !add_bool("alert_blink", alert_blink) ||
        !add_cstr("air_status", air_status_text(aqi)) ||
        !add_cstr("main_issue", main_issue_text(aqi)) ||
        !add_bool("backlight", backlight_on) ||
        !payload.appendf("}")) {
        return 0;
    }

    return payload.size();
}

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on) {
    return buildStatePayload(out,
                             out_size,
                             data,
                             FanStateSnapshot{},
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             false,
                             0);
}

size_t buildStatePayload(char *out,
                         size_t out_size,
                         const SensorData &data,
                         bool gas_warmup,
                         bool night_mode,
                         bool alert_blink,
                         bool backlight_on,
                         bool pressure_altitude_set,
                         int16_t pressure_altitude_m) {
    return buildStatePayload(out,
                             out_size,
                             data,
                             FanStateSnapshot{},
                             gas_warmup,
                             night_mode,
                             alert_blink,
                             backlight_on,
                             pressure_altitude_set,
                             pressure_altitude_m);
}

} // namespace MqttPayloadBuilder

