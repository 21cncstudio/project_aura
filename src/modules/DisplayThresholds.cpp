// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/DisplayThresholds.h"

#include <math.h>

#include "core/Logger.h"

namespace DisplayThresholds {
namespace {

bool finite(float value) {
    return isfinite(value);
}

void set_error(String *error, const char *message) {
    if (error) {
        *error = message;
    }
}

bool validate_high(const High &h, const char *name, String *error) {
    if (!finite(h.green) || !finite(h.yellow) || !finite(h.orange)) {
        if (error) {
            *error = String(name) + " thresholds must be finite";
        }
        return false;
    }
    if (h.green < 0.0f || h.yellow < 0.0f || h.orange < 0.0f) {
        if (error) {
            *error = String(name) + " thresholds must be non-negative";
        }
        return false;
    }
    if (!(h.green < h.yellow && h.yellow < h.orange)) {
        if (error) {
            *error = String(name) + " thresholds must be green < yellow < orange";
        }
        return false;
    }
    return true;
}

bool validate_range(const Range &r, const char *name, bool allow_negative, String *error) {
    if (!finite(r.orange_min) || !finite(r.yellow_min) || !finite(r.good_min) ||
        !finite(r.good_max) || !finite(r.yellow_max) || !finite(r.orange_max)) {
        if (error) {
            *error = String(name) + " thresholds must be finite";
        }
        return false;
    }
    if (!allow_negative &&
        (r.orange_min < 0.0f || r.yellow_min < 0.0f || r.good_min < 0.0f ||
         r.good_max < 0.0f || r.yellow_max < 0.0f || r.orange_max < 0.0f)) {
        if (error) {
            *error = String(name) + " thresholds must be non-negative";
        }
        return false;
    }
    if (!(r.orange_min < r.yellow_min &&
          r.yellow_min < r.good_min &&
          r.good_min < r.good_max &&
          r.good_max < r.yellow_max &&
          r.yellow_max < r.orange_max)) {
        if (error) {
            *error = String(name) + " thresholds must be monotonic";
        }
        return false;
    }
    return true;
}

bool read_float_field(ArduinoJson::JsonObjectConst obj, const char *field, float &out, String *error) {
    ArduinoJson::JsonVariantConst value = obj[field];
    if (value.isNull()) {
        return true;
    }
    if (!value.is<float>()) {
        if (error) {
            *error = String(field) + " threshold must be numeric";
        }
        return false;
    }
    out = value.as<float>();
    return true;
}

bool read_bool_field(ArduinoJson::JsonObjectConst obj, const char *field, bool &out, String *error) {
    ArduinoJson::JsonVariantConst value = obj[field];
    if (value.isNull()) {
        return true;
    }
    if (!value.is<bool>()) {
        if (error) {
            *error = String(field) + " must be boolean";
        }
        return false;
    }
    out = value.as<bool>();
    return true;
}

bool read_object_field(ArduinoJson::JsonObjectConst obj,
                       const char *field,
                       ArduinoJson::JsonObjectConst &out,
                       String *error) {
    ArduinoJson::JsonVariantConst value = obj[field];
    if (value.isNull()) {
        out = ArduinoJson::JsonObjectConst();
        return true;
    }
    if (!value.is<ArduinoJson::JsonObjectConst>()) {
        if (error) {
            *error = String(field) + " must be an object";
        }
        return false;
    }
    out = value.as<ArduinoJson::JsonObjectConst>();
    return true;
}

bool read_high(ArduinoJson::JsonObjectConst obj, High &out, String *error) {
    if (obj.isNull()) {
        return true;
    }
    return read_float_field(obj, "green", out.green, error) &&
           read_float_field(obj, "yellow", out.yellow, error) &&
           read_float_field(obj, "orange", out.orange, error);
}

bool read_range(ArduinoJson::JsonObjectConst obj, Range &out, String *error) {
    if (obj.isNull()) {
        return true;
    }
    return read_float_field(obj, "orange_min", out.orange_min, error) &&
           read_float_field(obj, "yellow_min", out.yellow_min, error) &&
           read_float_field(obj, "good_min", out.good_min, error) &&
           read_float_field(obj, "good_max", out.good_max, error) &&
           read_float_field(obj, "yellow_max", out.yellow_max, error) &&
           read_float_field(obj, "orange_max", out.orange_max, error);
}

void write_high(ArduinoJson::JsonObject obj, const High &h) {
    obj["type"] = "high";
    obj["green"] = h.green;
    obj["yellow"] = h.yellow;
    obj["orange"] = h.orange;
}

void write_range(ArduinoJson::JsonObject obj, const Range &r) {
    obj["type"] = "range";
    obj["orange_min"] = r.orange_min;
    obj["yellow_min"] = r.yellow_min;
    obj["good_min"] = r.good_min;
    obj["good_max"] = r.good_max;
    obj["yellow_max"] = r.yellow_max;
    obj["orange_max"] = r.orange_max;
}

bool apply_metrics_update(ArduinoJson::JsonObjectConst metrics, Config &out, String *error) {
    if (metrics.isNull()) {
        return true;
    }
    ArduinoJson::JsonObjectConst temp;
    ArduinoJson::JsonObjectConst rh;
    ArduinoJson::JsonObjectConst dew_point;
    ArduinoJson::JsonObjectConst ah;
    ArduinoJson::JsonObjectConst co2;
    ArduinoJson::JsonObjectConst hcho;
    ArduinoJson::JsonObjectConst co;
    ArduinoJson::JsonObjectConst voc;
    ArduinoJson::JsonObjectConst nox;
    return read_object_field(metrics, "temp", temp, error) &&
           read_object_field(metrics, "rh", rh, error) &&
           read_object_field(metrics, "dew_point", dew_point, error) &&
           read_object_field(metrics, "ah", ah, error) &&
           read_object_field(metrics, "co2", co2, error) &&
           read_object_field(metrics, "hcho", hcho, error) &&
           read_object_field(metrics, "co", co, error) &&
           read_object_field(metrics, "voc", voc, error) &&
           read_object_field(metrics, "nox", nox, error) &&
           read_range(temp, out.temp, error) &&
           read_range(rh, out.rh, error) &&
           read_range(dew_point, out.dew_point, error) &&
           read_range(ah, out.ah, error) &&
           read_high(co2, out.co2, error) &&
           read_high(hcho, out.hcho, error) &&
           read_high(co, out.co, error) &&
           read_high(voc, out.voc, error) &&
           read_high(nox, out.nox, error);
}

bool apply_background_update(ArduinoJson::JsonObjectConst obj, BackgroundAlerts &alerts, String *error) {
    if (obj.isNull()) {
        return true;
    }
    return read_bool_field(obj, "hcho_enabled", alerts.hcho_enabled, error) &&
           read_bool_field(obj, "co_enabled", alerts.co_enabled, error) &&
           read_bool_field(obj, "co2_enabled", alerts.co2_enabled, error);
}

}  // namespace

Config defaults() {
    Config cfg{};
    cfg.version = kVersion;
    cfg.temp = {16.0f, 18.0f, 20.0f, 25.0f, 26.0f, 28.0f};
    cfg.rh = {20.0f, 30.0f, 40.0f, 60.0f, 65.0f, 70.0f};
    cfg.dew_point = {5.0f, 8.0f, 10.0f, 16.0f, 18.0f, 21.0f};
    cfg.ah = {4.0f, 5.0f, 7.0f, 15.0f, 18.0f, 20.0f};
    cfg.co2 = {800.0f, 1000.0f, 1500.0f};
    cfg.hcho = {30.0f, 60.0f, 100.0f};
    cfg.co = {9.0f, 35.0f, 100.0f};
    cfg.voc = {150.0f, 250.0f, 350.0f};
    cfg.nox = {50.0f, 100.0f, 200.0f};
    cfg.background_alerts = {};
    return cfg;
}

bool validate(const Config &cfg, String *error) {
    if (cfg.version != kVersion) {
        set_error(error, "Unsupported display thresholds version");
        return false;
    }
    return validate_range(cfg.temp, "temp", true, error) &&
           validate_range(cfg.rh, "rh", false, error) &&
           validate_range(cfg.dew_point, "dew_point", true, error) &&
           validate_range(cfg.ah, "ah", false, error) &&
           validate_high(cfg.co2, "co2", error) &&
           validate_high(cfg.hcho, "hcho", error) &&
           validate_high(cfg.co, "co", error) &&
           validate_high(cfg.voc, "voc", error) &&
           validate_high(cfg.nox, "nox", error);
}

Band classifyHigh(float value, const High &thresholds) {
    if (!finite(value) || value < 0.0f) return Band::Invalid;
    if (value <= thresholds.green) return Band::Green;
    if (value <= thresholds.yellow) return Band::Yellow;
    if (value <= thresholds.orange) return Band::Orange;
    return Band::Red;
}

Band classifyRange(float value, const Range &thresholds) {
    if (!finite(value)) return Band::Invalid;
    if (value < thresholds.orange_min || value > thresholds.orange_max) return Band::Red;
    if (value < thresholds.yellow_min || value > thresholds.yellow_max) return Band::Orange;
    if (value < thresholds.good_min || value > thresholds.good_max) return Band::Yellow;
    return Band::Green;
}

bool deserialize(const String &json, Config &out, String *error) {
    ArduinoJson::JsonDocument doc;
    const ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, json);
    if (err) {
        set_error(error, "Invalid JSON");
        return false;
    }
    return applyUpdateJson(doc.as<ArduinoJson::JsonVariantConst>(), defaults(), out, error);
}

