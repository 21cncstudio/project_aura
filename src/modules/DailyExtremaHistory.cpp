// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/DailyExtremaHistory.h"

#include <math.h>
#include <memory>
#include <new>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/Logger.h"
#include "drivers/DfrOptionalGasSensor.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

namespace {

constexpr uint32_t kDailyExtremaMagic = 0x44455848; // "DEXH"
constexpr uint16_t kDailyExtremaVersionV1 = 1;
constexpr uint16_t kDailyExtremaVersion = 2;
constexpr uint32_t kDailySnapshotMagic = 0x44585333; // "DXS3"
constexpr uint16_t kDailySnapshotVersionV1 = 1;
constexpr uint16_t kDailySnapshotVersion = 2;
constexpr uint32_t kBackwardDayHoldMaxS =
    Config::CHART_HISTORY_STEP_MS / 1000UL;
constexpr float kHpaToInhg = 0.0295299830714f;
constexpr const char *kDailyCsvHeader =
    "date,metric,unit,min,min_time,max,max_time,sample_count\n";

const DailyExtremaHistory::MetricDef kMetricDefs[] = {
    {ChartsHistory::METRIC_CO2, "co2", "ppm", 0},
    {ChartsHistory::METRIC_TEMPERATURE, "temperature", "C", 1},
    {ChartsHistory::METRIC_HUMIDITY, "humidity", "%", 1},
    {ChartsHistory::METRIC_PRESSURE, "pressure", "hPa", 1},
    {ChartsHistory::METRIC_CO, "co", "ppm", 2},
    {ChartsHistory::METRIC_VOC, "voc", "idx", 0},
    {ChartsHistory::METRIC_NOX, "nox", "idx", 0},
    {ChartsHistory::METRIC_HCHO, "hcho", "ppb", 1},
    {ChartsHistory::METRIC_PM05, "pm05", "#/cm3", 0},
    {ChartsHistory::METRIC_PM1, "pm1", "ug/m3", 1},
    {ChartsHistory::METRIC_PM25, "pm25", "ug/m3", 1},
    {ChartsHistory::METRIC_PM4, "pm4", "ug/m3", 1},
    {ChartsHistory::METRIC_PM10, "pm10", "ug/m3", 1},
    {ChartsHistory::METRIC_OPTIONAL_GAS, "optional_gas", "ppm", 2},
};

static_assert(sizeof(kMetricDefs) / sizeof(kMetricDefs[0]) == ChartsHistory::kMetricCount,
              "Daily extrema metric definitions must match ChartsHistory metrics");

uint32_t crc32_prefix(const void *data, size_t length) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

String format_value(float value, uint8_t decimals) {
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%.*f", static_cast<int>(decimals), static_cast<double>(value));
    return String(buf);
}

String format_uint(uint32_t value) {
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(value));
    return String(buf);
}

float csv_metric_value(ChartsHistory::Metric metric, float value, bool units_c) {
    if (units_c) {
        return value;
    }
    if (metric == ChartsHistory::METRIC_TEMPERATURE) {
        return value * 9.0f / 5.0f + 32.0f;
    }
    if (metric == ChartsHistory::METRIC_PRESSURE) {
        return value * kHpaToInhg;
    }
    return value;
}

const char *csv_metric_unit(const DailyExtremaHistory::MetricDef &def,
                            bool units_c,
                            uint8_t optional_gas_type) {
    if (def.metric == ChartsHistory::METRIC_OPTIONAL_GAS) {
        return DfrOptionalGasSensor::unitForType(
            static_cast<DfrOptionalGasSensor::OptionalGasType>(optional_gas_type));
    }
    if (units_c) {
        return def.unit;
    }
    if (def.metric == ChartsHistory::METRIC_TEMPERATURE) {
        return "F";
    }
    if (def.metric == ChartsHistory::METRIC_PRESSURE) {
        return "inHg";
    }
    return def.unit;
}

uint8_t csv_metric_decimals(const DailyExtremaHistory::MetricDef &def,
                            bool units_c,
                            uint8_t optional_gas_type) {
    if (def.metric == ChartsHistory::METRIC_OPTIONAL_GAS &&
        optional_gas_type == static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O2)) {
        return 1;
    }
    if (!units_c && def.metric == ChartsHistory::METRIC_PRESSURE) {
        return 2;
    }
    return def.decimals;
}

bool localtime_safe(const time_t *epoch, tm *out) {
    if (!epoch || !out) {
        return false;
    }
#if defined(_WIN32)
    return localtime_s(out, epoch) == 0;
#else
    return localtime_r(epoch, out) != nullptr;
#endif
}

} // namespace

DailyExtremaHistory::NowEpochFn DailyExtremaHistory::now_epoch_fn_ =
    &DailyExtremaHistory::nowEpochRaw;

time_t DailyExtremaHistory::nowEpochRaw() {
    return time(nullptr);
}

DailyExtremaHistory::DailyExtremaHistory() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutex();
#endif
}

DailyExtremaHistory::~DailyExtremaHistory() {
#ifndef UNIT_TEST
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
#endif
}

DailyExtremaHistory::ScopedLock::ScopedLock(const DailyExtremaHistory &owner, uint32_t timeout_ms)
    : owner_(owner), locked_(owner.lock(timeout_ms)) {}

DailyExtremaHistory::ScopedLock::~ScopedLock() {
    if (locked_) {
        owner_.unlock();
    }
}

bool DailyExtremaHistory::lock(uint32_t timeout_ms) const {
#ifdef UNIT_TEST
    (void)timeout_ms;
    return true;
#else
    if (!mutex_) {
        return true;
    }
    return xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
#endif
}

void DailyExtremaHistory::unlock() const {
#ifndef UNIT_TEST
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}

void DailyExtremaHistory::setNowEpochFn(NowEpochFn fn) {
    now_epoch_fn_ = fn ? fn : &DailyExtremaHistory::nowEpochRaw;
}

