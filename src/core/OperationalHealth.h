// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

enum class OperationalHealth : uint8_t {
    Unavailable = 0,
    Healthy,
    Unhealthy,
};
