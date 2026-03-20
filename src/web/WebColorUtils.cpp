// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebColorUtils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace WebColorUtils {

namespace {

int hex_digit_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

} // namespace

String rgbToHexString(uint32_t rgb) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X",
             static_cast<unsigned>((rgb >> 16) & 0xFF),
             static_cast<unsigned>((rgb >> 8) & 0xFF),
             static_cast<unsigned>(rgb & 0xFF));
    return String(buf);
}

bool parseHexColorRgb(const String &value, uint32_t &out_rgb) {
    const char *begin = value.c_str();
    const char *end = begin + strlen(begin);

    while (begin < end && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    if (begin < end && *begin == '#') {
        ++begin;
    }

    if ((end - begin) != 6) {
        return false;
    }

    uint32_t rgb = 0;
    for (const char *it = begin; it < end; ++it) {
        const int digit = hex_digit_value(*it);
        if (digit < 0) {
            return false;
        }
        rgb = (rgb << 4) | static_cast<uint32_t>(digit);
    }

    out_rgb = rgb & 0xFFFFFFu;
    return true;
}

} // namespace WebColorUtils
