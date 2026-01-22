// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "PressureHistory.h"
#include <string.h>
#include <time.h>
#include "core/Logger.h"
#include "modules/StorageManager.h"

namespace {

constexpr uint32_t kPressureHistoryMagic = 0x50524849; // "PRHI"
constexpr uint16_t kPressureHistoryVersion = 1;

struct PressureHistoryBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t epoch;
    uint16_t index;
    uint16_t count;
    float history[Config::PRESSURE_HISTORY_24H_SAMPLES];
};

} // namespace

PressureHistory::NowEpochFn PressureHistory::now_epoch_fn_ = &PressureHistory::nowEpochRaw;

time_t PressureHistory::nowEpochRaw() {
    return time(nullptr);
}

void PressureHistory::setNowEpochFn(NowEpochFn fn) {
    now_epoch_fn_ = fn ? fn : &PressureHistory::nowEpochRaw;
}

bool PressureHistory::getNowEpoch(uint32_t &now_epoch) const {
    time_t now = now_epoch_fn_();
    if (now <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    now_epoch = static_cast<uint32_t>(now);
    return true;
}

void PressureHistory::reset(SensorData &data, StorageManager &storage, bool clear_storage) {
    index_ = 0;
    count_ = 0;
    epoch_ = 0;
    restored_ = false;
    memset(history_, 0, sizeof(history_));
    data.pressure_delta_3h_valid = false;
    data.pressure_delta_24h_valid = false;
    if (clear_storage) {
        storage.removeBlob(StorageManager::kPressurePath);
    }
}

bool PressureHistory::isStale(uint32_t now_epoch) const {
    if (epoch_ == 0) {
        return false;
    }
    if (now_epoch < epoch_) {
        return true;
    }
    return (now_epoch - epoch_) > Config::PRESSURE_HISTORY_MAX_AGE_S;
}

void PressureHistory::load(StorageManager &storage, SensorData &data) {
    PressureHistoryBlob blob = {};
    if (!storage.loadBlob(StorageManager::kPressurePath, &blob, sizeof(blob))) {
        restored_ = false;
        Logger::log(Logger::Debug, "PressureHistory",
                    "no stored history");
        return;
    }

    if (blob.magic != kPressureHistoryMagic || blob.version != kPressureHistoryVersion) {
        LOGW("PressureHistory", "invalid stored history header, reset");
        reset(data, storage, true);
        return;
    }

    memcpy(history_, blob.history, sizeof(history_));
    index_ = static_cast<int>(blob.index);
    count_ = static_cast<int>(blob.count);
    epoch_ = blob.epoch;

    if (index_ >= Config::PRESSURE_HISTORY_24H_SAMPLES ||
        count_ > Config::PRESSURE_HISTORY_24H_SAMPLES) {
        LOGW("PressureHistory", "invalid stored index/count, reset");
        reset(data, storage, true);
        return;
    }

    uint32_t now_epoch = 0;
    if (getNowEpoch(now_epoch) && isStale(now_epoch)) {
        LOGW("PressureHistory", "stored history stale, reset");
        reset(data, storage, true);
        return;
    }
    last_sample_ms_ = millis() - Config::PRESSURE_HISTORY_STEP_MS;
    restored_ = true;
    Logger::log(Logger::Info, "PressureHistory",
                "restored count=%d idx=%d epoch=%u",
                count_, index_, epoch_);
}

void PressureHistory::saveIfDue(StorageManager &storage, uint32_t now_ms) {
    if (count_ == 0) {
        return;
    }
    if (now_ms - last_save_ms_ < Config::PRESSURE_HISTORY_SAVE_MS) {
        return;
    }
    last_save_ms_ = now_ms;
    PressureHistoryBlob blob = {};
    blob.magic = kPressureHistoryMagic;
    blob.version = kPressureHistoryVersion;
    blob.epoch = epoch_;
    blob.index = static_cast<uint16_t>(index_);
    blob.count = static_cast<uint16_t>(count_);
    memcpy(blob.history, history_, sizeof(history_));
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));
}

