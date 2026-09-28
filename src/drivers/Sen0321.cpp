// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "drivers/Sen0321.h"
#include "AuraI2c.h"
#include "config/AppConfig.h"
#include "core/Logger.h"

void Sen0321::begin() { *this = Sen0321{}; }

bool Sen0321::start() {
    phase_start_ms_ = millis();
    if (attempts_ < 3) ++attempts_;
    if (aura_i2c_probe(Config::SENSOR_I2C_PORT, Address, Config::SENSOR_I2C_TIMEOUT_MS) != ESP_OK) {
        phase_ = Phase::Absent;
        wait_ms_ = 10000;
        return false;
    }
    present_ = true;
    valid_ = false;
    warmup_complete_ = false;
    phase_ = Phase::Configure;
    wait_ms_ = 0;
    LOGI("SEN0321", "address 0x73 detected; O3 measurement pending 180 s warmup");
    return true;
}

bool Sen0321::isWarmupActive() const {
    return present_ && !warmup_complete_;
}

bool Sen0321::write(const uint8_t *bytes, size_t length) {
    return aura_i2c_write(Config::SENSOR_I2C_PORT, Address, bytes, length,
                          Config::SENSOR_I2C_TIMEOUT_MS) == ESP_OK;
}

void Sen0321::fail(uint32_t now) {
    valid_ = false; // A transport failure must never become a zero O3 reading.
    if (failures_ < 255) ++failures_;
    if (failures_ == 1 || failures_ == 3) {
        LOGW("SEN0321", "invalid I2C sample (%u failures); retry in 5 s", failures_);
    }
    // Even one failed transaction can be a power interruption. Count a new
    // warmup only after successful reconfiguration, never while disconnected.
    warmup_complete_ = false;
    phase_ = Phase::Configure;
    phase_start_ms_ = now;
    wait_ms_ = 5000;
}

void Sen0321::poll() {
    const uint32_t now = millis();
    if (now - phase_start_ms_ < wait_ms_) return;
    if (phase_ == Phase::Absent) {
        if (attempts_ < 3) start();
        return;
    }
    // Each call performs at most one bounded transaction. The two manufacturer
    // 100 ms waits release the shared bus and sensor loop between transactions.
    bool ok = false;
    switch (phase_) {
    case Phase::Configure: {
        const uint8_t command[] = {0x03, 0x01}; // passive mode
        ok = write(command, sizeof(command));
        if (ok) {
            warmup_start_ms_ = millis();
            phase_ = Phase::Request;
        }
        break;
    }
    case Phase::Request: {
        const uint8_t command[] = {0x04, 0x01}; // request passive acquisition
        ok = write(command, sizeof(command));
        if (ok) phase_ = Phase::Select;
        break;
    }
    case Phase::Select: {
        const uint8_t reg = 0x07;
        ok = write(&reg, 1);
        if (ok) phase_ = Phase::Read;
        break;
    }
    case Phase::Read: {
        // AuraI2c requires the complete requested transfer or returns an error.
        uint8_t bytes[2] = {0xff, 0xff};
        ok = aura_i2c_read(Config::SENSOR_I2C_PORT, Address, bytes, sizeof(bytes),
                            Config::SENSOR_I2C_TIMEOUT_MS) == ESP_OK;
        const uint16_t ppb = (uint16_t(bytes[0]) << 8) | bytes[1];
        ok = ok && ppb <= 10000;
        if (ok) {
            ppb_ = ppb;
            failures_ = 0;
            if (!warmup_complete_ && now - warmup_start_ms_ >= WarmupMs)
                warmup_complete_ = true;
            valid_ = warmup_complete_;
            if (valid_) last_data_ms_ = now;
            phase_ = Phase::Request;
        }
        break;
    }
    case Phase::Absent: return;
    }
    if (!ok) { fail(millis()); return; }
    phase_start_ms_ = millis();
    wait_ms_ = phase_ == Phase::Request ? 1000 : 100;
}
