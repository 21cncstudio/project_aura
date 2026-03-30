// SPDX-FileCopyrightText: 2025-2026 netscout2001
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <Arduino.h>
#include "drivers/Lc709203f.h"

class StorageManager;

enum class BattColorLevel : uint8_t {
    Full     = 4,   // >= 90%  blue
    High     = 3,   // >= 60%  green
    Medium   = 2,   // >= 25%  yellow
    Low      = 1,   // >= 10%  orange
    Critical = 0,   //  < 10%  red
};

struct BatteryState {
    bool           detected        = false;
    bool           battery_present = false;
    bool           is_charging     = false;
    float          voltage_v       = 0.0f;
    float          percent         = 0.0f;
    BattColorLevel color_level     = BattColorLevel::Critical;
    uint32_t       last_update_ms  = 0;
};

class BatteryManager {
public:
    static BatteryManager& instance() {
        static BatteryManager inst;
        return inst;
    }

    void begin(StorageManager &storage);
    void update();

    const BatteryState& state()      const { return state_; }
    bool                isDetected() const { return state_.detected; }
    bool                hasBattery() const { return state_.battery_present; }

private:
    BatteryManager() = default;

    static BattColorLevel calcColor(float pct);
    bool                  resolveCharging(uint16_t dir, float new_pct);
    void                  loadSavedSoc();
    void                  saveSocIfNeeded();

    Lc709203f      driver_;
    BatteryState   state_;
    StorageManager *storage_  = nullptr;

    // ── Sliding window RSOC for Auto Mode (0x0000) charging detection ─────────
    // Hardware options checked: CS8501 CHRG pin not routed to any ESP32 GPIO.
    // Software approach: 6 samples × 10s = 60s sliding window.
    //   newest - oldest >= kRsocWindowDelta → charging
    //   newest - oldest <= -kRsocWindowDelta → discharging
    //   stable → keep previous state
    // WiFi bursts (±0.15V) affect voltage but RSOC is internally filtered by
    // the LC709203F HG-CVR algorithm -- random noise cancels over 60s window.
    // Detection time: ~20-30s for fast charging, ~60s for slow charging.
    static constexpr uint8_t  kRsocWindowSize  = 6;     // 60s max window
    static constexpr float    kRsocWindowDelta = 0.2f;  // 0.2% net rise = charging
    static constexpr uint32_t kRsocIntervalMs  = 10000; // 10s sample interval

    float    rsoc_window_[6]  = {};
    uint8_t  rsoc_win_idx_    = 0;
    uint8_t  rsoc_win_count_  = 0;
    uint32_t prev_rsoc_ms_    = 0;

    // Current Direction register values
    static constexpr uint16_t kDirCharge    = 0x0001;
    static constexpr uint16_t kDirAuto      = 0x0000;
    static constexpr uint16_t kDirDischarge = 0xFFFF;

    // SOC persistence
    uint32_t last_save_ms_ = 0;
    static constexpr uint32_t kSaveSocIntervalMs = 300000;
    static constexpr const char* kSocPath        = "/battery_soc.bin";

    static constexpr float kBattFullPct    = 90.0f;
    static constexpr float kBattHighPct    = 60.0f;
    static constexpr float kBattMedPct     = 25.0f;
    static constexpr float kBattLowPct     = 10.0f;
    static constexpr float kBattMinVoltage =  2.80f;

    struct SocRecord {
        float    pct;
        float    voltage_v;
        uint32_t epoch;
        bool     is_charging;
    };
};