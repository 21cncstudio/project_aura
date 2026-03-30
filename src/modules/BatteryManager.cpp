// SPDX-FileCopyrightText: 2025-2026 netscout2001
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modules/BatteryManager.h"
#include "modules/StorageManager.h"
#include "core/Logger.h"
#include <time.h>

BattColorLevel BatteryManager::calcColor(float pct) {
    if (pct >= kBattFullPct)  return BattColorLevel::Full;
    if (pct >= kBattHighPct)  return BattColorLevel::High;
    if (pct >= kBattMedPct)   return BattColorLevel::Medium;
    if (pct >= kBattLowPct)   return BattColorLevel::Low;
    return BattColorLevel::Critical;
}

// ── Hybrid charging detection ─────────────────────────────────────────────────
// 1. Current Direction register (per LC709203F datasheet):
//    0x0001 (Charge)    → definitively charging -- instant response
//    0xFFFF (Discharge) → definitively discharging -- instant response
//    0x0000 (Auto)      → chip undecided; use sliding window RSOC analysis
//
// 2. Sliding window RSOC (for Auto Mode only):
//    Maintains a circular buffer of the last kRsocWindowSize RSOC readings
//    (one per kRsocIntervalMs). Compares newest vs oldest in the buffer:
//      delta >= +kRsocWindowDelta → charging
//      delta <= -kRsocWindowDelta → discharging
//      else                       → keep previous state
//
//    This approach is immune to WiFi TX voltage spikes because:
//    - The LC709203F HG-CVR algorithm already filters fast transients from RSOC
//    - Random ±noise cancels out over the 60s window
//    - Only sustained directional trends (real charge/discharge) are detected
//
// Detection times:
//    Fast charging (~1%/min): delta threshold reached in ~20-30s ✓
//    Slow charging (~0.2%/min): detected after ~60s ✓
//    Trickle/full battery: RSOC stable → no false positives ✓
bool BatteryManager::resolveCharging(uint16_t dir, float new_pct) {
    // Definitive cases -- instant response
    if (dir == kDirCharge)    return true;
    if (dir == kDirDischarge) return false;

    // Auto Mode -- sliding window RSOC analysis
    const uint32_t now = static_cast<uint32_t>(millis());

    // Only add a new sample when enough time has elapsed
    if (prev_rsoc_ms_ != 0 && (now - prev_rsoc_ms_) < kRsocIntervalMs) {
        return state_.is_charging;  // not yet time for new sample
    }
    prev_rsoc_ms_ = now;

    // Add new sample to circular buffer
    rsoc_window_[rsoc_win_idx_] = new_pct;
    rsoc_win_idx_ = (rsoc_win_idx_ + 1) % kRsocWindowSize;
    if (rsoc_win_count_ < kRsocWindowSize) rsoc_win_count_++;

    // Need at least 2 samples to compare
    if (rsoc_win_count_ < 2) return state_.is_charging;

    // Compare newest sample vs oldest sample in the window
    // newest: one step behind current idx (just written)
    // oldest: current idx (about to be overwritten next time, or earliest)
    const uint8_t newest_idx = (rsoc_win_idx_ + kRsocWindowSize - 1) % kRsocWindowSize;
    const uint8_t oldest_idx = (rsoc_win_count_ < kRsocWindowSize)
                               ? 0
                               : rsoc_win_idx_;  // full window: oldest = next write pos
    const float newest = rsoc_window_[newest_idx];
    const float oldest = rsoc_window_[oldest_idx];
    const float delta  = newest - oldest;

    if (delta >= kRsocWindowDelta)  return true;   // sustained rise → charging
    if (delta <= -kRsocWindowDelta) return false;  // sustained fall → discharging
    return state_.is_charging;                     // stable → keep previous
}

