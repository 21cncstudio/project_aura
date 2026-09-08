// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace NetworkIdentity {

// esp_efuse_mac_get_default() returns the six bytes in display order. The
// vendor prefix is bytes 0-2; bytes 3-5 are the device-specific suffix.
constexpr uint32_t macSuffix24(const uint8_t mac[6]) {
    return (static_cast<uint32_t>(mac[3]) << 16U) |
           (static_cast<uint32_t>(mac[4]) << 8U) |
           static_cast<uint32_t>(mac[5]);
}

} // namespace NetworkIdentity
