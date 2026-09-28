// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>

// DFRobot SEN0321 I2C bridge, not the internal ZE27-O3 UART protocol.
// See docs/datasheets/SEN0321 for the pinned manufacturer reference.
class Sen0321 {
public:
    static constexpr uint8_t Address = 0x73;
    static constexpr uint32_t WarmupMs = 180000;
    static constexpr uint32_t StaleMs = 18000;
    void begin();
    bool start(); // ACK only; configuration is deferred to poll().
    void poll();
    bool isPresent() const { return present_; }
    bool isDataValid() const { return valid_ && !isWarmupActive() && uint32_t(millis() - last_data_ms_) <= StaleMs; }
    bool isWarmupActive() const;
    float ppm() const { return ppb_ / 1000.0f; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    void invalidate() { valid_ = false; }
private:
    enum class Phase : uint8_t { Absent, Configure, Request, Select, Read };
    bool write(const uint8_t *bytes, size_t length);
    void fail(uint32_t now);
    Phase phase_ = Phase::Absent;
    bool present_ = false;
    bool valid_ = false;
    bool warmup_complete_ = false;
    uint8_t attempts_ = 0;
    uint8_t failures_ = 0;
    uint16_t ppb_ = 0;
    uint32_t warmup_start_ms_ = 0;
    uint32_t phase_start_ms_ = 0;
    uint32_t wait_ms_ = 0;
    uint32_t last_data_ms_ = 0;
};
