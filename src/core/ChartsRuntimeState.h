// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "modules/ChartsHistory.h"

class ChartsRuntimeState {
public:
    class Snapshot {
    public:
        uint16_t count() const { return count_; }
        uint32_t latestEpoch() const { return latest_epoch_; }
        uint8_t optionalGasType() const { return optional_gas_type_; }
        bool metricValueFromOldest(uint16_t offset,
                                   ChartsHistory::Metric metric,
                                   float &value,
                                   bool &valid) const;
        bool latestMetric(ChartsHistory::Metric metric, float &out_value) const;

    private:
        friend class ChartsRuntimeState;
        Snapshot() = default;

        uint16_t count_ = 0;
        uint32_t latest_epoch_ = 0;
        uint8_t optional_gas_type_ = 0;
        ChartsHistory::Entry entries_[ChartsHistory::kCapacity]{};
    };

    ChartsRuntimeState();

    void update(const ChartsHistory &history);
    std::unique_ptr<const Snapshot> copySnapshot() const;

#ifdef UNIT_TEST
    using SnapshotCopyHook = void (*)();
    static void setSnapshotCopyHook(SnapshotCopyHook hook);
#endif

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    uint16_t count_ = 0;
    uint16_t source_index_ = 0;
    uint32_t latest_epoch_ = 0;
    uint8_t optional_gas_type_ = 0;
    ChartsHistory::Entry entries_[ChartsHistory::kCapacity]{};

#ifdef UNIT_TEST
    static SnapshotCopyHook snapshot_copy_hook_;
#endif
};