// ── LittleFS SOC persistence ──────────────────────────────────────────────────
void BatteryManager::loadSavedSoc() {
    if (!storage_) return;
    SocRecord rec{};
    if (!storage_->loadBlob(kSocPath, &rec, sizeof(rec))) {
        LOGI("Battery", "No saved SOC found");
        return;
    }
    if (rec.pct < 0.0f || rec.pct > 100.0f || rec.voltage_v < 2.5f || rec.voltage_v > 4.5f) {
        LOGW("Battery", "Saved SOC invalid, ignoring");
        return;
    }
    LOGI("Battery", "Restored SOC from LittleFS: %.0f%% (%.3fV)", (double)rec.pct, (double)rec.voltage_v);
    state_.percent         = rec.pct;
    state_.voltage_v       = rec.voltage_v;
    state_.is_charging     = rec.is_charging;
    state_.battery_present = (rec.voltage_v >= kBattMinVoltage);
    state_.color_level     = calcColor(rec.pct);
}

void BatteryManager::saveSocIfNeeded() {
    if (!storage_ || !state_.battery_present) return;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (last_save_ms_ != 0 && (now - last_save_ms_) < kSaveSocIntervalMs) return;
    last_save_ms_ = now;
    SocRecord rec{};
    rec.pct         = state_.percent;
    rec.voltage_v   = state_.voltage_v;
    rec.epoch       = static_cast<uint32_t>(time(nullptr));
    rec.is_charging = state_.is_charging;
    if (!storage_->saveBlobAtomic(kSocPath, &rec, sizeof(rec))) {
        LOGW("Battery", "SOC save failed");
    }
}

// ── begin() ───────────────────────────────────────────────────────────────────
void BatteryManager::begin(StorageManager &storage) {
    storage_       = &storage;
    state_         = BatteryState{};
    last_save_ms_  = 0;
    prev_rsoc_ms_  = 0;
    rsoc_win_idx_  = 0;
    rsoc_win_count_= 0;
    for (auto &s : rsoc_window_) s = 0.0f;

    // ── TEST MODE ────────────────────────────────────────────────────────────
    // state_.detected        = true;
    // state_.battery_present = true;
    // state_.is_charging     = true;
    // state_.percent         = 72.0f;
    // state_.voltage_v       = 3.85f;
    // state_.color_level     = BattColorLevel::High;
    // state_.last_update_ms  = static_cast<uint32_t>(millis());
    // LOGI("Battery", "TEST MODE: simulated battery 72%%");
    // return;
    // ── END TEST MODE ────────────────────────────────────────────────────────

    driver_.begin();
    if (!driver_.start()) {
        LOGI("Battery", "LC709203F not found -- Battery UI disabled");
        return;
    }
    state_.detected = true;
    loadSavedSoc();
    LOGI("Battery", "LC709203F ready, starting initial read");
    update();
}

// ── update() ──────────────────────────────────────────────────────────────────
void BatteryManager::update() {
    if (!state_.detected) return;
    driver_.poll();
    if (!driver_.isDataValid()) return;

    const float    v            = driver_.cellVoltage();
    const float    pct          = driver_.cellPercent();
    const uint16_t dir          = driver_.currentDir();
    const bool     new_present  = (v >= kBattMinVoltage);
    const bool     new_charging = resolveCharging(dir, pct);
    const BattColorLevel new_level = calcColor(pct);

    if (new_present != state_.battery_present) {
        if (new_present)
            LOGI("Battery", "Battery connected (%.3fV, %.0f%%)", (double)v, (double)pct);
        else
            LOGW("Battery", "Battery disconnected or drained (%.3fV)", (double)v);
    }
    if (new_present && (new_charging != state_.is_charging)) {
        if (new_charging)
            LOGI("Battery", "Charging started (%.0f%%)", (double)pct);
        else
            LOGI("Battery", "Charging stopped (%.0f%%)", (double)pct);
    }
    if (new_present && (new_level != state_.color_level)) {
        switch (new_level) {
            case BattColorLevel::Low:
                LOGW("Battery", "Battery low: %.0f%% (below 25%%)", (double)pct); break;
            case BattColorLevel::Critical:
                LOGW("Battery", "Battery CRITICAL: %.0f%% (below 10%%)", (double)pct); break;
            case BattColorLevel::Full:
                LOGI("Battery", "Battery full: %.0f%%", (double)pct); break;
            default: break;
        }
    }

    state_.voltage_v       = v;
    state_.percent         = pct;
    state_.battery_present = new_present;
    state_.is_charging     = new_charging;
    state_.color_level     = new_level;
    state_.last_update_ms  = static_cast<uint32_t>(millis());

    saveSocIfNeeded();
}