void DailyExtremaHistory::begin(DailyHistoryStorage &storage, bool initial_units_c) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    storage_ = &storage;
    memset(&state_, 0, sizeof(state_));
    memset(pending_days_, 0, sizeof(pending_days_));
    pending_count_ = 0;
    state_generation_ = 0;
    dropped_pending_days_ = 0;
    preferred_units_c_ = initial_units_c;
    restored_ = false;
    backward_day_hold_ = false;
    last_update_epoch_ = 0;
    csv_cleanup_day_key_ = 0;
    last_csv_cleanup_attempt_ms_ = 0;
    csv_cleanup_retry_pending_ = false;
    csv_cleanup_clear_pending_ = false;
    csv_cleanup_intent_durable_ = false;
    dirty_ = false;
    last_write_ok_ = true;
    last_save_ms_ = 0;
    last_save_attempt_ms_ = 0;
    save_retry_pending_ = false;
    last_pending_attempt_ms_ = 0;
    pending_retry_delay_ms_ = 0;
    pending_failure_count_ = 0;
}

void DailyExtremaHistory::setPreferredUnitsC(bool units_c) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    preferred_units_c_ = units_c;
}

bool DailyExtremaHistory::hasCurrentDay() const {
    ScopedLock guard(*this);
    return guard.locked() && state_.day_key != 0;
}

uint32_t DailyExtremaHistory::currentDayKey() const {
    ScopedLock guard(*this);
    return guard.locked() ? state_.day_key : 0;
}

bool DailyExtremaHistory::lastWriteOk() const {
    ScopedLock guard(*this);
    return guard.locked() && last_write_ok_;
}

bool DailyExtremaHistory::currentDayUnitsC() const {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return true;
    }
    return state_.day_key == 0 ? preferred_units_c_ : state_.units_c != 0;
}

bool DailyExtremaHistory::preferredUnitsC() const {
    ScopedLock guard(*this);
    return guard.locked() ? preferred_units_c_ : true;
}

uint8_t DailyExtremaHistory::pendingDayCount() const {
    ScopedLock guard(*this);
    return guard.locked() ? pending_count_ : 0;
}

uint32_t DailyExtremaHistory::oldestPendingDayKey() const {
    ScopedLock guard(*this);
    return guard.locked() && pending_count_ > 0 ? pending_days_[0].day_key : 0;
}

uint32_t DailyExtremaHistory::droppedPendingDayCount() const {
    ScopedLock guard(*this);
    return guard.locked() ? dropped_pending_days_ : 0;
}

uint32_t DailyExtremaHistory::pendingRetryRemainingMs(uint32_t now_ms) const {
    ScopedLock guard(*this);
    if (!guard.locked() || pending_count_ == 0 || pending_retry_delay_ms_ == 0) {
        return 0;
    }
    const uint32_t elapsed = now_ms - last_pending_attempt_ms_;
    return elapsed >= pending_retry_delay_ms_ ? 0 : pending_retry_delay_ms_ - elapsed;
}

uint32_t DailyExtremaHistory::currentSampleCountLocked() const {
    uint32_t total = 0;
    for (const auto &metric : state_.metrics) {
        total += metric.sample_count;
    }
    return total;
}

uint32_t DailyExtremaHistory::currentSampleCount() const {
    ScopedLock guard(*this);
    return guard.locked() ? currentSampleCountLocked() : 0;
}

