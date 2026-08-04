// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/TouchWakePolicy.h"

namespace TouchWakePolicy {

void StateMachine::setEnabled(bool enabled, bool touch_released, uint32_t now_ms) {
    if (enabled == enabled_) {
        return;
    }

    enabled_ = enabled;
    pending_wake_ = false;
    armed_ = enabled ? touch_released : true;
    last_probe_ms_ = enabled ? now_ms : 0;
}

bool StateMachine::shouldProbe(bool interrupt_gated,
                               bool interrupt_pending,
                               uint32_t now_ms) const {
    if (!enabled_ || pending_wake_) {
        return false;
    }
    if (!interrupt_gated || interrupt_pending) {
        return true;
    }
    return static_cast<uint32_t>(now_ms - last_probe_ms_) >=
           FALLBACK_PROBE_INTERVAL_MS;
}

void StateMachine::recordProbe(Sample sample, uint32_t now_ms) {
    if (!enabled_) {
        return;
    }

    last_probe_ms_ = now_ms;
    if (sample == Sample::Released) {
        armed_ = true;
    } else if (sample == Sample::Pressed && armed_) {
        pending_wake_ = true;
    }
}

bool StateMachine::takePendingWake() {
    const bool pending = pending_wake_;
    pending_wake_ = false;
    return pending;
}

} // namespace TouchWakePolicy
