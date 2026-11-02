// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/CaptiveDnsReply.h"
#include <string.h>

namespace {
uint16_t read16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }
void write16(uint8_t *p, uint16_t value) { p[0] = value >> 8; p[1] = value & 0xff; }
}

size_t CaptiveDnsReply::build(const uint8_t *query, size_t length, uint8_t *reply,
                              size_t capacity, const uint8_t address[4]) {
    if (!query || !reply || !address || length < 17 || length > 512) return 0;
    const uint16_t flags = read16(query + 2);
    if ((flags & 0xf800) || read16(query + 4) != 1 ||
        read16(query + 6) || read16(query + 8)) return 0;
    size_t end = 12;
    for (;;) {
        if (end >= length || end - 12 >= 255) return 0;
        const uint8_t label = query[end++];
        if (!label) break;
        // Compression and extended labels are deliberately unsupported here.
        if (label > 63 || label > length - end) return 0;
        end += label;
    }
    if (length - end < 4) return 0;
    const uint16_t type = read16(query + end);
    if (read16(query + end + 2) != 1) return 0;
    end += 4;
    const bool answer = type == 1 || type == 255;
    const size_t total = end + (answer ? 16 : 0);
    if (total > capacity) return 0;
    memmove(reply, query, end);
    // Authoritative local answer, preserve RD but never claim recursive service
    // or authenticated DNSSEC data. Strip any query additional/EDNS records.
    write16(reply + 2, 0x8400 | (flags & 0x0100));
    write16(reply + 4, 1);
    write16(reply + 6, answer ? 1 : 0);
    write16(reply + 8, 0);
    write16(reply + 10, 0);
    if (answer) {
        uint8_t *record = reply + end;
        write16(record, 0xc00c);
        write16(record + 2, 1);
        write16(record + 4, 1);
        memset(record + 6, 0, 4); // TTL zero: do not retain portal answers on STA.
        write16(record + 10, 4);
        memcpy(record + 12, address, 4);
    }
    return total;
}
