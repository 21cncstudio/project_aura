// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/DailyExtremaHistory.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/Logger.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace {

constexpr uint32_t kDailyExtremaMagic = 0x44455848; // "DEXH"
constexpr uint16_t kDailyExtremaVersion = 1;

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

void DailyExtremaHistory::begin(DailyHistoryStorage &storage) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    storage_ = &storage;
    memset(&state_, 0, sizeof(state_));
    restored_ = false;
    dirty_ = false;
    last_write_ok_ = true;
    last_save_ms_ = 0;
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
    return state.magic == kDailyExtremaMagic &&
           state.version == kDailyExtremaVersion &&
           state.metric_count == ChartsHistory::kMetricCount &&
           state.day_key >= 20200101U;
}

const DailyExtremaHistory::MetricDef &DailyExtremaHistory::metricDef(uint8_t index) {
    return kMetricDefs[index];
}

bool DailyExtremaHistory::restoreOrFinalizeStoredDay(uint32_t current_day_key) {
    restored_ = true;
    if (!storage_ || !storage_->isReady()) {
        return true;
    }

    PersistedState stored{};
    size_t read_len = 0;
    if (!storage_->readBinary(kStatePath, &stored, sizeof(stored), read_len) ||
        read_len != sizeof(stored) ||
        !validPersistedState(stored)) {
        return true;
    }

    state_ = stored;
    if (state_.day_key == current_day_key) {
        LOGI("DailyHistory", "restored day=%u samples=%u",
             static_cast<unsigned>(state_.day_key),
             static_cast<unsigned>(currentSampleCountLocked()));
        return true;
    }

    LOGI("DailyHistory", "finalizing stored day=%u before current day=%u",
         static_cast<unsigned>(state_.day_key),
         static_cast<unsigned>(current_day_key));
    if (!finalizeCurrentDayCsv()) {
        return false;
    }
    if (!storage_->removeFile(kStatePath)) {
        LOGW("DailyHistory", "failed to remove finalized current day state");
    }
    memset(&state_, 0, sizeof(state_));
    dirty_ = false;
    last_save_ms_ = 0;
    return true;
}

bool DailyExtremaHistory::ensureDay(uint32_t day_key) {
    if (!restored_ && !restoreOrFinalizeStoredDay(day_key)) {
        return false;
    }

    if (state_.day_key == 0) {
        resetForDay(day_key);
        return true;
    }

    if (state_.day_key != day_key) {
        if (!finalizeCurrentDayCsv()) {
            return false;
        }
        resetForDay(day_key);
    }
    return state_.day_key == day_key;
}

void DailyExtremaHistory::resetForDay(uint32_t day_key) {
    memset(&state_, 0, sizeof(state_));
    state_.magic = kDailyExtremaMagic;
    state_.version = kDailyExtremaVersion;
    state_.metric_count = ChartsHistory::kMetricCount;
    state_.day_key = day_key;
    dirty_ = true;
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

void DailyExtremaHistory::update(const SensorData &data, uint32_t now_ms) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    uint32_t day_key = 0;
    const time_t now_epoch = now_epoch_fn_();
    if (!localDateFromEpoch(now_epoch, day_key)) {
        return;
    }
    if (!ensureDay(day_key)) {
        saveStateIfDue(now_ms, true, true);
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

    saveStateIfDue(now_ms, false);
}

void DailyExtremaHistory::poll(uint32_t now_ms) {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    saveStateIfDue(now_ms, false);
}

void DailyExtremaHistory::flush() {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    saveStateIfDue(millis(), true);
}

bool DailyExtremaHistory::hasAnySamples() const {
    for (const auto &metric : state_.metrics) {
        if (metric.valid && metric.sample_count > 0) {
            return true;
        }
    }
    return false;
}

bool DailyExtremaHistory::ensureCsvHeader() {
    if (!storage_ || !storage_->isReady()) {
        return false;
    }
    size_t size = 0;
    if (storage_->fileExists(kDailyCsvPath) && storage_->fileSize(kDailyCsvPath, size) && size > 0) {
        return true;
    }
    return storage_->appendText(
        kDailyCsvPath,
        "date,metric,unit,min,min_time,max,max_time,sample_count\n");
}

bool DailyExtremaHistory::appendCsvRows(String &rows, bool include_header) const {
    if (state_.day_key == 0 || !hasAnySamples()) {
        return false;
    }
    if (include_header) {
        rows += "date,metric,unit,min,min_time,max,max_time,sample_count\n";
    }

    char day_buf[16] = {};
    char min_time[16] = {};
    char max_time[16] = {};
    formatDay(state_.day_key, day_buf, sizeof(day_buf));

    for (uint8_t i = 0; i < ChartsHistory::kMetricCount; ++i) {
        const MetricState &metric = state_.metrics[i];
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
        rows += def.unit;
        rows += ',';
        rows += format_value(metric.min_value, def.decimals);
        rows += ',';
        rows += min_time;
        rows += ',';
        rows += format_value(metric.max_value, def.decimals);
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
    return appendCsvRows(out, include_header);
}

void DailyExtremaHistory::clearCurrentDay() {
    ScopedLock guard(*this);
    if (!guard.locked()) {
        return;
    }
    memset(&state_, 0, sizeof(state_));
    dirty_ = false;
    restored_ = true;
    last_save_ms_ = 0;
    last_write_ok_ = true;
}

bool DailyExtremaHistory::finalizeCurrentDayCsv() {
    if (state_.day_key == 0 || !hasAnySamples()) {
        return true;
    }
    return appendCurrentDayCsv();
}

bool DailyExtremaHistory::appendCurrentDayCsv() {
    if (!storage_ || !storage_->isReady() || state_.day_key == 0 || !hasAnySamples()) {
        return false;
    }
    if (!ensureCsvHeader()) {
        last_write_ok_ = false;
        return false;
    }

    String rows;
    rows.reserve(1024);
    appendCsvRows(rows, false);

    const bool ok = rows.length() == 0 || storage_->appendText(kDailyCsvPath, rows.c_str());
    last_write_ok_ = ok;
    if (!ok) {
        LOGW("DailyHistory", "failed to append daily CSV for day=%u",
             static_cast<unsigned>(state_.day_key));
        return false;
    }
    LOGI("DailyHistory", "saved daily extrema day=%u samples=%u",
         static_cast<unsigned>(state_.day_key),
         static_cast<unsigned>(currentSampleCountLocked()));
    return true;
}

void DailyExtremaHistory::saveStateIfDue(uint32_t now_ms, bool force, bool preserve_failure) {
    if (!dirty_ || !storage_ || !storage_->isReady() || state_.day_key == 0) {
        return;
    }
    if (!force && last_save_ms_ != 0 && now_ms - last_save_ms_ < kStateSaveIntervalMs) {
        return;
    }
    state_.magic = kDailyExtremaMagic;
    state_.version = kDailyExtremaVersion;
    state_.metric_count = ChartsHistory::kMetricCount;
    last_save_ms_ = now_ms;
    const bool ok = storage_->writeBinaryAtomic(kStatePath, &state_, sizeof(state_));
    if (ok) {
        dirty_ = false;
        if (!preserve_failure) {
            last_write_ok_ = true;
        }
    } else {
        last_write_ok_ = false;
        LOGW("DailyHistory", "failed to save current day state");
    }
}
