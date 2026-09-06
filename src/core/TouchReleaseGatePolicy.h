// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace TouchReleaseGatePolicy {

constexpr uint32_t QUIET_SAMPLE_MIN_INTERVAL_MS = 40;

enum class ProbeResult : uint8_t { NoData, Released, Pressed, Error };

enum class Decision : uint8_t { Hold, Open };

// Status-aware release gate used after a touch-read block. A real GT911
// release frame is always sufficient. The quiet fallback is deliberately
// narrower: it is available only when the gate started from a cached release
// with INT inactive, and it is permanently disabled after a press is seen.
// This prevents an early ready-bit gap from turning a held wake touch into a
// synthetic release.
class Gate {
public:
    void begin(bool cached_released, bool interrupt_active) {
        waiting_ = true;
        quiet_fallback_allowed_ = cached_released && !interrupt_active;
        press_seen_ = false;
        resetQuietSamples();
    }

    Decision observe(ProbeResult result,
                     bool interrupt_active,
                     uint32_t now_ms) {
        if (!waiting_) {
            return Decision::Open;
        }

        if (result == ProbeResult::Released) {
            waiting_ = false;
            resetQuietSamples();
            return Decision::Open;
        }

        if (result == ProbeResult::Pressed) {
            press_seen_ = true;
            resetQuietSamples();
            return Decision::Hold;
        }

        if (result == ProbeResult::Error) {
            resetQuietSamples();
            return Decision::Hold;
        }

        // NoData is only quiet evidence while INT is inactive. An active INT
        // can precede the GT911 ready bit, so it restarts the quiet window.
        if (interrupt_active) {
            resetQuietSamples();
            return Decision::Hold;
        }

        if (!quiet_fallback_allowed_ || press_seen_) {
            return Decision::Hold;
        }

        if (!quiet_sample_recorded_) {
            quiet_sample_recorded_ = true;
            first_quiet_sample_ms_ = now_ms;
            return Decision::Hold;
        }

        // Unsigned subtraction is well-defined across the uint32_t millis()
        // wrap, provided the measured interval is below half the range.
        if (static_cast<uint32_t>(now_ms - first_quiet_sample_ms_) <
            QUIET_SAMPLE_MIN_INTERVAL_MS) {
            return Decision::Hold;
        }

        waiting_ = false;
        resetQuietSamples();
        return Decision::Open;
    }

    bool waiting() const { return waiting_; }
    bool quietFallbackAllowed() const {
        return waiting_ && quiet_fallback_allowed_ && !press_seen_;
    }

private:
    void resetQuietSamples() {
        quiet_sample_recorded_ = false;
        first_quiet_sample_ms_ = 0;
    }

    bool waiting_ = true;
    bool quiet_fallback_allowed_ = false;
    bool press_seen_ = false;
    bool quiet_sample_recorded_ = false;
    uint32_t first_quiet_sample_ms_ = 0;
};

} // namespace TouchReleaseGatePolicy