void PressureHistory::append(float pressure, SensorData &data) {
    history_[index_] = pressure;
    index_ = (index_ + 1) % Config::PRESSURE_HISTORY_24H_SAMPLES;
    if (count_ < Config::PRESSURE_HISTORY_24H_SAMPLES) {
        count_++;
    }
    if (count_ == Config::PRESSURE_HISTORY_3H_STEPS + 1) {
        LOGI("PressureHistory", "3h delta ready");
    }
    if (count_ == Config::PRESSURE_HISTORY_24H_SAMPLES) {
        LOGI("PressureHistory", "24h delta ready");
    }

    int latest_index = (index_ + Config::PRESSURE_HISTORY_24H_SAMPLES - 1) %
                       Config::PRESSURE_HISTORY_24H_SAMPLES;
    if (count_ > Config::PRESSURE_HISTORY_3H_STEPS) {
        int idx_3h = (latest_index + Config::PRESSURE_HISTORY_24H_SAMPLES -
                      Config::PRESSURE_HISTORY_3H_STEPS) %
                     Config::PRESSURE_HISTORY_24H_SAMPLES;
        data.pressure_delta_3h = pressure - history_[idx_3h];
        data.pressure_delta_3h_valid = true;
    } else {
        data.pressure_delta_3h_valid = false;
    }

    if (count_ >= Config::PRESSURE_HISTORY_24H_SAMPLES) {
        int idx_24h = index_;
        data.pressure_delta_24h = pressure - history_[idx_24h];
        data.pressure_delta_24h_valid = true;
    } else {
        data.pressure_delta_24h_valid = false;
    }
}

void PressureHistory::update(float pressure, SensorData &data, StorageManager &storage) {
    uint32_t now_ms = millis();
    uint32_t now_epoch = 0;
    bool time_valid = getNowEpoch(now_epoch);
    if (time_valid) {
        if (isStale(now_epoch)) {
            LOGW("PressureHistory", "history stale, reset");
            reset(data, storage, true);
            last_sample_ms_ = now_ms - Config::PRESSURE_HISTORY_STEP_MS;
        }
    }

    if (time_valid && restored_ && epoch_ != 0 && count_ > 0) {
        uint32_t gap_s = (now_epoch >= epoch_) ? (now_epoch - epoch_) : 0;
        if (gap_s >= Config::PRESSURE_HISTORY_FILL_LONG_S) {
            Logger::log(Logger::Warn, "PressureHistory",
                        "gap %us, reset",
                        static_cast<unsigned>(gap_s));
            reset(data, storage, true);
            last_sample_ms_ = now_ms - Config::PRESSURE_HISTORY_STEP_MS;
        } else if (gap_s >= Config::PRESSURE_HISTORY_FILL_SHORT_S) {
            Logger::log(Logger::Info, "PressureHistory",
                        "filling gap %us",
                        static_cast<unsigned>(gap_s));
            uint32_t step_s = Config::PRESSURE_HISTORY_STEP_MS / 1000UL;
            uint32_t steps = gap_s / step_s;
            int latest_index = (index_ + Config::PRESSURE_HISTORY_24H_SAMPLES - 1) %
                               Config::PRESSURE_HISTORY_24H_SAMPLES;
            float start = history_[latest_index];
            for (uint32_t i = 1; i <= steps; ++i) {
                float value = start + (pressure - start) * (static_cast<float>(i) / steps);
                append(value, data);
            }
            epoch_ += steps * step_s;
            last_sample_ms_ = now_ms;
            saveIfDue(storage, now_ms);
            restored_ = false;
            return;
        }
        restored_ = false;
    }

    if (time_valid) {
        uint32_t step_s = Config::PRESSURE_HISTORY_STEP_MS / 1000UL;
        if (epoch_ != 0) {
            if (now_epoch - epoch_ < step_s) {
                return;
            }
        } else if (now_ms - last_sample_ms_ < Config::PRESSURE_HISTORY_STEP_MS) {
            return;
        }
    } else if (now_ms - last_sample_ms_ < Config::PRESSURE_HISTORY_STEP_MS) {
        return;
    }
    last_sample_ms_ = now_ms;

    append(pressure, data);
    epoch_ = time_valid ? now_epoch : 0;
    restored_ = false;

    saveIfDue(storage, now_ms);
}