String serialize(const Config &cfg) {
    ArduinoJson::JsonDocument doc;
    writeJson(doc.to<ArduinoJson::JsonObject>(), cfg);
    String out;
    serializeJson(doc, out);
    return out;
}

void writeMetricsJson(ArduinoJson::JsonObject root, const Config &cfg) {
    write_range(root["temp"].to<ArduinoJson::JsonObject>(), cfg.temp);
    write_range(root["rh"].to<ArduinoJson::JsonObject>(), cfg.rh);
    write_range(root["dew_point"].to<ArduinoJson::JsonObject>(), cfg.dew_point);
    write_range(root["ah"].to<ArduinoJson::JsonObject>(), cfg.ah);
    write_high(root["co2"].to<ArduinoJson::JsonObject>(), cfg.co2);
    write_high(root["hcho"].to<ArduinoJson::JsonObject>(), cfg.hcho);
    write_high(root["co"].to<ArduinoJson::JsonObject>(), cfg.co);
    write_high(root["voc"].to<ArduinoJson::JsonObject>(), cfg.voc);
    write_high(root["nox"].to<ArduinoJson::JsonObject>(), cfg.nox);
}

void writeBackgroundAlertsJson(ArduinoJson::JsonObject root, const BackgroundAlerts &alerts) {
    root["hcho_enabled"] = alerts.hcho_enabled;
    root["co_enabled"] = alerts.co_enabled;
    root["co2_enabled"] = alerts.co2_enabled;
}

