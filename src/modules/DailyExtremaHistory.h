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
    static constexpr const char *kLegacyStatePath = "/aura/history/current_day.bin";
    static constexpr const char *kStatePathA = "/aura/history/current_day.a.bin";
    static constexpr const char *kStatePathB = "/aura/history/current_day.b.bin";
    static constexpr const char *kStatePath = kLegacyStatePath;
    static constexpr uint32_t kStateSaveIntervalMs = 15UL * 60UL * 1000UL;
    static constexpr uint32_t kStateSaveRetryIntervalMs = 5000UL;
    static constexpr uint8_t kMaxPendingDays = 7;

    DailyExtremaHistory();
    ~DailyExtremaHistory();

    void begin(DailyHistoryStorage &storage, bool initial_units_c);
    void setPreferredUnitsC(bool units_c);
    void update(const SensorData &data,
                uint32_t now_ms,
                bool system_time_trusted);
#ifdef UNIT_TEST
    void update(const SensorData &data, uint32_t now_ms) {
        update(data, now_ms, true);
    }
#endif
    void poll(uint32_t now_ms);
    void flush();

    bool hasCurrentDay() const;
    uint32_t currentDayKey() const;
    uint32_t currentSampleCount() const;
    bool lastWriteOk() const;
    bool currentDayUnitsC() const;
    bool preferredUnitsC() const;
    uint8_t pendingDayCount() const;
    uint32_t oldestPendingDayKey() const;
    uint32_t droppedPendingDayCount() const;
    uint32_t pendingRetryRemainingMs(uint32_t now_ms) const;
    bool currentDayCsv(String &out, bool include_header = true) const;
    struct ClearCurrentDayResult {
        bool ok = false;
        bool state_existed = false;
    };
    ClearCurrentDayResult clearCurrentDay(bool remove_state_files = false,
                                          bool clear_pending_days = false);

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

    struct PersistedSnapshot {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t size = 0;
        uint32_t generation = 0;
        uint8_t pending_count = 0;
        uint8_t reserved[3] = {};
        uint32_t dropped_pending_days = 0;
        uint32_t last_update_epoch = 0;
        uint32_t csv_cleanup_day_key = 0;
        PersistedState current{};
        PersistedState pending[kMaxPendingDays] = {};
        uint32_t crc32 = 0;
    };

    struct PersistedSnapshotV1 {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t size = 0;
        uint32_t generation = 0;
        uint8_t pending_count = 0;
        uint8_t reserved[3] = {};
        uint32_t dropped_pending_days = 0;
        PersistedState current{};
        PersistedState pending[kMaxPendingDays] = {};
        uint32_t crc32 = 0;
    };

    enum class StoredReadResult : uint8_t {
        Missing = 0,
        Valid,
        Invalid,
        RetryableError,
    };

    static_assert(sizeof(MetricState) == 24U,
                  "Daily metric-state ABI changed; bump the format version");
    static_assert(sizeof(PersistedState) == 352U &&
                      offsetof(PersistedState, metrics) == 16U,
                  "Daily persisted-state ABI changed; bump the format version");
    static_assert(sizeof(PersistedSnapshotV1) == 2840U &&
                      offsetof(PersistedSnapshotV1, current) == 20U &&
                      offsetof(PersistedSnapshotV1, crc32) == 2836U,
                  "Daily snapshot v1 disk ABI changed");
    static_assert(sizeof(PersistedSnapshot) == 2848U &&
                      offsetof(PersistedSnapshot, last_update_epoch) == 20U &&
                      offsetof(PersistedSnapshot, csv_cleanup_day_key) == 24U &&
                      offsetof(PersistedSnapshot, current) == 28U &&
                      offsetof(PersistedSnapshot, crc32) == 2844U,
                  "Daily snapshot v2 disk ABI changed");

    static time_t nowEpochRaw();
    static bool localDateFromEpoch(time_t epoch, uint32_t &day_key);
    static void formatDay(uint32_t day_key, char *out, size_t len);
    static void formatTime(uint32_t epoch, char *out, size_t len);
    static bool validPersistedState(const PersistedState &state);
    static void migratePersistedState(PersistedState &state);
    static uint32_t snapshotCrc32(const PersistedSnapshot &snapshot);
    static bool validSnapshot(const PersistedSnapshot &snapshot);
    static bool validSnapshotV1Bytes(const void *data, size_t size);
    static bool generationNewer(uint32_t lhs, uint32_t rhs);
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
    bool ensureDay(uint32_t day_key, time_t current_epoch);
    bool shouldHoldBackwardDay(time_t current_epoch,
                               uint32_t current_day_key,
                               uint32_t stored_day_key) const;
    void discardPendingDaysAtOrAfter(uint32_t day_key);
    void resetForTemporalGeneration(uint32_t day_key);
    bool cleanupFutureCsvIfNeeded(uint32_t now_ms, bool force);
    void resetForDay(uint32_t day_key);
    void resetMetric(ChartsHistory::Metric metric);
    bool restoreStoredState(uint32_t current_day_key, time_t current_epoch);
    StoredReadResult readSnapshotFile(const char *path,
                                      PersistedSnapshot &snapshot) const;
    StoredReadResult readLegacyState(PersistedState &state) const;
    bool enqueuePendingDay(const PersistedState &state);
    void removeOldestPendingDay();
    bool removeAllStateFiles(bool &existed);
    void buildSnapshot(PersistedSnapshot &snapshot, uint32_t generation) const;
    void updateMetric(ChartsHistory::Metric metric, bool valid, float value, uint32_t epoch);
    void updateOptionalGasMetric(const SensorData &data, uint32_t epoch);
    static bool hasAnySamples(const PersistedState &state);
    bool appendCsvRowsForState(const PersistedState &state, String &rows, bool include_header) const;
    bool flushOldestPendingDay(uint32_t now_ms, bool force);
    static uint32_t pendingRetryDelayMs(uint8_t failure_count);
    void saveStateIfDue(uint32_t now_ms, bool force, bool preserve_failure = false);

    static NowEpochFn now_epoch_fn_;

    DailyHistoryStorage *storage_ = nullptr;
    PersistedState state_{};
    PersistedState pending_days_[kMaxPendingDays] = {};
    uint8_t pending_count_ = 0;
    uint32_t state_generation_ = 0;
    uint32_t dropped_pending_days_ = 0;
    bool restored_ = false;
    bool backward_day_hold_ = false;
    time_t last_update_epoch_ = 0;
    uint32_t csv_cleanup_day_key_ = 0;
    uint32_t last_csv_cleanup_attempt_ms_ = 0;
    bool csv_cleanup_retry_pending_ = false;
    bool csv_cleanup_clear_pending_ = false;
    bool csv_cleanup_intent_durable_ = false;
    bool dirty_ = false;
    bool last_write_ok_ = true;
    bool preferred_units_c_ = true;
    uint32_t last_save_ms_ = 0;
    uint32_t last_save_attempt_ms_ = 0;
    bool save_retry_pending_ = false;
    uint32_t last_pending_attempt_ms_ = 0;
    uint32_t pending_retry_delay_ms_ = 0;
    uint8_t pending_failure_count_ = 0;

#ifndef UNIT_TEST
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
};
