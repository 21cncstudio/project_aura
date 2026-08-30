// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/OperationalHealth.h"

namespace LastGoodHealthPolicy {

struct Inputs {
    bool board_ready;
    bool lvgl_ready;
    bool display_bus_ready;
    bool critical_runtime_fault;
    bool recovery_or_restart_pending;
    bool transient_pause;
    bool ui_runtime_healthy;
};

inline OperationalHealth classify(const Inputs &inputs) {
    if (!inputs.board_ready ||
        !inputs.lvgl_ready ||
        !inputs.display_bus_ready ||
        inputs.critical_runtime_fault ||
        inputs.recovery_or_restart_pending) {
        return OperationalHealth::Unhealthy;
    }
    if (inputs.transient_pause) {
        return OperationalHealth::Unavailable;
    }
    return inputs.ui_runtime_healthy
        ? OperationalHealth::Healthy
        : OperationalHealth::Unhealthy;
}

} // namespace LastGoodHealthPolicy
