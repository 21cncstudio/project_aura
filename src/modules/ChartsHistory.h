// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <memory>
#include <time.h>
#include "config/AppConfig.h"
#include "config/AppData.h"

class StorageManager;

// One acquisition accumulator and retained history for local charts and export.
class ChartsHistory {
public:
    enum Metric : uint8_t {
        METRIC_CO2, METRIC_TEMPERATURE, METRIC_HUMIDITY, METRIC_PRESSURE,
        METRIC_CO, METRIC_VOC, METRIC_NOX, METRIC_HCHO, METRIC_PM05,
        METRIC_PM1, METRIC_PM25, METRIC_PM4, METRIC_PM10, METRIC_OPTIONAL_GAS,
        METRIC_COUNT
    };
    static constexpr int kCapacity = Config::CHART_HISTORY_24H_SAMPLES;
    static constexpr int kMetricCount = METRIC_COUNT;
    static constexpr uint32_t kStepSeconds = 300;
    static constexpr uint16_t kSchemaVersion = 3;
    enum Kind : uint8_t { Gap = 0, LegacySnapshot = 1, Summary = 2 };
    struct Entry {
        uint32_t start_epoch = 0; // UTC interval start; zero means unanchored RAM history.
        uint16_t valid_mask = 0;
        bool persisted = false; // Runtime/export status; recomputed when a block is loaded.
        uint8_t kind = Gap;
        uint8_t optional_gas_type = 0;
        float values[kMetricCount] = {}; // arithmetic mean of valid new acquisitions
        float minimum[kMetricCount] = {};
        float maximum[kMetricCount] = {};
        uint16_t samples[kMetricCount] = {};
        uint16_t first_second[kMetricCount] = {};
        uint16_t last_second[kMetricCount] = {};
    };
    static constexpr uint16_t metricBit(Metric metric) {
        return static_cast<uint16_t>(1U << static_cast<uint8_t>(metric));
    }
    void load(StorageManager &storage);
    // fresh_mask identifies *new acquisitions*, including equal consecutive values.
    void update(const SensorData &data, StorageManager &storage, bool gas_warmup,
                bool system_time_trusted, uint16_t fresh_mask = 0);
    void clear(StorageManager &storage);
    uint16_t count() const { return count_; }
    uint16_t index() const { return index_; }
    uint32_t latestEpoch() const;
    uint8_t optionalGasType() const { return optional_gas_type_; }
    uint32_t revision() const { return revision_; }
    bool entryFromOldest(uint16_t offset, Entry &out) const;
    bool metricValueFromOldest(uint16_t offset, Metric metric, float &value, bool &valid) const;
    using NowEpochFn = time_t (*)();
    static void setNowEpochFn(NowEpochFn fn);

private:
    struct LegacyState {
        uint32_t magic; uint16_t version; uint16_t reserved;
        uint8_t optional_gas_type; uint8_t reserved2;
        uint32_t epoch; uint16_t index; uint16_t count;
        uint16_t valid_mask[kCapacity];
        float values[kMetricCount][kCapacity];
    };
    struct Segment {
        uint32_t magic;
        uint32_t hour;
        uint16_t version;
        uint16_t present;
        Entry entries[12];
        uint32_t checksum;
    };
    static_assert(sizeof(Segment) <= 4096, "History block must remain bounded to 4 KiB");
    static_assert(kCapacity == 288, "Review hourly block retention before changing capacity");
    static_assert(kStepSeconds * 1000 == Config::CHART_HISTORY_STEP_MS, "History step mismatch");
    static time_t nowEpochRaw();
    static NowEpochFn now_epoch_fn_;
    static uint32_t checksum(const Segment &segment);
    static bool validEntry(const Entry &entry);
    bool ensureBuffers();
    void append(const Entry &entry);
    void finishInterval();
    void flushOneSegment(StorageManager &storage, uint32_t now_ms);
    int rawIndexFromOldest(uint16_t offset) const;
    void resetActive();
    std::unique_ptr<Entry[]> entries_;
    std::unique_ptr<Segment> segment_;
    bool dirty_[kCapacity] = {};
    uint16_t count_ = 0, index_ = 0;
    uint8_t optional_gas_type_ = 0;
    uint32_t revision_ = 0;
    Entry active_{};
    double sums_[kMetricCount] = {};
    uint32_t active_bucket_ = 0;
    bool active_set_ = false, active_trusted_ = false, restored_ = false;
    uint32_t last_epoch_ = 0, last_tick_ms_ = 0, last_retry_ms_ = 0;
    uint64_t monotonic_ms_ = 0;
    bool retry_pending_ = false;
};
