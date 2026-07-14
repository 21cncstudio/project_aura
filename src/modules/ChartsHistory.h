// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <time.h>
#include "config/AppConfig.h"
#include "config/AppData.h"

class StorageManager;

class ChartsHistory {
public:
    enum Metric : uint8_t {
        METRIC_CO2 = 0,
        METRIC_TEMPERATURE,
        METRIC_HUMIDITY,
        METRIC_PRESSURE,
        METRIC_CO,
        METRIC_VOC,
        METRIC_NOX,
        METRIC_HCHO,
        METRIC_PM05,
        METRIC_PM1,
        METRIC_PM25,
        METRIC_PM4,
        METRIC_PM10,
        METRIC_OPTIONAL_GAS,
        METRIC_COUNT
    };

    static constexpr int kCapacity = Config::CHART_HISTORY_24H_SAMPLES;
    static constexpr int kMetricCount = static_cast<int>(METRIC_COUNT);

    struct Entry {
        uint16_t valid_mask = 0;
        float values[kMetricCount] = {};
    };

    void load(StorageManager &storage);
    void update(const SensorData &data, StorageManager &storage, bool gas_warmup);
    void clear(StorageManager &storage);

    uint16_t count() const { return state_.count; }
    uint16_t index() const { return state_.index; }
    uint32_t latestEpoch() const { return state_.epoch; }

    bool entryFromOldest(uint16_t offset, Entry &out) const;
    bool metricValueFromOldest(uint16_t offset, Metric metric, float &value, bool &valid) const;

    using NowEpochFn = time_t (*)();
    static void setNowEpochFn(NowEpochFn fn);

private:
    static constexpr uint16_t metricBit(Metric metric) {
        return static_cast<uint16_t>(1U << static_cast<uint8_t>(metric));
    }

    struct PersistedState {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t reserved = 0;
        uint8_t optional_gas_type = 0;
        uint8_t reserved2 = 0;
        uint32_t epoch = 0;
        uint16_t index = 0;
        uint16_t count = 0;
        uint16_t valid_mask[kCapacity] = {};
        float values[kMetricCount][kCapacity] = {};
    };

    struct Sample {
        uint16_t valid_mask = 0;
        float values[kMetricCount] = {};
    };

    static time_t nowEpochRaw();
    bool getNowEpoch(uint32_t &now_epoch) const;
    bool isStale(uint32_t now_epoch) const;
    uint8_t optionalGasHistoryType(const SensorData &data) const;
    void syncOptionalGasHistoryType(uint8_t current_type);
    void clearOptionalGasMetric();
    void reset(StorageManager &storage, bool clear_storage);
    void saveIfDue(StorageManager &storage, uint32_t now_ms);
    Sample makeSample(const SensorData &data, bool gas_warmup) const;
    void appendSample(const Sample &sample);
    void appendGapPoints(uint32_t gap_points, const Sample &current_sample);
    int rawIndexFromOldest(uint16_t offset) const;
    bool metricValidAtRaw(int raw_index, Metric metric) const;

    static NowEpochFn now_epoch_fn_;

    uint32_t last_sample_ms_ = 0;
    uint32_t last_save_ms_ = 0;
    bool first_update_after_load_ = true;
    PersistedState state_{};
};
