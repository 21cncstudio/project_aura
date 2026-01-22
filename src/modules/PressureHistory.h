#pragma once
#include <Arduino.h>
#include <time.h>
#include "config/AppConfig.h"
#include "config/AppData.h"

class StorageManager;

class PressureHistory {
public:
    void load(StorageManager &storage, SensorData &data);
    void update(float pressure, SensorData &data, StorageManager &storage);
    using NowEpochFn = time_t (*)();
    static void setNowEpochFn(NowEpochFn fn);

private:
    static time_t nowEpochRaw();
    static NowEpochFn now_epoch_fn_;
    void reset(SensorData &data, StorageManager &storage, bool clear_storage);
    bool isStale(uint32_t now_epoch) const;
    void saveIfDue(StorageManager &storage, uint32_t now_ms);
    void append(float pressure, SensorData &data);
    bool getNowEpoch(uint32_t &now_epoch) const;

    uint32_t last_sample_ms_ = 0;
    uint32_t last_save_ms_ = 0;
    float history_[Config::PRESSURE_HISTORY_24H_SAMPLES] = {};
    int index_ = 0;
    int count_ = 0;
    uint32_t epoch_ = 0;
    bool restored_ = false;
};
