// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace RuntimeI2cRecoveryPolicy {

constexpr uint32_t SAMPLE_INTERVAL_MS = 1000U;
constexpr uint8_t STUCK_LINE_SAMPLE_LIMIT = 5U;

enum class Decision : uint8_t {
    None = 0,
    Restart,
    SuppressAlreadyAttempted,
    SuppressRestartUnavailable,
};

class State {
public:
    Decision poll(uint32_t now_ms,
                  bool lines_idle,
                  bool touch_offline,
                  bool recovery_boot,
                  bool restart_available) {
        if (!shared_bus_fault_confirmed_ &&
            (!sampled_ ||
             static_cast<uint32_t>(now_ms - last_sample_ms_) >= SAMPLE_INTERVAL_MS)) {
            sampled_ = true;
            last_sample_ms_ = now_ms;

            if (lines_idle) {
                stuck_line_samples_ = 0;
            } else {
                if (stuck_line_samples_ < UINT8_MAX) {
                    ++stuck_line_samples_;
                }
                if (stuck_line_samples_ >= STUCK_LINE_SAMPLE_LIMIT) {
                    shared_bus_fault_confirmed_ = true;
                }
            }
        }

        if (handled_) {
            return Decision::None;
        }

        if (touch_offline || shared_bus_fault_confirmed_) {
            return handleFault(recovery_boot, restart_available);
        }
        return Decision::None;
    }

    uint8_t stuckLineSamples() const { return stuck_line_samples_; }
    bool handled() const { return handled_; }
    bool sharedBusFaultConfirmed() const { return shared_bus_fault_confirmed_; }

private:
    Decision handleFault(bool recovery_boot, bool restart_available) {
        handled_ = true;
        if (recovery_boot) {
            return Decision::SuppressAlreadyAttempted;
        }
        if (!restart_available) {
            return Decision::SuppressRestartUnavailable;
        }
        return Decision::Restart;
    }

    uint32_t last_sample_ms_ = 0;
    uint8_t stuck_line_samples_ = 0;
    bool sampled_ = false;
    bool handled_ = false;
    bool shared_bus_fault_confirmed_ = false;
};

inline const char *decisionText(Decision decision) {
    switch (decision) {
        case Decision::None: return "none";
        case Decision::Restart: return "restart_requested";
        case Decision::SuppressAlreadyAttempted: return "already_attempted";
        case Decision::SuppressRestartUnavailable: return "restart_unavailable";
        default: return "unknown";
    }
}

} // namespace RuntimeI2cRecoveryPolicy