bool DailyExtremaHistory::localDateFromEpoch(time_t epoch, uint32_t &day_key) {
    if (epoch <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    tm local_tm = {};
    if (!localtime_safe(&epoch, &local_tm)) {
        return false;
    }
    const int year = local_tm.tm_year + 1900;
    const int month = local_tm.tm_mon + 1;
    const int day = local_tm.tm_mday;
    if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    day_key = static_cast<uint32_t>(year * 10000 + month * 100 + day);
    return true;
}

void DailyExtremaHistory::formatDay(uint32_t day_key, char *out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    const uint32_t year = day_key / 10000U;
    const uint32_t month = (day_key / 100U) % 100U;
    const uint32_t day = day_key % 100U;
    snprintf(out, len, "%04u-%02u-%02u",
             static_cast<unsigned>(year),
             static_cast<unsigned>(month),
             static_cast<unsigned>(day));
}

void DailyExtremaHistory::formatTime(uint32_t epoch, char *out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    time_t raw = static_cast<time_t>(epoch);
    tm local_tm = {};
    if (!localtime_safe(&raw, &local_tm)) {
        out[0] = '\0';
        return;
    }
    snprintf(out, len, "%02d:%02d:%02d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
}

bool DailyExtremaHistory::validPersistedState(const PersistedState &state) {
    if (state.magic != kDailyExtremaMagic ||
        (state.version != kDailyExtremaVersionV1 && state.version != kDailyExtremaVersion) ||
        state.metric_count != ChartsHistory::kMetricCount ||
        state.day_key < 20200101U || state.day_key > 20991231U) {
        return false;
    }
    const uint32_t month = (state.day_key / 100U) % 100U;
    const uint32_t day = state.day_key % 100U;
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    for (const MetricState &metric : state.metrics) {
        if (metric.valid > 1) {
            return false;
        }
        if (!metric.valid) {
            continue;
        }
        if (metric.sample_count == 0 || !isfinite(metric.min_value) ||
            !isfinite(metric.max_value) || metric.min_value > metric.max_value) {
            return false;
        }
    }
    return true;
}

void DailyExtremaHistory::migratePersistedState(PersistedState &state) {
    if (state.version == kDailyExtremaVersionV1) {
        state.version = kDailyExtremaVersion;
        state.units_c = 1;
    } else {
        state.units_c = state.units_c ? 1 : 0;
    }
}

uint32_t DailyExtremaHistory::snapshotCrc32(const PersistedSnapshot &snapshot) {
    return crc32_prefix(&snapshot, offsetof(PersistedSnapshot, crc32));
}

bool DailyExtremaHistory::validSnapshot(const PersistedSnapshot &snapshot) {
    const PersistedState empty_state{};
    const bool current_valid =
        snapshot.current.day_key == 0
            ? memcmp(&snapshot.current, &empty_state, sizeof(empty_state)) == 0
            : validPersistedState(snapshot.current);
    if (snapshot.magic != kDailySnapshotMagic ||
        snapshot.version != kDailySnapshotVersion ||
        snapshot.size != sizeof(PersistedSnapshot) ||
        snapshot.pending_count > kMaxPendingDays ||
        (snapshot.last_update_epoch != 0 &&
         snapshot.last_update_epoch <= Config::TIME_VALID_EPOCH) ||
        (snapshot.current.day_key == 0 && snapshot.last_update_epoch != 0) ||
        (snapshot.csv_cleanup_day_key != 0 &&
         (snapshot.csv_cleanup_day_key < 20200101U ||
          snapshot.csv_cleanup_day_key > 20991231U)) ||
        snapshot.crc32 != snapshotCrc32(snapshot) ||
        !current_valid) {
        return false;
    }
    for (uint8_t i = 0; i < snapshot.pending_count; ++i) {
        if (!validPersistedState(snapshot.pending[i])) {
            return false;
        }
    }
    return true;
}

bool DailyExtremaHistory::generationNewer(uint32_t lhs, uint32_t rhs) {
    return static_cast<int32_t>(lhs - rhs) > 0;
}

const DailyExtremaHistory::MetricDef &DailyExtremaHistory::metricDef(uint8_t index) {
    return kMetricDefs[index];
}

DailyExtremaHistory::StoredReadResult
DailyExtremaHistory::readSnapshotFile(const char *path,
                                      PersistedSnapshot &snapshot) const {
    if (!storage_ || !storage_->isReady() || !path) {
        return StoredReadResult::RetryableError;
    }
    bool exists = false;
    size_t size = 0;
    if (!storage_->fileInfo(path, exists, size)) {
        LOGE("DailyHistory", "state snapshot stat failed path=%s", path);
        return StoredReadResult::RetryableError;
    }
    if (!exists) {
        return StoredReadResult::Missing;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    size_t read_len = 0;
    if (size == sizeof(PersistedSnapshot)) {
        if (!storage_->readBinary(path, &snapshot, sizeof(snapshot), read_len) ||
            read_len != sizeof(snapshot)) {
            LOGE("DailyHistory",
                 "state snapshot read failed path=%s size=%u read=%u",
                 path,
                 static_cast<unsigned>(size),
                 static_cast<unsigned>(read_len));
            return StoredReadResult::RetryableError;
        }
        if (validSnapshot(snapshot)) {
            return StoredReadResult::Valid;
        }
    } else if (size == sizeof(PersistedSnapshotV1)) {
        // The v1 record is eight bytes smaller than v2. Read it into the
        // already allocated v2 object representation, validate it as bytes,
        // then shift the state block in place. This avoids a third ~2.8 KiB
        // restore allocation under memory pressure.
        if (!storage_->readBinary(path, &snapshot, size, read_len) ||
            read_len != size) {
            LOGE("DailyHistory",
                 "v1 snapshot read failed path=%s size=%u read=%u",
                 path,
                 static_cast<unsigned>(size),
                 static_cast<unsigned>(read_len));
            return StoredReadResult::RetryableError;
        }
        uint8_t *bytes = reinterpret_cast<uint8_t *>(&snapshot);
        if (validSnapshotV1Bytes(bytes, size)) {
            uint32_t generation = 0;
            uint32_t dropped_pending_days = 0;
            uint8_t pending_count = 0;
            memcpy(&generation,
                   bytes + offsetof(PersistedSnapshotV1, generation),
                   sizeof(generation));
            memcpy(&pending_count,
                   bytes + offsetof(PersistedSnapshotV1, pending_count),
                   sizeof(pending_count));
            memcpy(&dropped_pending_days,
                   bytes + offsetof(PersistedSnapshotV1, dropped_pending_days),
                   sizeof(dropped_pending_days));
            constexpr size_t kStateBlockSize =
                sizeof(PersistedState) * (1U + kMaxPendingDays);
            memmove(bytes + offsetof(PersistedSnapshot, current),
                    bytes + offsetof(PersistedSnapshotV1, current),
                    kStateBlockSize);
            snapshot.magic = kDailySnapshotMagic;
            snapshot.version = kDailySnapshotVersion;
            snapshot.size = sizeof(PersistedSnapshot);
            snapshot.generation = generation;
            snapshot.pending_count = pending_count;
            memset(snapshot.reserved, 0, sizeof(snapshot.reserved));
            snapshot.dropped_pending_days = dropped_pending_days;
            snapshot.last_update_epoch = 0;
            snapshot.csv_cleanup_day_key = 0;
            snapshot.crc32 = snapshotCrc32(snapshot);
            LOGI("DailyHistory", "migrated v1 snapshot path=%s", path);
            return StoredReadResult::Valid;
        }
    }
    LOGW("DailyHistory", "ignored invalid state snapshot path=%s size=%u read=%u",
         path,
         static_cast<unsigned>(size),
         static_cast<unsigned>(read_len));
    return StoredReadResult::Invalid;
}

DailyExtremaHistory::StoredReadResult
DailyExtremaHistory::readLegacyState(PersistedState &state) const {
    if (!storage_ || !storage_->isReady()) {
        return StoredReadResult::RetryableError;
    }
    bool exists = false;
    size_t size = 0;
    if (!storage_->fileInfo(kLegacyStatePath, exists, size)) {
        LOGE("DailyHistory", "legacy state stat failed");
        return StoredReadResult::RetryableError;
    }
    if (!exists) {
        return StoredReadResult::Missing;
    }
    if (size != sizeof(state)) {
        LOGW("DailyHistory", "ignored invalid legacy state size=%u",
             static_cast<unsigned>(size));
        return StoredReadResult::Invalid;
    }
    size_t read_len = 0;
    memset(&state, 0, sizeof(state));
    if (!storage_->readBinary(kLegacyStatePath, &state, sizeof(state), read_len) ||
        read_len != sizeof(state)) {
        LOGE("DailyHistory", "legacy state read failed size=%u",
             static_cast<unsigned>(read_len));
        return StoredReadResult::RetryableError;
    }
    if (!validPersistedState(state)) {
        LOGW("DailyHistory", "ignored invalid legacy state size=%u",
             static_cast<unsigned>(read_len));
        return StoredReadResult::Invalid;
    }
    return StoredReadResult::Valid;
}

bool DailyExtremaHistory::enqueuePendingDay(const PersistedState &state) {
    if (state.day_key == 0 || !hasAnySamples(state)) {
        return true;
    }
    for (uint8_t i = 0; i < pending_count_; ++i) {
        if (pending_days_[i].day_key == state.day_key) {
            pending_days_[i] = state;
            dirty_ = true;
            return true;
        }
    }
    if (pending_count_ >= kMaxPendingDays) {
        const uint32_t dropped_day = pending_days_[0].day_key;
        removeOldestPendingDay();
        ++dropped_pending_days_;
        LOGE("DailyHistory", "pending queue full; dropped oldest day=%u",
             static_cast<unsigned>(dropped_day));
    }
    pending_days_[pending_count_++] = state;
    dirty_ = true;
    LOGI("DailyHistory", "queued day=%u for SD finalization pending=%u",
         static_cast<unsigned>(state.day_key), static_cast<unsigned>(pending_count_));
    return true;
}

void DailyExtremaHistory::removeOldestPendingDay() {
    if (pending_count_ == 0) {
        return;
    }
    for (uint8_t i = 1; i < pending_count_; ++i) {
        pending_days_[i - 1] = pending_days_[i];
    }
    --pending_count_;
    memset(&pending_days_[pending_count_], 0, sizeof(PersistedState));
    dirty_ = true;
}

bool DailyExtremaHistory::restoreStoredState(uint32_t current_day_key,
                                             time_t current_epoch) {
    restored_ = false;
    if (!storage_ || !storage_->isReady()) {
        restored_ = true;
        return true;
    }

    // Two PersistedSnapshot values consume roughly 6 KiB. Keeping both in a
    // loop-task frame can overflow the default Arduino stack while LittleFS is
    // active, so use one contiguous heap scratch allocation.
    std::unique_ptr<PersistedSnapshot[]> snapshots(
        new (std::nothrow) PersistedSnapshot[2]{});
    if (!snapshots) {
        last_write_ok_ = false;
        LOGE("DailyHistory", "snapshot restore scratch allocation failed");
        return false;
    }
    PersistedSnapshot &snapshot_a = snapshots[0];
    PersistedSnapshot &snapshot_b = snapshots[1];
    const StoredReadResult result_a = readSnapshotFile(kStatePathA, snapshot_a);
    const StoredReadResult result_b = readSnapshotFile(kStatePathB, snapshot_b);
    if (result_a == StoredReadResult::RetryableError ||
        result_b == StoredReadResult::RetryableError) {
        last_write_ok_ = false;
        LOGE("DailyHistory",
             "snapshot restore deferred after storage read failure a=%u b=%u",
             static_cast<unsigned>(result_a),
             static_cast<unsigned>(result_b));
        return false;
    }
    const bool valid_a = result_a == StoredReadResult::Valid;
    const bool valid_b = result_b == StoredReadResult::Valid;
    const PersistedSnapshot *selected = nullptr;
    if (valid_a && valid_b) {
        selected = generationNewer(snapshot_b.generation, snapshot_a.generation)
                       ? &snapshot_b
                       : &snapshot_a;
    } else if (valid_a) {
        selected = &snapshot_a;
    } else if (valid_b) {
        selected = &snapshot_b;
    }

    if (selected) {
        state_ = selected->current;
        if (state_.day_key != 0) {
            migratePersistedState(state_);
        }
        pending_count_ = selected->pending_count;
        for (uint8_t i = 0; i < pending_count_; ++i) {
            pending_days_[i] = selected->pending[i];
            migratePersistedState(pending_days_[i]);
        }
        state_generation_ = selected->generation;
        dropped_pending_days_ = selected->dropped_pending_days;
        last_update_epoch_ = static_cast<time_t>(selected->last_update_epoch);
        csv_cleanup_day_key_ = selected->csv_cleanup_day_key;
        csv_cleanup_intent_durable_ = csv_cleanup_day_key_ != 0;
        LOGI("DailyHistory",
             "restored snapshot generation=%u day=%u pending=%u samples=%u anchor=%u cleanup=%u",
             static_cast<unsigned>(state_generation_),
             static_cast<unsigned>(state_.day_key),
             static_cast<unsigned>(pending_count_),
             static_cast<unsigned>(currentSampleCountLocked()),
             static_cast<unsigned>(selected->last_update_epoch),
             static_cast<unsigned>(csv_cleanup_day_key_));
    } else {
        PersistedState legacy{};
        const StoredReadResult legacy_result = readLegacyState(legacy);
        if (legacy_result == StoredReadResult::RetryableError) {
            last_write_ok_ = false;
            LOGE("DailyHistory", "legacy state restore deferred after storage read failure");
            return false;
        }
        if (legacy_result == StoredReadResult::Valid) {
            migratePersistedState(legacy);
            state_ = legacy;
            dirty_ = true;
            LOGI("DailyHistory", "restored legacy day=%u samples=%u",
                 static_cast<unsigned>(state_.day_key),
                 static_cast<unsigned>(currentSampleCountLocked()));
        }
    }

    if (state_.day_key != 0 && state_.day_key < current_day_key) {
        enqueuePendingDay(state_);
        resetForDay(current_day_key);
    } else if (state_.day_key > current_day_key) {
        if (shouldHoldBackwardDay(
                current_epoch, current_day_key, state_.day_key)) {
            backward_day_hold_ = true;
            Logger::log(Logger::Warn,
                        "DailyHistory",
                        "date moved backwards %u -> %u; holding current day until catch-up",
                        static_cast<unsigned>(state_.day_key),
                        static_cast<unsigned>(current_day_key));
        } else {
            Logger::log(Logger::Warn,
                        "DailyHistory",
                        "date moved backwards without a verified <=%us runtime anchor; "
                        "start replacement without exporting future day=%u",
                        static_cast<unsigned>(kBackwardDayHoldMaxS),
                        static_cast<unsigned>(state_.day_key));
            resetForTemporalGeneration(current_day_key);
        }
    }
    restored_ = true;
#ifndef UNIT_TEST
    const UBaseType_t stack_free_units = uxTaskGetStackHighWaterMark(nullptr);
    const uint32_t stack_free_bytes =
        static_cast<uint32_t>(stack_free_units) * sizeof(StackType_t);
    LOGI("DailyHistory",
         "calling task stack minimum free after restore: %u bytes",
         static_cast<unsigned>(stack_free_bytes));
#endif
    return true;
}

bool DailyExtremaHistory::validSnapshotV1Bytes(const void *data, size_t size) {
    if (!data || size != sizeof(PersistedSnapshotV1)) {
        return false;
    }
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t record_size = 0;
    uint8_t pending_count = 0;
    uint32_t stored_crc = 0;
    memcpy(&magic,
           bytes + offsetof(PersistedSnapshotV1, magic),
           sizeof(magic));
    memcpy(&version,
           bytes + offsetof(PersistedSnapshotV1, version),
           sizeof(version));
    memcpy(&record_size,
           bytes + offsetof(PersistedSnapshotV1, size),
           sizeof(record_size));
    memcpy(&pending_count,
           bytes + offsetof(PersistedSnapshotV1, pending_count),
           sizeof(pending_count));
    memcpy(&stored_crc,
           bytes + offsetof(PersistedSnapshotV1, crc32),
           sizeof(stored_crc));

    const PersistedState empty_state{};
    PersistedState candidate{};
    memcpy(&candidate,
           bytes + offsetof(PersistedSnapshotV1, current),
           sizeof(candidate));
    const bool current_valid =
        candidate.day_key == 0
            ? memcmp(&candidate, &empty_state, sizeof(empty_state)) == 0
            : validPersistedState(candidate);
    if (magic != kDailySnapshotMagic ||
        version != kDailySnapshotVersionV1 ||
        record_size != sizeof(PersistedSnapshotV1) ||
        pending_count > kMaxPendingDays ||
        stored_crc != crc32_prefix(bytes, offsetof(PersistedSnapshotV1, crc32)) ||
        !current_valid) {
        return false;
    }
    for (uint8_t i = 0; i < pending_count; ++i) {
        memcpy(&candidate,
               bytes + offsetof(PersistedSnapshotV1, pending) +
                   static_cast<size_t>(i) * sizeof(candidate),
               sizeof(candidate));
        if (!validPersistedState(candidate)) {
            return false;
        }
    }
    return true;
}

bool DailyExtremaHistory::ensureDay(uint32_t day_key, time_t current_epoch) {
    if (!restored_ && !restoreStoredState(day_key, current_epoch)) {
        return false;
    }

    if (state_.day_key == 0) {
        resetForDay(day_key);
        return true;
    }

    if (backward_day_hold_) {
        if (current_epoch < last_update_epoch_) {
            const time_t backward_delta = last_update_epoch_ - current_epoch;
            if (backward_delta <= static_cast<time_t>(kBackwardDayHoldMaxS)) {
                return false;
            }
            Logger::log(Logger::Warn,
                        "DailyHistory",
                        "backward hold exceeded %us; start replacement day=%u",
                        static_cast<unsigned>(kBackwardDayHoldMaxS),
                        static_cast<unsigned>(day_key));
            resetForTemporalGeneration(day_key);
            return true;
        }
        backward_day_hold_ = false;
        LOGI("DailyHistory", "date caught up; daily sampling resumed");
    }

    if (day_key < state_.day_key) {
        if (!shouldHoldBackwardDay(current_epoch, day_key, state_.day_key)) {
            Logger::log(Logger::Warn,
                        "DailyHistory",
                        "date moved backwards without a verified <=%us runtime anchor; "
                        "start replacement without exporting future day=%u",
                        static_cast<unsigned>(kBackwardDayHoldMaxS),
                        static_cast<unsigned>(state_.day_key));
            resetForTemporalGeneration(day_key);
            return true;
        }
        if (!backward_day_hold_) {
            Logger::log(Logger::Warn,
                        "DailyHistory",
                        "date moved backwards %u -> %u; holding current day until catch-up",
                        static_cast<unsigned>(state_.day_key),
                        static_cast<unsigned>(day_key));
        }
        backward_day_hold_ = true;
        return false;
    }

    if (state_.day_key < day_key) {
        enqueuePendingDay(state_);
        resetForDay(day_key);
    }
    return state_.day_key == day_key;
}

bool DailyExtremaHistory::shouldHoldBackwardDay(time_t current_epoch,
                                                uint32_t current_day_key,
                                                uint32_t stored_day_key) const {
    if (stored_day_key <= current_day_key || last_update_epoch_ <= 0 ||
        current_epoch >= last_update_epoch_) {
        return false;
    }
    const time_t backward_delta = last_update_epoch_ - current_epoch;
    if (backward_delta > static_cast<time_t>(kBackwardDayHoldMaxS)) {
        return false;
    }
    uint32_t last_update_day_key = 0;
    return localDateFromEpoch(last_update_epoch_, last_update_day_key) &&
           last_update_day_key == stored_day_key;
}

void DailyExtremaHistory::discardPendingDaysAtOrAfter(uint32_t day_key) {
    uint8_t write_index = 0;
    uint8_t discarded = 0;
    for (uint8_t read_index = 0; read_index < pending_count_; ++read_index) {
        if (pending_days_[read_index].day_key >= day_key) {
            ++discarded;
            continue;
        }
        if (write_index != read_index) {
            pending_days_[write_index] = pending_days_[read_index];
        }
        ++write_index;
    }
    for (uint8_t i = write_index; i < pending_count_; ++i) {
        memset(&pending_days_[i], 0, sizeof(PersistedState));
    }
    pending_count_ = write_index;
    if (discarded > 0) {
        dirty_ = true;
        last_pending_attempt_ms_ = 0;
        pending_retry_delay_ms_ = 0;
        pending_failure_count_ = 0;
        Logger::log(Logger::Warn,
                    "DailyHistory",
                    "discarded %u pending future day(s) for replacement day=%u",
                    static_cast<unsigned>(discarded),
                    static_cast<unsigned>(day_key));
    }
}

void DailyExtremaHistory::resetForTemporalGeneration(uint32_t day_key) {
    if (csv_cleanup_day_key_ == 0 || day_key < csv_cleanup_day_key_) {
        csv_cleanup_day_key_ = day_key;
        csv_cleanup_clear_pending_ = false;
        csv_cleanup_intent_durable_ = false;
        csv_cleanup_retry_pending_ = false;
        last_csv_cleanup_attempt_ms_ = 0;
    }
    discardPendingDaysAtOrAfter(day_key);
    resetForDay(day_key);
}

bool DailyExtremaHistory::cleanupFutureCsvIfNeeded(uint32_t now_ms, bool force) {
    if (csv_cleanup_day_key_ == 0) {
        return true;
    }
    if (!storage_ || !storage_->isReady()) {
        return false;
    }
    if (csv_cleanup_clear_pending_) {
        return true;
    }
    if (!force && csv_cleanup_retry_pending_ &&
        now_ms - last_csv_cleanup_attempt_ms_ < kStateSaveRetryIntervalMs) {
        return false;
    }

    char cutoff_day[16] = {};
    formatDay(csv_cleanup_day_key_, cutoff_day, sizeof(cutoff_day));
    last_csv_cleanup_attempt_ms_ = now_ms;
    if (!storage_->removeDailyCsvRowsOnOrAfterAtomic(kDailyCsvPath, cutoff_day)) {
        csv_cleanup_retry_pending_ = true;
        if (storage_->lastFailureKind() != DailyStorageFailureKind::Busy) {
            last_write_ok_ = false;
        }
        LOGW("DailyHistory",
             "failed to prune daily CSV from day=%u; retry pending",
             static_cast<unsigned>(csv_cleanup_day_key_));
        return false;
    }

    LOGI("DailyHistory",
         "pruned daily CSV from replacement day=%u",
         static_cast<unsigned>(csv_cleanup_day_key_));
    last_csv_cleanup_attempt_ms_ = 0;
    csv_cleanup_retry_pending_ = false;
    csv_cleanup_clear_pending_ = true;
    dirty_ = true;
    if (pending_failure_count_ == 0) {
        last_write_ok_ = true;
    }
    return true;
}

void DailyExtremaHistory::resetForDay(uint32_t day_key) {
    memset(&state_, 0, sizeof(state_));
    state_.magic = kDailyExtremaMagic;
    state_.version = kDailyExtremaVersion;
    state_.metric_count = ChartsHistory::kMetricCount;
    state_.day_key = day_key;
    state_.units_c = preferred_units_c_ ? 1 : 0;
    backward_day_hold_ = false;
    dirty_ = true;
    last_save_ms_ = 0;
    last_save_attempt_ms_ = 0;
    save_retry_pending_ = false;
}

void DailyExtremaHistory::resetMetric(ChartsHistory::Metric metric) {
    if (metric >= ChartsHistory::METRIC_COUNT) {
        return;
    }
    state_.metrics[static_cast<uint8_t>(metric)] = MetricState{};
    dirty_ = true;
}

void DailyExtremaHistory::updateMetric(ChartsHistory::Metric metric,
                                       bool valid,
                                       float value,
                                       uint32_t epoch) {
    if (!valid || !isfinite(value) || metric >= ChartsHistory::METRIC_COUNT) {
        return;
    }

    MetricState &state = state_.metrics[static_cast<uint8_t>(metric)];
    if (!state.valid) {
        state.valid = 1;
        state.min_value = value;
        state.max_value = value;
        state.min_epoch = epoch;
        state.max_epoch = epoch;
    } else {
        if (value < state.min_value) {
            state.min_value = value;
            state.min_epoch = epoch;
        }
        if (value > state.max_value) {
            state.max_value = value;
            state.max_epoch = epoch;
        }
    }
    ++state.sample_count;
    dirty_ = true;
}

void DailyExtremaHistory::updateOptionalGasMetric(const SensorData &data, uint32_t epoch) {
    if (!data.optional_gas_sensor_present || data.optional_gas_type == 0) {
        return;
    }
    if (state_.optional_gas_type != 0 && state_.optional_gas_type != data.optional_gas_type) {
        resetMetric(ChartsHistory::METRIC_OPTIONAL_GAS);
    }
    state_.optional_gas_type = data.optional_gas_type;
    updateMetric(ChartsHistory::METRIC_OPTIONAL_GAS,
                 data.optional_gas_valid && !data.optional_gas_warmup,
                 data.optional_gas_ppm,
                 epoch);
}

void DailyExtremaHistory::update(const SensorData &data,
                                 uint32_t now_ms,
                                 bool system_time_trusted) {
    if (!system_time_trusted) {
        return;
    }
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    uint32_t day_key = 0;
    const time_t now_epoch = now_epoch_fn_();
    if (!localDateFromEpoch(now_epoch, day_key)) {
        return;
    }
    if (!ensureDay(day_key, now_epoch)) {
        if (!backward_day_hold_) {
            saveStateIfDue(now_ms, true, true);
        }
        return;
    }
    const uint32_t epoch = static_cast<uint32_t>(now_epoch);

    updateMetric(ChartsHistory::METRIC_CO2, data.co2_valid, static_cast<float>(data.co2), epoch);
    updateMetric(ChartsHistory::METRIC_TEMPERATURE, data.temp_valid, data.temperature, epoch);
    updateMetric(ChartsHistory::METRIC_HUMIDITY, data.hum_valid, data.humidity, epoch);
    updateMetric(ChartsHistory::METRIC_PRESSURE, data.pressure_valid, data.pressure, epoch);
    updateMetric(ChartsHistory::METRIC_CO,
                 data.co_sensor_present && data.co_valid && !data.co_warmup,
                 data.co_ppm,
                 epoch);
    updateMetric(ChartsHistory::METRIC_VOC, data.voc_valid, static_cast<float>(data.voc_index), epoch);
    updateMetric(ChartsHistory::METRIC_NOX, data.nox_valid, static_cast<float>(data.nox_index), epoch);
    updateMetric(ChartsHistory::METRIC_HCHO,
                 data.hcho_sensor_present && data.hcho_valid && !data.hcho_warmup,
                 data.hcho,
                 epoch);
    updateMetric(ChartsHistory::METRIC_PM05, data.pm05_valid, data.pm05, epoch);
    updateMetric(ChartsHistory::METRIC_PM1, data.pm1_valid, data.pm1, epoch);
    updateMetric(ChartsHistory::METRIC_PM25, data.pm25_valid, data.pm25, epoch);
    updateMetric(ChartsHistory::METRIC_PM4, data.pm4_valid, data.pm4, epoch);
    updateMetric(ChartsHistory::METRIC_PM10, data.pm10_valid, data.pm10, epoch);
    updateOptionalGasMetric(data, epoch);
    last_update_epoch_ = now_epoch;

    const bool cleanup_clear_was_pending = csv_cleanup_clear_pending_;
    saveStateIfDue(now_ms, false);
    if (cleanup_clear_was_pending && csv_cleanup_clear_pending_) {
        return;
    }
    if (csv_cleanup_day_key_ != 0 && !csv_cleanup_intent_durable_) {
        // Persist the cleanup intent before mutating the CSV. Otherwise a
        // reboot could restore the old timeline with no record of the prune.
        return;
    }
    const bool had_csv_cleanup = csv_cleanup_day_key_ != 0;
    if (!cleanupFutureCsvIfNeeded(now_ms, false)) {
        return;
    }
    if (had_csv_cleanup) {
        saveStateIfDue(now_ms, true, false);
        if (csv_cleanup_clear_pending_) {
            // Do not append rows until the cleared watermark is durable. A
            // reboot may otherwise repeat the prune over newly written rows.
            return;
        }
    }
    if (flushOldestPendingDay(now_ms, false)) {
        saveStateIfDue(now_ms, true, false);
    }
}

void DailyExtremaHistory::poll(uint32_t now_ms) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    if (backward_day_hold_) {
        return;
    }
    const bool cleanup_clear_was_pending = csv_cleanup_clear_pending_;
    saveStateIfDue(now_ms, false, pending_failure_count_ > 0);
    if (cleanup_clear_was_pending && csv_cleanup_clear_pending_) {
        return;
    }
    if (csv_cleanup_day_key_ != 0 && !csv_cleanup_intent_durable_) {
        return;
    }
    const bool had_csv_cleanup = csv_cleanup_day_key_ != 0;
    if (!cleanupFutureCsvIfNeeded(now_ms, false)) {
        return;
    }
    if (had_csv_cleanup) {
        saveStateIfDue(now_ms, true, pending_failure_count_ > 0);
        if (csv_cleanup_clear_pending_) {
            return;
        }
    }
    if (flushOldestPendingDay(now_ms, false)) {
        saveStateIfDue(now_ms, true, false);
    }
}

void DailyExtremaHistory::flush() {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    if (backward_day_hold_) {
        return;
    }
    const uint32_t now_ms = millis();
    const bool cleanup_clear_was_pending = csv_cleanup_clear_pending_;
    saveStateIfDue(now_ms, true, pending_failure_count_ > 0);
    if (cleanup_clear_was_pending && csv_cleanup_clear_pending_) {
        return;
    }
    if (csv_cleanup_day_key_ != 0 && !csv_cleanup_intent_durable_) {
        return;
    }
    const bool had_csv_cleanup = csv_cleanup_day_key_ != 0;
    if (!cleanupFutureCsvIfNeeded(now_ms, true)) {
        return;
    }
    if (had_csv_cleanup) {
        saveStateIfDue(now_ms, true, pending_failure_count_ > 0);
        if (csv_cleanup_clear_pending_) {
            return;
        }
    }
    if (flushOldestPendingDay(now_ms, true)) {
        saveStateIfDue(now_ms, true, false);
    }
}

bool DailyExtremaHistory::hasAnySamples(const PersistedState &state) {
    for (const auto &metric : state.metrics) {
        if (metric.valid && metric.sample_count > 0) {
            return true;
        }
    }
    return false;
}

bool DailyExtremaHistory::appendCsvRowsForState(const PersistedState &state,
                                                String &rows,
                                                bool include_header) const {
    if (state.day_key == 0 || !hasAnySamples(state)) {
        return false;
    }
    if (include_header) {
        rows += kDailyCsvHeader;
    }

    char day_buf[16] = {};
    char min_time[16] = {};
    char max_time[16] = {};
    formatDay(state.day_key, day_buf, sizeof(day_buf));

    for (uint8_t i = 0; i < ChartsHistory::kMetricCount; ++i) {
        const MetricState &metric = state.metrics[i];
        if (!metric.valid || metric.sample_count == 0) {
            continue;
        }
        const MetricDef &def = metricDef(i);
        formatTime(metric.min_epoch, min_time, sizeof(min_time));
        formatTime(metric.max_epoch, max_time, sizeof(max_time));

        rows += day_buf;
        rows += ',';
        rows += def.key;
        rows += ',';
        rows += csv_metric_unit(def, state.units_c != 0, state.optional_gas_type);
        rows += ',';
        rows += format_value(csv_metric_value(def.metric, metric.min_value, state.units_c != 0),
                             csv_metric_decimals(def, state.units_c != 0, state.optional_gas_type));
        rows += ',';
        rows += min_time;
        rows += ',';
        rows += format_value(csv_metric_value(def.metric, metric.max_value, state.units_c != 0),
                             csv_metric_decimals(def, state.units_c != 0, state.optional_gas_type));
        rows += ',';
        rows += max_time;
        rows += ',';
        rows += format_uint(metric.sample_count);
        rows += '\n';
    }
    return true;
}

bool DailyExtremaHistory::currentDayCsv(String &out, bool include_header) const {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        out = "";
        return false;
    }
    out = "";
    out.reserve(1152);
    return appendCsvRowsForState(state_, out, include_header);
}

DailyExtremaHistory::ClearCurrentDayResult
DailyExtremaHistory::clearCurrentDay(bool remove_state_files, bool clear_pending_days) {
    ClearCurrentDayResult result{};
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return result;
    }
    const PersistedState previous_state = state_;
    if (csv_cleanup_day_key_ != 0) {
        if (!csv_cleanup_intent_durable_) {
            dirty_ = true;
            saveStateIfDue(millis(), true, true);
            if (!csv_cleanup_intent_durable_) {
                return result;
            }
        }
        if (!cleanupFutureCsvIfNeeded(millis(), true)) {
            return result;
        }
        saveStateIfDue(millis(), true, true);
        if (csv_cleanup_clear_pending_) {
            return result;
        }
    }
    if (remove_state_files) {
        if (!storage_ || !storage_->isReady()) {
            return result;
        }
        if (!removeAllStateFiles(result.state_existed)) {
            return result;
        }
    }
    memset(&state_, 0, sizeof(state_));
    if (clear_pending_days) {
        memset(pending_days_, 0, sizeof(pending_days_));
        pending_count_ = 0;
        dropped_pending_days_ = 0;
    }
    state_generation_ = 0;
    dirty_ = pending_count_ > 0;
    restored_ = true;
    backward_day_hold_ = false;
    last_update_epoch_ = 0;
    csv_cleanup_day_key_ = 0;
    last_csv_cleanup_attempt_ms_ = 0;
    csv_cleanup_retry_pending_ = false;
    csv_cleanup_clear_pending_ = false;
    csv_cleanup_intent_durable_ = false;
    last_save_ms_ = 0;
    last_save_attempt_ms_ = 0;
    save_retry_pending_ = false;
    last_pending_attempt_ms_ = 0;
    pending_retry_delay_ms_ = 0;
    pending_failure_count_ = 0;
    last_write_ok_ = true;
    if (dirty_) {
        saveStateIfDue(millis(), true, false);
        if (dirty_) {
            state_ = previous_state;
            dirty_ = true;
            restored_ = true;
            return result;
        }
    }
    result.ok = true;
    return result;
}

