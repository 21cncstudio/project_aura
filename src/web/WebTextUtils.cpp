// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebTextUtils.h"

#include <ctype.h>
#include <stdlib.h>

namespace WebTextUtils {

String htmlEscape(const String &input) {
    String out;
    out.reserve(input.length());
    for (size_t i = 0; i < input.length(); ++i) {
        const char ch = input[i];
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += ch; break;
        }
    }
    return out;
}

int wifiRssiToQuality(int rssi) {
    if (rssi <= -100) {
        return 0;
    }
    if (rssi >= -50) {
        return 100;
    }
    return 2 * (rssi + 100);
}

bool hasControlChars(const String &value) {
    const char *raw = value.c_str();
    if (!raw) {
        return false;
    }

    for (; *raw != '\0'; ++raw) {
        const unsigned char ch = static_cast<unsigned char>(*raw);
        if (ch < 32 || ch == 127) {
            return true;
        }
    }
    return false;
}

bool mqttTopicHasWildcards(const String &topic) {
    const char *raw = topic.c_str();
    if (!raw) {
        return false;
    }

    for (; *raw != '\0'; ++raw) {
        if (*raw == '#' || *raw == '+') {
            return true;
        }
    }
    return false;
}

bool parsePositiveSize(const String &value, size_t &out) {
    const char *raw = value.c_str();
    if (!raw) {
        return false;
    }

    while (*raw != '\0' && isspace(static_cast<unsigned char>(*raw))) {
        ++raw;
    }
    if (*raw == '\0') {
        return false;
    }

    char *end = nullptr;
    const unsigned long long parsed = strtoull(raw, &end, 10);
    while (end && *end != '\0' && isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    if (!end || *end != '\0' || parsed == 0 || parsed > static_cast<unsigned long long>(SIZE_MAX)) {
        return false;
    }

    out = static_cast<size_t>(parsed);
    return true;
}

String wifiLabelSafe(const String &value) {
    if (value.length() == 0) {
        return "---";
    }

    String out;
    out.reserve(value.length());
    const char *raw = value.c_str();
    if (!raw) {
        return "---";
    }

    for (; *raw != '\0'; ++raw) {
        const unsigned char ch = static_cast<unsigned char>(*raw);
        if (ch >= 32 && ch <= 126) {
            out += static_cast<char>(ch);
        } else {
            out += '?';
        }
    }
    return out;
}

} // namespace WebTextUtils
