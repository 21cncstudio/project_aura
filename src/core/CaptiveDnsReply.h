// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace CaptiveDnsReply {
// One uncompressed IN question; A/ANY redirects to the AP, other types return
// NOERROR without answers. Malformed/unsupported queries are silently dropped.
// Input/output may alias. No allocation, name logging or recursive resolution.
size_t build(const uint8_t *query, size_t length, uint8_t *reply,
             size_t capacity, const uint8_t address[4]);
}