bool DailyExtremaHistory::removeAllStateFiles(bool &existed) {
    existed = false;
    if (!storage_ || !storage_->isReady()) {
        return false;
    }
    const char *paths[] = {kStatePathA, kStatePathB, kLegacyStatePath};
    for (const char *path : paths) {
        bool path_exists = false;
        size_t path_size = 0;
        if (!storage_->fileInfo(path, path_exists, path_size)) {
            return false;
        }
        existed = existed || path_exists;
        if (!storage_->removeFile(path)) {
            return false;
        }
    }
    return true;
}

void DailyExtremaHistory::buildSnapshot(PersistedSnapshot &snapshot,
                                        uint32_t generation) const {
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.magic = kDailySnapshotMagic;
    snapshot.version = kDailySnapshotVersion;
    snapshot.size = sizeof(PersistedSnapshot);
    snapshot.generation = generation;
    snapshot.pending_count = pending_count_;
    snapshot.dropped_pending_days = dropped_pending_days_;
    snapshot.last_update_epoch =
        last_update_epoch_ > 0 ? static_cast<uint32_t>(last_update_epoch_) : 0U;
    snapshot.csv_cleanup_day_key =
        csv_cleanup_clear_pending_ ? 0U : csv_cleanup_day_key_;
    snapshot.current = state_;
    for (uint8_t i = 0; i < pending_count_; ++i) {
        snapshot.pending[i] = pending_days_[i];
    }
    snapshot.crc32 = snapshotCrc32(snapshot);
}

