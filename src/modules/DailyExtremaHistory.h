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
#include "modules/ChartsHistory.h"
#include "modules/DailyHistoryStorage.h"

class DailyExtremaHistory {
public:
    static constexpr const char *kDailyCsvPath = "/aura/history/daily.csv";
    static constexpr const char *kStatePath = "/aura/history/current_day.bin";
    static constexpr uint32_t kStateSaveIntervalMs = 15UL * 60UL * 1000UL;

    void begin(DailyHistoryStorage &storage);
    void update(const SensorData &data, uint32_t now_ms);
    void poll(uint32_t now_ms);
    void flush();

    bool hasCurrentDay() const { return state_.day_key != 0; }
    uint32_t currentDayKey() const { return state_.day_key; }
    uint32_t currentSampleCount() const;
    bool lastWriteOk() const { return last_write_ok_; }

    using NowEpochFn = time_t (*)();
    static void setNowEpochFn(NowEpochFn fn);

    struct MetricDef {
        ChartsHistory::Metric metric;
        const char *key;
        const char *unit;
        uint8_t decimals;
    };

private:
    struct MetricState {
        uint8_t valid = 0;
        uint8_t reserved[3] = {};
        float min_value = 0.0f;
        float max_value = 0.0f;
        uint32_t min_epoch = 0;
        uint32_t max_epoch = 0;
        uint32_t sample_count = 0;
    };

    struct PersistedState {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t metric_count = 0;
        uint32_t day_key = 0;
        uint8_t optional_gas_type = 0;
        uint8_t reserved[3] = {};
        MetricState metrics[ChartsHistory::kMetricCount] = {};
    };

    static time_t nowEpochRaw();
    static bool localDateFromEpoch(time_t epoch, uint32_t &day_key);
    static void formatDay(uint32_t day_key, char *out, size_t len);
    static void formatTime(uint32_t epoch, char *out, size_t len);
    static bool validPersistedState(const PersistedState &state);
    static const MetricDef &metricDef(uint8_t index);

    void ensureDay(uint32_t day_key);
    void resetForDay(uint32_t day_key);
    void resetMetric(ChartsHistory::Metric metric);
    void restoreOrFinalizeStoredDay(uint32_t current_day_key);
    void updateMetric(ChartsHistory::Metric metric, bool valid, float value, uint32_t epoch);
    void updateOptionalGasMetric(const SensorData &data, uint32_t epoch);
    bool hasAnySamples() const;
    bool appendCurrentDayCsv();
    bool ensureCsvHeader();
    void saveStateIfDue(uint32_t now_ms, bool force);

    static NowEpochFn now_epoch_fn_;

    DailyHistoryStorage *storage_ = nullptr;
    PersistedState state_{};
    bool restored_ = false;
    bool dirty_ = false;
    bool last_write_ok_ = true;
    uint32_t last_save_ms_ = 0;
};
