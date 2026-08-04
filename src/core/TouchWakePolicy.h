// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace TouchWakePolicy {

constexpr uint32_t FALLBACK_PROBE_INTERVAL_MS = 2500;

enum class Sample : uint8_t {
    Error = 0,
    Released,
    Pressed,
};

class StateMachine {
public:
    void setEnabled(bool enabled, bool touch_released, uint32_t now_ms);
    bool isEnabled() const { return enabled_; }

    bool shouldProbe(bool interrupt_gated,
                     bool interrupt_pending,
                     uint32_t now_ms) const;
    void recordProbe(Sample sample, uint32_t now_ms);

    bool hasPendingWake() const { return pending_wake_; }
    bool takePendingWake();

private:
    bool enabled_ = false;
    bool armed_ = true;
    bool pending_wake_ = false;
    uint32_t last_probe_ms_ = 0;
};

} // namespace TouchWakePolicy
