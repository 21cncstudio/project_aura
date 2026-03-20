// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "modules/ChartsHistory.h"

class ChartsRuntimeState {
public:
    ChartsRuntimeState();

    void update(const ChartsHistory &history);

    uint16_t count() const;
    uint32_t latestEpoch() const;
    bool entryFromOldest(uint16_t offset, ChartsHistory::Entry &out) const;
    bool metricValueFromOldest(uint16_t offset,
                               ChartsHistory::Metric metric,
                               float &value,
                               bool &valid) const;
    bool latestMetric(ChartsHistory::Metric metric, float &out_value) const;

private:
    void lock() const;
    void unlock() const;

    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    uint16_t count_ = 0;
    uint16_t source_index_ = 0;
    uint32_t latest_epoch_ = 0;
    ChartsHistory::Entry entries_[ChartsHistory::kCapacity]{};
};
