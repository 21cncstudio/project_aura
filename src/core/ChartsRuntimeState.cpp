// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/ChartsRuntimeState.h"

#include <math.h>

ChartsRuntimeState::ChartsRuntimeState() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

void ChartsRuntimeState::update(const ChartsHistory &history) {
    const uint16_t source_count = history.count();
    const uint16_t source_index = history.index();
    const uint32_t source_epoch = history.latestEpoch();

    lock();
    const bool unchanged = (count_ == source_count) &&
                           (source_index_ == source_index) &&
                           (latest_epoch_ == source_epoch);
    unlock();
    if (unchanged) {
        return;
    }

    lock();
    count_ = source_count;
    source_index_ = source_index;
    latest_epoch_ = source_epoch;
    for (uint16_t offset = 0; offset < source_count; ++offset) {
        if (!history.entryFromOldest(offset, entries_[offset])) {
            entries_[offset] = ChartsHistory::Entry{};
        }
    }
    for (uint16_t offset = source_count; offset < ChartsHistory::kCapacity; ++offset) {
        entries_[offset] = ChartsHistory::Entry{};
    }
    unlock();
}

uint16_t ChartsRuntimeState::count() const {
    lock();
    const uint16_t value = count_;
    unlock();
    return value;
}

uint32_t ChartsRuntimeState::latestEpoch() const {
    lock();
    const uint32_t value = latest_epoch_;
    unlock();
    return value;
}

bool ChartsRuntimeState::entryFromOldest(uint16_t offset, ChartsHistory::Entry &out) const {
    lock();
    if (offset >= count_) {
        unlock();
        return false;
    }
    out = entries_[offset];
    unlock();
    return true;
}

bool ChartsRuntimeState::metricValueFromOldest(uint16_t offset,
                                               ChartsHistory::Metric metric,
                                               float &value,
                                               bool &valid) const {
    lock();
    if (offset >= count_ || metric >= ChartsHistory::METRIC_COUNT) {
        unlock();
        return false;
    }
    const ChartsHistory::Entry &entry = entries_[offset];
    value = entry.values[metric];
    valid = (entry.valid_mask &
             static_cast<uint16_t>(1U << static_cast<uint8_t>(metric))) != 0;
    unlock();
    return true;
}

bool ChartsRuntimeState::latestMetric(ChartsHistory::Metric metric, float &out_value) const {
    lock();
    if (metric >= ChartsHistory::METRIC_COUNT || count_ == 0) {
        unlock();
        return false;
    }

    for (int offset = static_cast<int>(count_) - 1; offset >= 0; --offset) {
        const ChartsHistory::Entry &entry = entries_[static_cast<uint16_t>(offset)];
        const bool valid =
            (entry.valid_mask & static_cast<uint16_t>(1U << static_cast<uint8_t>(metric))) != 0;
        const float value = entry.values[metric];
        if (valid && isfinite(value)) {
            out_value = value;
            unlock();
            return true;
        }
    }
    unlock();
    return false;
}

void ChartsRuntimeState::lock() const {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void ChartsRuntimeState::unlock() const {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}
