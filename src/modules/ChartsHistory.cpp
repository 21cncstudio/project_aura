// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/ChartsHistory.h"

#include <math.h>
#include <string.h>
#include "core/Logger.h"
#include "modules/StorageManager.h"

namespace {

constexpr uint32_t kChartsHistoryMagic = 0x43524849; // "CRHI"
constexpr uint16_t kChartsHistoryVersion = 2;

} // namespace

ChartsHistory::NowEpochFn ChartsHistory::now_epoch_fn_ = &ChartsHistory::nowEpochRaw;

time_t ChartsHistory::nowEpochRaw() {
    return time(nullptr);
}

void ChartsHistory::setNowEpochFn(NowEpochFn fn) {
    now_epoch_fn_ = fn ? fn : &ChartsHistory::nowEpochRaw;
}

bool ChartsHistory::getNowEpoch(uint32_t &now_epoch) const {
    time_t now = now_epoch_fn_();
    if (now <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    now_epoch = static_cast<uint32_t>(now);
    return true;
}

bool ChartsHistory::isStale(uint32_t now_epoch) const {
    if (state_.epoch == 0) {
        return false;
    }
    if (now_epoch < state_.epoch) {
        return true;
    }
    return (now_epoch - state_.epoch) > Config::CHART_HISTORY_MAX_AGE_S;
}

uint8_t ChartsHistory::optionalGasHistoryType(const SensorData &data) const {
    if (!data.optional_gas_sensor_present || data.optional_gas_type == 0) {
        return 0;
    }
    return data.optional_gas_type;
}

void ChartsHistory::clearOptionalGasMetric() {
    const uint16_t mask = static_cast<uint16_t>(~metricBit(METRIC_OPTIONAL_GAS));
    for (int i = 0; i < kCapacity; ++i) {
        state_.valid_mask[i] &= mask;
        state_.values[METRIC_OPTIONAL_GAS][i] = 0.0f;
    }
}

void ChartsHistory::syncOptionalGasHistoryType(uint8_t current_type) {
    if (current_type == 0) {
        return;
    }
    if (state_.optional_gas_type != 0 && state_.optional_gas_type != current_type) {
        Logger::log(Logger::Info,
                    "ChartsHistory",
                    "optional gas type changed %u -> %u, clear metric history",
                    static_cast<unsigned>(state_.optional_gas_type),
                    static_cast<unsigned>(current_type));
        clearOptionalGasMetric();
    }
    state_.optional_gas_type = current_type;
}

void ChartsHistory::reset(StorageManager &storage, bool clear_storage) {
    memset(&state_, 0, sizeof(state_));
    state_.magic = kChartsHistoryMagic;
    state_.version = kChartsHistoryVersion;
    last_sample_ms_ = 0;
    last_save_ms_ = 0;
    first_update_after_load_ = true;
    restored_ = false;
    backward_time_hold_ = false;
    replacement_save_pending_ = false;
    if (clear_storage) {
        storage.removeBlob(StorageManager::kChartsPath);
    }
}

void ChartsHistory::clear(StorageManager &storage) {
    reset(storage, true);
}

void ChartsHistory::load(StorageManager &storage) {
    reset(storage, false);
    if (!storage.loadBlob(StorageManager::kChartsPath, &state_, sizeof(state_))) {
        Logger::log(Logger::Debug, "ChartsHistory", "no stored history");
        return;
    }

    if (state_.magic != kChartsHistoryMagic || state_.version != kChartsHistoryVersion) {
        LOGW("ChartsHistory", "invalid stored history header, reset");
        reset(storage, true);
        return;
    }

    if (state_.index >= kCapacity || state_.count > kCapacity) {
        LOGW("ChartsHistory", "invalid stored index/count, reset");
        reset(storage, true);
        return;
    }

    if (state_.epoch == 0) {
        // Samples recorded without trusted time cannot be placed on an
        // absolute timeline after a reboot. Start a fresh RAM generation, but
        // preserve the old atomic blob until the first replacement save works.
        LOGW("ChartsHistory",
             "stored history has no trusted timestamp; start new generation");
        reset(storage, false);
        replacement_save_pending_ = true;
        return;
    }

    // load() runs before TimeManager validates RTC/NTP. Keep a structurally
    // valid generation quarantined until update() receives trusted time.
    last_sample_ms_ = millis() - Config::CHART_HISTORY_STEP_MS;
    first_update_after_load_ = true;
    restored_ = true;
    Logger::log(Logger::Info, "ChartsHistory",
                "restored count=%u idx=%u epoch=%u; awaiting trusted time",
                static_cast<unsigned>(state_.count),
                static_cast<unsigned>(state_.index),
                static_cast<unsigned>(state_.epoch));
}

void ChartsHistory::saveIfDue(StorageManager &storage, uint32_t now_ms) {
    if (backward_time_hold_) {
        return;
    }
    if (state_.count == 0) {
        return;
    }
    if (!replacement_save_pending_ &&
        now_ms - last_save_ms_ < Config::CHART_HISTORY_SAVE_MS) {
        return;
    }
    state_.magic = kChartsHistoryMagic;
    state_.version = kChartsHistoryVersion;
    if (!storage.saveBlobAtomic(
            StorageManager::kChartsPath, &state_, sizeof(state_))) {
        LOGW("ChartsHistory", "atomic history save failed; previous blob preserved");
        return;
    }
    last_save_ms_ = now_ms;
    replacement_save_pending_ = false;
}

ChartsHistory::Sample ChartsHistory::makeSample(const SensorData &data, bool gas_warmup) const {
    Sample sample = {};

    if (data.co2_valid) {
        sample.valid_mask |= metricBit(METRIC_CO2);
        sample.values[METRIC_CO2] = static_cast<float>(data.co2);
    }
    if (data.temp_valid) {
        sample.valid_mask |= metricBit(METRIC_TEMPERATURE);
        sample.values[METRIC_TEMPERATURE] = data.temperature;
    }
    if (data.hum_valid) {
        sample.valid_mask |= metricBit(METRIC_HUMIDITY);
        sample.values[METRIC_HUMIDITY] = data.humidity;
    }
    if (data.pressure_valid) {
        sample.valid_mask |= metricBit(METRIC_PRESSURE);
        sample.values[METRIC_PRESSURE] = data.pressure;
    }
    if (data.co_valid && data.co_sensor_present) {
        sample.valid_mask |= metricBit(METRIC_CO);
        sample.values[METRIC_CO] = data.co_ppm;
    }
    if (!gas_warmup && data.voc_valid) {
        sample.valid_mask |= metricBit(METRIC_VOC);
        sample.values[METRIC_VOC] = static_cast<float>(data.voc_index);
    }
    if (!gas_warmup && data.nox_valid) {
        sample.valid_mask |= metricBit(METRIC_NOX);
        sample.values[METRIC_NOX] = static_cast<float>(data.nox_index);
    }
    if (data.hcho_valid) {
        sample.valid_mask |= metricBit(METRIC_HCHO);
        sample.values[METRIC_HCHO] = data.hcho;
    }
    if (data.pm05_valid) {
        sample.valid_mask |= metricBit(METRIC_PM05);
        sample.values[METRIC_PM05] = data.pm05;
    }
    if (data.pm1_valid) {
        sample.valid_mask |= metricBit(METRIC_PM1);
        sample.values[METRIC_PM1] = data.pm1;
    }
    if (data.pm25_valid) {
        sample.valid_mask |= metricBit(METRIC_PM25);
        sample.values[METRIC_PM25] = data.pm25;
    }
    if (data.pm4_valid) {
        sample.valid_mask |= metricBit(METRIC_PM4);
        sample.values[METRIC_PM4] = data.pm4;
    }
    if (data.pm10_valid) {
        sample.valid_mask |= metricBit(METRIC_PM10);
        sample.values[METRIC_PM10] = data.pm10;
    }
    if (data.optional_gas_sensor_present &&
        data.optional_gas_valid &&
        data.optional_gas_type != 0 &&
        isfinite(data.optional_gas_ppm) &&
        data.optional_gas_ppm >= 0.0f) {
        sample.valid_mask |= metricBit(METRIC_OPTIONAL_GAS);
        sample.values[METRIC_OPTIONAL_GAS] = data.optional_gas_ppm;
    }

    return sample;
}

void ChartsHistory::appendSample(const Sample &sample) {
    const int idx = state_.index;
    state_.valid_mask[idx] = sample.valid_mask;
    for (int metric = 0; metric < kMetricCount; ++metric) {
        state_.values[metric][idx] = sample.values[metric];
    }

    state_.index = static_cast<uint16_t>((idx + 1) % kCapacity);
    if (state_.count < kCapacity) {
        state_.count++;
    }
}

bool ChartsHistory::metricValidAtRaw(int raw_index, Metric metric) const {
    if (raw_index < 0 || raw_index >= kCapacity) {
        return false;
    }
    return (state_.valid_mask[raw_index] & metricBit(metric)) != 0;
}

void ChartsHistory::appendGapPoints(uint32_t gap_points, const Sample &current_sample) {
    if (gap_points == 0 || state_.count == 0) {
        return;
    }
    if (gap_points > static_cast<uint32_t>(kCapacity - 1)) {
        gap_points = static_cast<uint32_t>(kCapacity - 1);
    }

    const int latest_raw = (state_.index + kCapacity - 1) % kCapacity;
    const bool pressure_start_valid = metricValidAtRaw(latest_raw, METRIC_PRESSURE);
    const bool pressure_end_valid = (current_sample.valid_mask & metricBit(METRIC_PRESSURE)) != 0;
    const float pressure_start = state_.values[METRIC_PRESSURE][latest_raw];
    const float pressure_end = current_sample.values[METRIC_PRESSURE];

    for (uint32_t i = 1; i <= gap_points; ++i) {
        Sample gap = {};
        if (pressure_start_valid && pressure_end_valid &&
            isfinite(pressure_start) && isfinite(pressure_end)) {
            float ratio = static_cast<float>(i) / static_cast<float>(gap_points + 1);
            gap.valid_mask |= metricBit(METRIC_PRESSURE);
            gap.values[METRIC_PRESSURE] =
                pressure_start + (pressure_end - pressure_start) * ratio;
        }
        appendSample(gap);
    }
}

void ChartsHistory::update(const SensorData &data,
                           StorageManager &storage,
                           bool gas_warmup,
                           bool system_time_trusted) {
    const uint32_t now_ms = millis();
    const uint32_t step_ms = Config::CHART_HISTORY_STEP_MS;
    const uint32_t step_s = step_ms / 1000UL;
    const uint8_t current_optional_gas_type = optionalGasHistoryType(data);

    uint32_t now_epoch = 0;
    const bool time_valid = system_time_trusted && getNowEpoch(now_epoch);
    if (backward_time_hold_ && !time_valid) {
        return;
    }
    if (restored_ && !time_valid) {
        // Do not append, rewrite or delete a restored generation based on a
        // merely plausible process epoch.
        return;
    }

    if (time_valid && state_.epoch != 0 && now_epoch < state_.epoch) {
        const uint32_t backward_s = state_.epoch - now_epoch;
        if (backward_s <= step_s) {
            if (!backward_time_hold_) {
                Logger::log(Logger::Warn,
                            "ChartsHistory",
                            "clock moved backwards %us; holding history until catch-up",
                            static_cast<unsigned>(backward_s));
            }
            backward_time_hold_ = true;
            return;
        }
        backward_time_hold_ = false;
    } else if (time_valid && backward_time_hold_) {
        backward_time_hold_ = false;
        LOGI("ChartsHistory", "clock caught up; history sampling resumed");
    }

    if (time_valid && isStale(now_epoch)) {
        LOGW("ChartsHistory", "history stale, reset");
        // Keep the old generation until the first sample of its replacement
        // has been committed atomically.
        reset(storage, false);
        replacement_save_pending_ = true;
        last_sample_ms_ = now_ms - step_ms;
    }
    restored_ = false;

    Sample sample = makeSample(data, gas_warmup);

    if (time_valid && state_.epoch != 0) {
        const uint32_t delta_s = now_epoch - state_.epoch;
        if (delta_s < step_s) {
            if (!first_update_after_load_) {
                return;
            }
        } else {
            const uint32_t steps = delta_s / step_s;
            if (steps > 1) {
                appendGapPoints(steps - 1, sample);
            }
        }
    } else if (now_ms - last_sample_ms_ < step_ms) {
        if (!first_update_after_load_) {
            return;
        }
    }

    syncOptionalGasHistoryType(current_optional_gas_type);

    last_sample_ms_ = now_ms;
    appendSample(sample);
    state_.epoch = time_valid ? now_epoch : 0;
    first_update_after_load_ = false;

    saveIfDue(storage, now_ms);
}

int ChartsHistory::rawIndexFromOldest(uint16_t offset) const {
    if (offset >= state_.count) {
        return -1;
    }
    int oldest = (state_.index + kCapacity - state_.count) % kCapacity;
    return (oldest + offset) % kCapacity;
}

bool ChartsHistory::entryFromOldest(uint16_t offset, Entry &out) const {
    int raw = rawIndexFromOldest(offset);
    if (raw < 0) {
        return false;
    }
    out.valid_mask = state_.valid_mask[raw];
    for (int metric = 0; metric < kMetricCount; ++metric) {
        out.values[metric] = state_.values[metric][raw];
    }
    return true;
}

bool ChartsHistory::metricValueFromOldest(uint16_t offset,
                                          Metric metric,
                                          float &value,
                                          bool &valid) const {
    int raw = rawIndexFromOldest(offset);
    if (raw < 0 || metric >= METRIC_COUNT) {
        return false;
    }
    value = state_.values[metric][raw];
    valid = metricValidAtRaw(raw, metric);
    return true;
}
