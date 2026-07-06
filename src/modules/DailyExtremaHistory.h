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

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

class DailyExtremaHistory {
public:
    static constexpr const char *kDailyCsvPath = "/aura/history/daily.csv";
    static constexpr const char *kStatePath = "/aura/history/current_day.bin";
    static constexpr uint32_t kStateSaveIntervalMs = 15UL * 60UL * 1000UL;

    DailyExtremaHistory();
    ~DailyExtremaHistory();

    void begin(DailyHistoryStorage &storage, bool initial_units_c);
    void setPreferredUnitsC(bool units_c);
    void update(const SensorData &data, uint32_t now_ms);
    void poll(uint32_t now_ms);
    void flush();

    bool hasCurrentDay() const;
    uint32_t currentDayKey() const;
    uint32_t currentSampleCount() const;
    bool lastWriteOk() const;
    bool currentDayUnitsC() const;
    bool preferredUnitsC() const;
    bool currentDayCsv(String &out, bool include_header = true) const;
    void clearCurrentDay();

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
        uint8_t units_c = 1;
        uint8_t reserved[2] = {};
        MetricState metrics[ChartsHistory::kMetricCount] = {};
    };

    static time_t nowEpochRaw();
    static bool localDateFromEpoch(time_t epoch, uint32_t &day_key);
    static void formatDay(uint32_t day_key, char *out, size_t len);
    static void formatTime(uint32_t epoch, char *out, size_t len);
    static bool validPersistedState(const PersistedState &state);
    static void migratePersistedState(PersistedState &state);
    static const MetricDef &metricDef(uint8_t index);

    class ScopedLock {
    public:
        explicit ScopedLock(const DailyExtremaHistory &owner, uint32_t timeout_ms = 1000);
        ~ScopedLock();
        ScopedLock(const ScopedLock &) = delete;
        ScopedLock &operator=(const ScopedLock &) = delete;
        bool locked() const { return locked_; }

    private:
        const DailyExtremaHistory &owner_;
        bool locked_ = false;
    };

    bool lock(uint32_t timeout_ms = 1000) const;
    void unlock() const;
    uint32_t currentSampleCountLocked() const;
    bool ensureDay(uint32_t day_key);
    void resetForDay(uint32_t day_key);
    void resetMetric(ChartsHistory::Metric metric);
    bool restoreOrFinalizeStoredDay(uint32_t current_day_key);
    void updateMetric(ChartsHistory::Metric metric, bool valid, float value, uint32_t epoch);
    void updateOptionalGasMetric(const SensorData &data, uint32_t epoch);
    bool hasAnySamples() const;
    bool appendCsvRows(String &rows, bool include_header) const;
    bool finalizeCurrentDayCsv();
    bool appendCurrentDayCsv();
    bool ensureCsvHeader();
    void saveStateIfDue(uint32_t now_ms, bool force, bool preserve_failure = false);

    static NowEpochFn now_epoch_fn_;

    DailyHistoryStorage *storage_ = nullptr;
    PersistedState state_{};
    bool restored_ = false;
    bool dirty_ = false;
    bool last_write_ok_ = true;
    bool preferred_units_c_ = true;
    uint32_t last_save_ms_ = 0;

#ifndef UNIT_TEST
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
};
