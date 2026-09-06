// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/ChartsRuntimeState.h"

#include <math.h>
#include <new>

#ifdef UNIT_TEST
ChartsRuntimeState::SnapshotCopyHook ChartsRuntimeState::snapshot_copy_hook_ = nullptr;
#endif

ChartsRuntimeState::ChartsRuntimeState() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
}

void ChartsRuntimeState::update(const ChartsHistory &history) {
    const uint16_t source_count = history.count();
    const uint16_t source_index = history.index();
    const uint32_t source_epoch = history.latestEpoch();
    const uint8_t source_optional_gas_type = history.optionalGasType();

    lock();
    const bool unchanged = (count_ == source_count) &&
                           (source_index_ == source_index) &&
                           (latest_epoch_ == source_epoch) &&
                           (optional_gas_type_ == source_optional_gas_type);
    unlock();
    if (unchanged) {
        return;
    }

    lock();
    count_ = source_count;
    source_index_ = source_index;
    latest_epoch_ = source_epoch;
    optional_gas_type_ = source_optional_gas_type;
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

std::unique_ptr<const ChartsRuntimeState::Snapshot> ChartsRuntimeState::copySnapshot() const {
    // Snapshot is about 17 KiB at the current capacity. Keep it off the web
    // task stack and give each request exclusive ownership of its immutable
    // generation. Allocation happens before taking the runtime mutex.
    std::unique_ptr<Snapshot> snapshot(new (std::nothrow) Snapshot());
    if (!snapshot) {
        return {};
    }

    lock();
    snapshot->count_ = count_;
    snapshot->latest_epoch_ = latest_epoch_;
    snapshot->optional_gas_type_ = optional_gas_type_;
#ifdef UNIT_TEST
    if (snapshot_copy_hook_) {
        snapshot_copy_hook_();
    }
#endif
    for (uint16_t offset = 0; offset < count_; ++offset) {
        snapshot->entries_[offset] = entries_[offset];
    }
    unlock();

    return std::unique_ptr<const Snapshot>(snapshot.release());
}

bool ChartsRuntimeState::Snapshot::metricValueFromOldest(
    uint16_t offset,
    ChartsHistory::Metric metric,
    float &value,
    bool &valid) const {
    if (offset >= count_ || metric >= ChartsHistory::METRIC_COUNT) {
        return false;
    }
    const ChartsHistory::Entry &entry = entries_[offset];
    value = entry.values[metric];
    valid = (entry.valid_mask &
             static_cast<uint16_t>(1U << static_cast<uint8_t>(metric))) != 0;
    return true;
}

bool ChartsRuntimeState::Snapshot::latestMetric(ChartsHistory::Metric metric,
                                                float &out_value) const {
    if (metric >= ChartsHistory::METRIC_COUNT || count_ == 0) {
        return false;
    }

    for (int offset = static_cast<int>(count_) - 1; offset >= 0; --offset) {
        const ChartsHistory::Entry &entry = entries_[static_cast<uint16_t>(offset)];
        const bool valid =
            (entry.valid_mask & static_cast<uint16_t>(1U << static_cast<uint8_t>(metric))) != 0;
        const float value = entry.values[metric];
        if (valid && isfinite(value)) {
            out_value = value;
            return true;
        }
    }
    return false;
}

#ifdef UNIT_TEST
void ChartsRuntimeState::setSnapshotCopyHook(SnapshotCopyHook hook) {
    snapshot_copy_hook_ = hook;
}
#endif

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
