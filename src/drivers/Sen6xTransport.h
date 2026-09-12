// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/I2CHelper.h"
#include <stdint.h>
#include <string.h>

// Shared wire framing only. Model-specific timing and interpretation live in
// the drivers. Each read must follow a command and its execution deadline.
namespace Sen6xTransport {
constexpr uint8_t Address = 0x6B;
constexpr uint32_t CommandMs = 20;
inline bool command(uint16_t cmd) {
    return I2C::write_cmd(Address, cmd, nullptr, 0) == ESP_OK;
}
inline bool write(uint16_t cmd, const uint16_t *words, size_t count) {
    if (!words || count > 6) return false;
    uint8_t bytes[18]{};
    for (size_t i = 0; i < count; ++i) {
        bytes[3*i] = words[i] >> 8;
        bytes[3*i+1] = words[i];
        bytes[3*i+2] = I2C::crc8(bytes + 3*i, 2);
    }
    return I2C::write_cmd(Address, cmd, bytes, count*3) == ESP_OK;
}
inline bool receive(uint16_t *words, size_t count) {
    if (!words || count == 0 || count > 16) return false;
    uint8_t bytes[48]{};
    if (I2C::read_bytes(Address, bytes, count*3) != ESP_OK) return false;
    for (size_t i = 0; i < count; ++i)
        if (I2C::crc8(bytes + 3*i, 2) != bytes[3*i+2]) return false;
    for (size_t i = 0; i < count; ++i)
        words[i] = (uint16_t(bytes[3*i]) << 8) | bytes[3*i+1];
    return true;
}
inline bool receiveString(char (&out)[32]) {
    uint16_t words[16]{};
    if (!receive(words, 16)) return false;
    char decoded[32]{};
    for (size_t i = 0; i < 16; ++i) {
        decoded[2*i] = words[i] >> 8;
        decoded[2*i+1] = words[i];
    }
    if (!memchr(decoded, '\0', sizeof(decoded)) || !decoded[0]) return false;
    for (size_t i = 0; decoded[i]; ++i)
        if (decoded[i] < 0x20 || decoded[i] > 0x7e) return false;
    memcpy(out, decoded, sizeof(out));
    return true;
}
inline bool due(uint32_t now, uint32_t deadline) {
    return int32_t(now - deadline) >= 0;
}
}
