// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace Gt911AddressSelect {

enum class Failure : uint8_t {
    None = 0,
    InvalidOps,
    IntOutput,
    IntLow,
    ResetLow,
    IntHigh,
    ResetHigh,
    IntRelease,
};

struct Result {
    Failure failure = Failure::None;

    bool ok() const { return failure == Failure::None; }
};

struct Ops {
    void *context = nullptr;
    bool (*set_int_output)(void *context) = nullptr;
    bool (*set_int_level)(void *context, bool high) = nullptr;
    bool (*set_reset_level)(void *context, bool high) = nullptr;
    bool (*release_int)(void *context) = nullptr;
    void (*delay_ms)(void *context, uint32_t delay_ms) = nullptr;
};

// Select the GT911 backup I2C address (0x14) by holding INT high while reset
// is released. The caller owns the shared I2C bus for the whole sequence.
Result selectBackupAddress(const Ops &ops);

const char *failureText(Failure failure);

} // namespace Gt911AddressSelect
