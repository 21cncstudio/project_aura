// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace CooperativeStart {

enum class Result : uint8_t {
    Idle = 0,
    InProgress,
    Success,
    Failed,
};

} // namespace CooperativeStart
