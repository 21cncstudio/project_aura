// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include "modules/StorageManager.h"

namespace DisplayThresholds {

constexpr uint8_t kVersion = 1;

struct High {
    float green = 0.0f;
    float yellow = 0.0f;
    float orange = 0.0f;
};

struct Range {
    float orange_min = 0.0f;
    float yellow_min = 0.0f;
    float good_min = 0.0f;
    float good_max = 0.0f;
    float yellow_max = 0.0f;
    float orange_max = 0.0f;
};

struct BackgroundAlerts {
    bool hcho_enabled = true;
    bool co_enabled = true;
    bool co2_enabled = true;
};

struct Config {
    uint8_t version = kVersion;
    Range temp{};
    Range rh{};
    Range dew_point{};
    Range ah{};
    High co2{};
    High hcho{};
    High co{};
    High voc{};
    High nox{};
    BackgroundAlerts background_alerts{};
};

enum class Band : uint8_t {
    Invalid = 0,
    Green,
    Yellow,
    Orange,
    Red,
};

Config defaults();
bool validate(const Config &cfg, String *error = nullptr);
Band classifyHigh(float value, const High &thresholds);
Band classifyRange(float value, const Range &thresholds);
bool deserialize(const String &json, Config &out, String *error = nullptr);
String serialize(const Config &cfg);
void writeJson(ArduinoJson::JsonObject root, const Config &cfg);
void writeMetricsJson(ArduinoJson::JsonObject root, const Config &cfg);
void writeBackgroundAlertsJson(ArduinoJson::JsonObject root, const BackgroundAlerts &alerts);
bool applyUpdateJson(ArduinoJson::JsonVariantConst root,
                     const Config &current,
                     Config &out,
                     String *error = nullptr);

}  // namespace DisplayThresholds

class DisplayThresholdManager {
public:
    DisplayThresholdManager();

    void begin(StorageManager &storage);
    DisplayThresholds::Config snapshot() const;
    uint32_t revision() const;
    bool apply(const DisplayThresholds::Config &cfg, String *error = nullptr);
    bool reset(String *error = nullptr);

private:
    void lock() const;
    void unlock() const;
    void setDefaultsLocked();

    StorageManager *storage_ = nullptr;
#ifdef UNIT_TEST
    mutable std::mutex mutex_;
#else
    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
    DisplayThresholds::Config cfg_{};
    uint32_t revision_ = 0;
};
