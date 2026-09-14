// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/ChartsHistory.h"
#include <algorithm>
#include <math.h>
#include <new>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "modules/StorageManager.h"
#include "core/Logger.h"

namespace {
constexpr uint32_t kMagic = 0x43524849;
constexpr uint32_t kSegmentMagic = 0x43485333;
}
ChartsHistory::NowEpochFn ChartsHistory::now_epoch_fn_ = &ChartsHistory::nowEpochRaw;
time_t ChartsHistory::nowEpochRaw() { return time(nullptr); }
void ChartsHistory::setNowEpochFn(NowEpochFn fn) { now_epoch_fn_ = fn ? fn : &nowEpochRaw; }

bool ChartsHistory::ensureBuffers() {
    if (!entries_) entries_.reset(new (std::nothrow) Entry[kCapacity]());
    if (!segment_) segment_.reset(new (std::nothrow) Segment());
    return entries_ && segment_;
}
uint32_t ChartsHistory::checksum(const Segment &segment) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&segment);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(Segment, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
bool ChartsHistory::validEntry(const Entry &e) {
    if (e.kind > Summary || (e.valid_mask >> kMetricCount) != 0 ||
        (e.kind == Gap && e.valid_mask != 0) ||
        (e.kind != LegacySnapshot && e.start_epoch % kStepSeconds != 0)) return false;
    for (int m = 0; m < kMetricCount; ++m) {
        if (!(e.valid_mask & (1U << m))) continue;
        if (!isfinite(e.values[m])) return false;
        if (e.kind == Summary && (!e.samples[m] || !isfinite(e.minimum[m]) ||
            !isfinite(e.maximum[m]) || e.minimum[m] > e.values[m] ||
            e.maximum[m] < e.values[m] || e.first_second[m] > e.last_second[m] ||
            e.last_second[m] >= kStepSeconds)) return false;
    }
    return true;
}
void ChartsHistory::resetActive() {
    active_ = Entry{};
    memset(sums_, 0, sizeof(sums_));
    active_set_ = false;
}
void ChartsHistory::clear(StorageManager &storage) {
    storage.clearChartsHistory();
    count_ = index_ = 0;
    memset(dirty_, 0, sizeof(dirty_));
    optional_gas_type_ = 0;
    restored_ = false;
    last_epoch_ = 0;
    resetActive();
    ++revision_;
}
void ChartsHistory::append(const Entry &entry) {
    entries_[index_] = entry;
    dirty_[index_] = entry.start_epoch != 0;
    index_ = (index_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
    ++revision_;
}
int ChartsHistory::rawIndexFromOldest(uint16_t offset) const {
    if (!entries_ || offset >= count_) return -1;
    return (index_ + kCapacity - count_ + offset) % kCapacity;
}
uint32_t ChartsHistory::latestEpoch() const {
    const int raw = count_ ? rawIndexFromOldest(count_ - 1) : -1;
    return raw < 0 ? 0 : entries_[raw].start_epoch;
}
bool ChartsHistory::entryFromOldest(uint16_t offset, Entry &out) const {
    const int raw = rawIndexFromOldest(offset);
    if (raw < 0) return false;
    out = entries_[raw];
    out.persisted = out.start_epoch && !dirty_[raw];
    return true;
}
bool ChartsHistory::metricValueFromOldest(uint16_t offset, Metric metric, float &value, bool &valid) const {
    Entry e;
    if (metric >= METRIC_COUNT || !entryFromOldest(offset, e)) return false;
    value = e.values[metric];
    valid = (e.valid_mask & metricBit(metric)) != 0 &&
            (metric != METRIC_OPTIONAL_GAS || e.optional_gas_type == optional_gas_type_);
    return true;
}
void ChartsHistory::load(StorageManager &storage) {
    count_ = index_ = 0;
    last_epoch_ = 0;
    last_tick_ms_ = millis();
    monotonic_ms_ = 0;
    resetActive();
    memset(dirty_, 0, sizeof(dirty_));
    if (!ensureBuffers()) { LOGW("ChartsHistory", "history allocation failed"); return; }

    // Retain legacy snapshots as such. Never invent full-interval statistics.
    std::unique_ptr<LegacyState> legacy(new (std::nothrow) LegacyState());
    if (legacy && storage.loadBlob(StorageManager::kChartsPath, legacy.get(), sizeof(LegacyState)) &&
        legacy->magic == kMagic && legacy->version == 2 && legacy->index < kCapacity &&
        legacy->count <= kCapacity && legacy->epoch > Config::TIME_VALID_EPOCH &&
        legacy->epoch >= (legacy->count ? legacy->count - 1 : 0) * kStepSeconds) {
        for (uint16_t n = 0; n < legacy->count; ++n) {
            const int raw = (legacy->index + kCapacity - legacy->count + n) % kCapacity;
            Entry e{};
            e.start_epoch = legacy->epoch - (legacy->count - 1 - n) * kStepSeconds;
            e.kind = LegacySnapshot;
            e.optional_gas_type = legacy->optional_gas_type;
            for (int m = 0; m < kMetricCount; ++m) if ((legacy->valid_mask[raw] & (1U << m)) &&
                isfinite(legacy->values[m][raw])) {
                e.valid_mask |= 1U << m;
                e.values[m] = legacy->values[m][raw];
            }
            append(e);
        }
    }
    legacy.reset(); // Release the legacy migration buffer before reading v3 blocks.
    // Select the newest 288 entries from bounded hourly blocks. Each block is <4 KiB.
    for (int file = 0; file < StorageManager::kChartsSegmentCount; ++file) {
        char path[32]; StorageManager::chartsSegmentPath(file, path, sizeof(path));
        if (!storage.loadBlob(path, segment_.get(), sizeof(Segment)) || segment_->magic != kSegmentMagic ||
            segment_->version != kSchemaVersion || segment_->checksum != checksum(*segment_) ||
            segment_->hour % StorageManager::kChartsSegmentCount != static_cast<uint32_t>(file)) continue;
        for (int slot = 0; slot < 12; ++slot) {
            if (!(segment_->present & (1U << slot))) continue;
            const Entry &e = segment_->entries[slot];
            if (!validEntry(e) || e.start_epoch / 3600 != segment_->hour ||
                (e.start_epoch % 3600) / kStepSeconds != static_cast<uint32_t>(slot)) continue;
            int existing = -1;
            for (int n = 0; n < count_; ++n) if (entries_[n].start_epoch == e.start_epoch) { existing = n; break; }
            if (existing >= 0) entries_[existing] = e;
            else if (count_ < kCapacity) entries_[count_++] = e;
            else {
                int oldest = 0;
                for (int n = 1; n < count_; ++n) if (entries_[n].start_epoch < entries_[oldest].start_epoch) oldest = n;
                if (e.start_epoch > entries_[oldest].start_epoch) entries_[oldest] = e;
            }
        }
    }
    std::sort(entries_.get(), entries_.get() + count_, [](const Entry &a, const Entry &b) {
        return a.start_epoch < b.start_epoch;
    });
    index_ = count_ % kCapacity;
    memset(dirty_, 0, sizeof(dirty_));
    optional_gas_type_ = count_ ? entries_[count_ - 1].optional_gas_type : 0;
    restored_ = count_ != 0;
    ++revision_;
}
void ChartsHistory::finishInterval() {
    for (int m = 0; m < kMetricCount; ++m) if (active_.samples[m]) {
        active_.valid_mask |= 1U << m;
        active_.values[m] = static_cast<float>(sums_[m] / active_.samples[m]);
    }
    active_.kind = active_.valid_mask ? Summary : Gap;
    if (!count_ || !active_.start_epoch || active_.start_epoch > latestEpoch()) append(active_);
}
void ChartsHistory::flushOneSegment(StorageManager &storage, uint32_t now_ms) {
    if (retry_pending_ && now_ms - last_retry_ms_ < 10000) return;
    int first = -1;
    for (uint16_t n = 0; n < count_; ++n) {
        int raw = rawIndexFromOldest(n);
        if (dirty_[raw]) { first = raw; break; }
    }
    if (first < 0) return;
    const uint32_t hour = entries_[first].start_epoch / 3600;
    memset(segment_.get(), 0, sizeof(Segment));
    segment_->magic = kSegmentMagic; segment_->version = kSchemaVersion; segment_->hour = hour;
    for (uint16_t n = 0; n < count_; ++n) {
        const Entry &e = entries_[rawIndexFromOldest(n)];
        if (!e.start_epoch || e.start_epoch / 3600 != hour) continue;
        const int slot = (e.start_epoch % 3600) / kStepSeconds;
        segment_->entries[slot] = e;
        segment_->present |= 1U << slot;
    }
    segment_->checksum = checksum(*segment_);
    char path[32]; StorageManager::chartsSegmentPath(hour % StorageManager::kChartsSegmentCount, path, sizeof(path));
    last_retry_ms_ = now_ms;
    retry_pending_ = !storage.saveBlobAtomic(path, segment_.get(), sizeof(Segment));
    if (retry_pending_) { LOGW("ChartsHistory", "summary save failed; retry pending"); return; }
    for (uint16_t n = 0; n < count_; ++n) {
        const int raw = rawIndexFromOldest(n);
        if (entries_[raw].start_epoch / 3600 == hour) dirty_[raw] = false;
    }
    ++revision_; // Publish the durable acknowledgement state to readers.
}
void ChartsHistory::update(const SensorData &data, StorageManager &storage, bool gas_warmup,
                           bool system_time_trusted, uint16_t fresh_mask) {
    if (!ensureBuffers()) return;
    const uint32_t now_ms = millis();
    monotonic_ms_ += static_cast<uint32_t>(now_ms - last_tick_ms_);
    last_tick_ms_ = now_ms;
    const time_t epoch = now_epoch_fn_();
    const bool trusted = system_time_trusted && epoch > Config::TIME_VALID_EPOCH &&
                         static_cast<uint64_t>(epoch) <= UINT32_MAX;
    const uint32_t now = trusted ? static_cast<uint32_t>(epoch) : static_cast<uint32_t>(monotonic_ms_ / 1000);
    if (restored_ && !trusted) return;
    if (trusted && last_epoch_ && now < last_epoch_) { resetActive(); return; }
    if (trusted && count_ && latestEpoch() >= now) { resetActive(); return; }
    if (trusted) {
        last_epoch_ = now;
        while (count_) {
            const auto &oldest = entries_[rawIndexFromOldest(0)];
            if (!oldest.start_epoch || oldest.start_epoch + static_cast<uint64_t>(kCapacity) * kStepSeconds >= now - now % kStepSeconds) break;
            dirty_[rawIndexFromOldest(0)] = false;
            --count_;
            ++revision_;
        }
    }
    const uint32_t bucket = now - now % kStepSeconds;
    if (active_set_ && active_trusted_ != trusted) {
        resetActive();
        if (trusted && count_ && latestEpoch() == 0) { count_ = index_ = 0; ++revision_; }
    }
    if (active_set_ && bucket > active_bucket_) {
        finishInterval();
        const uint32_t elapsed = (bucket - active_bucket_) / kStepSeconds;
        const uint32_t gaps = std::min<uint32_t>(elapsed - 1, kCapacity - 1);
        for (uint32_t n = gaps; n > 0; --n) {
            Entry gap{};
            gap.start_epoch = trusted ? bucket - n * kStepSeconds : 0;
            gap.optional_gas_type = optional_gas_type_;
            append(gap);
        }
        resetActive();
    }
    if (!active_set_) {
        // Reboot gaps stay gaps, including pressure. Never interpolate measurements.
        if (trusted && count_ && latestEpoch() && latestEpoch() + 2 * kStepSeconds <= bucket) {
            const uint32_t gaps = std::min<uint32_t>((bucket - latestEpoch()) / kStepSeconds - 1, kCapacity - 1);
            for (uint32_t n = gaps; n > 0; --n) {
                Entry gap{}; gap.start_epoch = bucket - n * kStepSeconds; gap.optional_gas_type = optional_gas_type_;
                append(gap);
            }
        }
        active_set_ = true; active_trusted_ = trusted; active_bucket_ = bucket;
        active_.start_epoch = trusted ? bucket : 0;
        active_.optional_gas_type = optional_gas_type_;
    }
    restored_ = false;
    if (data.optional_gas_sensor_present && data.optional_gas_type && data.optional_gas_type != optional_gas_type_) {
        optional_gas_type_ = data.optional_gas_type;
        // Never combine different gases in the active summary.
        active_.samples[METRIC_OPTIONAL_GAS] = 0;
        sums_[METRIC_OPTIONAL_GAS] = 0;
        active_.optional_gas_type = optional_gas_type_;
        ++revision_;
    }
    Entry reading{};


    if (data.co2_valid) {
        reading.valid_mask |= metricBit(METRIC_CO2);
        reading.values[METRIC_CO2] = static_cast<float>(data.co2);
    }
    if (data.temp_valid) {
        reading.valid_mask |= metricBit(METRIC_TEMPERATURE);
        reading.values[METRIC_TEMPERATURE] = data.temperature;
    }
    if (data.hum_valid) {
        reading.valid_mask |= metricBit(METRIC_HUMIDITY);
        reading.values[METRIC_HUMIDITY] = data.humidity;
    }
    if (data.pressure_valid) {
        reading.valid_mask |= metricBit(METRIC_PRESSURE);
        reading.values[METRIC_PRESSURE] = data.pressure;
    }
    if (data.co_valid && data.co_sensor_present) {
        reading.valid_mask |= metricBit(METRIC_CO);
        reading.values[METRIC_CO] = data.co_ppm;
    }
    if (!gas_warmup && data.voc_valid) {
        reading.valid_mask |= metricBit(METRIC_VOC);
        reading.values[METRIC_VOC] = static_cast<float>(data.voc_index);
    }
    if (!gas_warmup && data.nox_valid) {
        reading.valid_mask |= metricBit(METRIC_NOX);
        reading.values[METRIC_NOX] = static_cast<float>(data.nox_index);
    }
    if (data.hcho_valid) {
        reading.valid_mask |= metricBit(METRIC_HCHO);
        reading.values[METRIC_HCHO] = data.hcho;
    }
    if (data.pm05_valid) {
        reading.valid_mask |= metricBit(METRIC_PM05);
        reading.values[METRIC_PM05] = data.pm05;
    }
    if (data.pm1_valid) {
        reading.valid_mask |= metricBit(METRIC_PM1);
        reading.values[METRIC_PM1] = data.pm1;
    }
    if (data.pm25_valid) {
        reading.valid_mask |= metricBit(METRIC_PM25);
        reading.values[METRIC_PM25] = data.pm25;
    }
    if (data.pm4_valid) {
        reading.valid_mask |= metricBit(METRIC_PM4);
        reading.values[METRIC_PM4] = data.pm4;
    }
    if (data.pm10_valid) {
        reading.valid_mask |= metricBit(METRIC_PM10);
        reading.values[METRIC_PM10] = data.pm10;
    }
    if (data.optional_gas_sensor_present &&
        data.optional_gas_valid &&
        data.optional_gas_type != 0 &&
        isfinite(data.optional_gas_ppm) &&
        data.optional_gas_ppm >= 0.0f) {
        reading.valid_mask |= metricBit(METRIC_OPTIONAL_GAS);
        reading.values[METRIC_OPTIONAL_GAS] = data.optional_gas_ppm;
    }


    for (int m = 0; m < kMetricCount; ++m) {
        if (!(fresh_mask & reading.valid_mask & (1U << m)) || !isfinite(reading.values[m])) continue;
        if (active_.samples[m] == UINT16_MAX) continue;
        const float value = reading.values[m];
        const uint16_t second = now - bucket;
        if (!active_.samples[m]) {
            active_.minimum[m] = active_.maximum[m] = value;
            active_.first_second[m] = second;
        } else {
            active_.minimum[m] = std::min(active_.minimum[m], value);
            active_.maximum[m] = std::max(active_.maximum[m], value);
        }
        sums_[m] += value;
        ++active_.samples[m];
        active_.last_second[m] = second;
    }
    flushOneSegment(storage, now_ms);
}