uint32_t DailyExtremaHistory::pendingRetryDelayMs(uint8_t failure_count) {
    if (failure_count <= 1) {
        return 5000UL;
    }
    if (failure_count == 2) {
        return 30000UL;
    }
    if (failure_count == 3) {
        return 60000UL;
    }
    return 5UL * 60UL * 1000UL;
}

bool DailyExtremaHistory::flushOldestPendingDay(uint32_t now_ms, bool force) {
    if (pending_count_ == 0 || !storage_ || !storage_->isReady()) {
        return false;
    }
    if (!force && pending_retry_delay_ms_ > 0 &&
        now_ms - last_pending_attempt_ms_ < pending_retry_delay_ms_) {
        return false;
    }

    const PersistedState pending = pending_days_[0];
    String rows;
    rows.reserve(1024);
    if (!appendCsvRowsForState(pending, rows, false)) {
        removeOldestPendingDay();
        pending_failure_count_ = 0;
        pending_retry_delay_ms_ = 0;
        last_pending_attempt_ms_ = 0;
        return true;
    }

    char day_prefix[16] = {};
    formatDay(pending.day_key, day_prefix, sizeof(day_prefix));
    const size_t prefix_len = strlen(day_prefix);
    if (prefix_len + 1 < sizeof(day_prefix)) {
        day_prefix[prefix_len] = ',';
        day_prefix[prefix_len + 1] = '\0';
    }

    last_pending_attempt_ms_ = now_ms;
    if (!storage_->appendUniqueTextBlockAtomic(
            kDailyCsvPath, day_prefix, kDailyCsvHeader, rows.c_str())) {
        if (pending_failure_count_ < UINT8_MAX) {
            ++pending_failure_count_;
        }
        pending_retry_delay_ms_ = pendingRetryDelayMs(pending_failure_count_);
        if (storage_->lastFailureKind() != DailyStorageFailureKind::Busy) {
            last_write_ok_ = false;
        }
        LOGW("DailyHistory",
             "failed to finalize day=%u pending=%u retry_ms=%u",
             static_cast<unsigned>(pending.day_key),
             static_cast<unsigned>(pending_count_),
             static_cast<unsigned>(pending_retry_delay_ms_));
        return false;
    }

    removeOldestPendingDay();
    pending_failure_count_ = 0;
    pending_retry_delay_ms_ = 0;
    last_pending_attempt_ms_ = 0;
    last_write_ok_ = true;
    uint32_t samples = 0;
    for (const MetricState &metric : pending.metrics) {
        samples += metric.sample_count;
    }
    LOGI("DailyHistory", "saved daily extrema day=%u samples=%u pending=%u",
         static_cast<unsigned>(pending.day_key),
         static_cast<unsigned>(samples),
         static_cast<unsigned>(pending_count_));
    return true;
}