void writeJson(ArduinoJson::JsonObject root, const Config &cfg) {
    root["version"] = kVersion;
    writeMetricsJson(root["metrics"].to<ArduinoJson::JsonObject>(), cfg);
    writeBackgroundAlertsJson(root["background_alerts"].to<ArduinoJson::JsonObject>(), cfg.background_alerts);
}

bool applyUpdateJson(ArduinoJson::JsonVariantConst root,
                     const Config &current,
                     Config &out,
                     String *error) {
    if (!root.is<ArduinoJson::JsonObjectConst>()) {
        set_error(error, "Expected JSON object");
        return false;
    }

    Config parsed = current;
    parsed.version = kVersion;

    ArduinoJson::JsonObjectConst obj = root.as<ArduinoJson::JsonObjectConst>();
    ArduinoJson::JsonVariantConst version = obj["version"];
    if (!version.isNull() && !version.is<int>()) {
        set_error(error, "version must be numeric");
        return false;
    }
    if (version.is<int>() && version.as<int>() != kVersion) {
        set_error(error, "Unsupported display thresholds version");
        return false;
    }

    ArduinoJson::JsonObjectConst metrics;
    ArduinoJson::JsonObjectConst background_alerts;
    if (!read_object_field(obj, "metrics", metrics, error) ||
        !read_object_field(obj, "background_alerts", background_alerts, error)) {
        return false;
    }

    if (!apply_metrics_update(metrics, parsed, error)) {
        return false;
    }
    if (!apply_background_update(background_alerts, parsed.background_alerts, error)) {
        return false;
    }

    if (!validate(parsed, error)) {
        return false;
    }

    out = parsed;
    return true;
}

}  // namespace DisplayThresholds

DisplayThresholdManager::DisplayThresholdManager() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
#endif
    cfg_ = DisplayThresholds::defaults();
}

void DisplayThresholdManager::lock() const {
#ifdef UNIT_TEST
    mutex_.lock();
#else
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
#endif
}

void DisplayThresholdManager::unlock() const {
#ifdef UNIT_TEST
    mutex_.unlock();
#else
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}

void DisplayThresholdManager::setDefaultsLocked() {
    cfg_ = DisplayThresholds::defaults();
    ++revision_;
}

void DisplayThresholdManager::begin(StorageManager &storage) {
    storage_ = &storage;
    String json;
    DisplayThresholds::Config loaded{};
    String error;
    if (storage.loadText(StorageManager::kDisplayThresholdsPath, json) &&
        DisplayThresholds::deserialize(json, loaded, &error)) {
        lock();
        cfg_ = loaded;
        ++revision_;
        unlock();
        Logger::log(Logger::Info, "Thresholds", "display thresholds loaded");
        return;
    }

    lock();
    setDefaultsLocked();
    unlock();
    if (json.length() > 0) {
        Logger::log(Logger::Warn, "Thresholds", "display thresholds invalid, using defaults");
    } else {
        Logger::log(Logger::Info, "Thresholds", "display thresholds not found, using defaults");
    }
}

DisplayThresholds::Config DisplayThresholdManager::snapshot() const {
    lock();
    DisplayThresholds::Config copy = cfg_;
    unlock();
    return copy;
}

uint32_t DisplayThresholdManager::revision() const {
    lock();
    const uint32_t rev = revision_;
    unlock();
    return rev;
}

bool DisplayThresholdManager::apply(const DisplayThresholds::Config &cfg, String *error) {
    if (!storage_) {
        if (error) *error = "Storage unavailable";
        return false;
    }
    if (!DisplayThresholds::validate(cfg, error)) {
        return false;
    }
    const String json = DisplayThresholds::serialize(cfg);
    if (!storage_->saveTextAtomic(StorageManager::kDisplayThresholdsPath, json)) {
        if (error) *error = "Failed to save display thresholds";
        return false;
    }
    lock();
    cfg_ = cfg;
    ++revision_;
    unlock();
    return true;
}

bool DisplayThresholdManager::reset(String *error) {
    return apply(DisplayThresholds::defaults(), error);
}
