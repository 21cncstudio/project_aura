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

class StorageManager;

class PressureHistory {
public:
    void load(StorageManager &storage, SensorData &data);
    void update(float pressure,
                SensorData &data,
                StorageManager &storage,
                bool system_time_trusted);
#ifdef UNIT_TEST
    void update(float pressure, SensorData &data, StorageManager &storage) {
        update(pressure, data, storage, true);
    }
#endif
    bool flush(StorageManager &storage);
    using NowEpochFn = time_t (*)();
    static void setNowEpochFn(NowEpochFn fn);

private:
    static time_t nowEpochRaw();
    static NowEpochFn now_epoch_fn_;
    void reset(SensorData &data, StorageManager &storage, bool clear_storage);
    bool isStale(uint32_t now_epoch) const;
    bool saveIfDue(StorageManager &storage, uint32_t now_ms, bool force = false);
    void append(float pressure, SensorData &data);
    void recomputeDeltas(SensorData &data) const;
    bool getNowEpoch(uint32_t &now_epoch) const;

    uint32_t last_sample_ms_ = 0;
    uint32_t last_save_ms_ = 0;
    float history_[Config::PRESSURE_HISTORY_24H_SAMPLES] = {};
    int index_ = 0;
    int count_ = 0;
    uint32_t epoch_ = 0;
    bool restored_ = false;
    bool backward_time_hold_ = false;
    bool replacement_save_pending_ = false;
};