void DailyExtremaHistory::saveStateIfDue(uint32_t now_ms, bool force, bool preserve_failure) {
    if (!dirty_ || !storage_ || !storage_->isReady() ||
        (state_.day_key == 0 && pending_count_ == 0 && csv_cleanup_day_key_ == 0)) {
        return;
    }
    if (!force) {
        if (save_retry_pending_) {
            if (now_ms - last_save_attempt_ms_ < kStateSaveRetryIntervalMs) {
                return;
            }
        } else if (last_save_ms_ != 0 && now_ms - last_save_ms_ < kStateSaveIntervalMs) {
            return;
        }
    }
    if (state_.day_key != 0) {
        state_.magic = kDailyExtremaMagic;
        state_.version = kDailyExtremaVersion;
        state_.metric_count = ChartsHistory::kMetricCount;
    }
    uint32_t next_generation = state_generation_ + 1U;
    if (next_generation == 0) {
        next_generation = 1;
    }
    PersistedSnapshot snapshot{};
    buildSnapshot(snapshot, next_generation);
    const char *target_path = (next_generation & 1U) ? kStatePathA : kStatePathB;
    last_save_attempt_ms_ = now_ms;
    const bool ok = storage_->writeBinaryAtomic(target_path, &snapshot, sizeof(snapshot));
    if (ok) {
        state_generation_ = next_generation;
        dirty_ = false;
        save_retry_pending_ = false;
        last_save_ms_ = now_ms;
        if (csv_cleanup_clear_pending_) {
            csv_cleanup_day_key_ = 0;
            csv_cleanup_clear_pending_ = false;
            csv_cleanup_intent_durable_ = false;
        } else if (csv_cleanup_day_key_ != 0) {
            csv_cleanup_intent_durable_ = true;
        }
        if (!preserve_failure && pending_failure_count_ == 0) {
            last_write_ok_ = true;
        }
        bool legacy_exists = false;
        size_t legacy_size = 0;
        if (storage_->fileInfo(kLegacyStatePath, legacy_exists, legacy_size) && legacy_exists &&
            !storage_->removeFile(kLegacyStatePath)) {
            LOGW("DailyHistory", "failed to remove migrated legacy state");
        }
    } else {
        save_retry_pending_ = true;
        last_write_ok_ = false;
        LOGW("DailyHistory", "failed to save current day state");
    }
}
