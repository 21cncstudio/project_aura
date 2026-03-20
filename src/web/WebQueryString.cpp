// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebQueryString.h"

namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

size_t find_char(const String &value, char needle, size_t start = 0) {
    for (size_t i = start; i < value.length(); ++i) {
        if (value[i] == needle) {
            return i;
        }
    }
    return value.length();
}

String slice_copy(const String &value, size_t start, size_t end) {
    String out;
    if (end <= start) {
        return out;
    }
    for (size_t i = start; i < end && i < value.length(); ++i) {
        out += value[i];
    }
    return out;
}

} // namespace

namespace WebQueryString {

String urlDecode(const String &text) {
    String decoded;
    for (size_t i = 0; i < text.length(); ++i) {
        const char encoded = text[i];
        if (encoded == '+') {
            decoded += ' ';
            continue;
        }
        if (encoded == '%' && i + 2 < text.length()) {
            const int hi = hex_value(text[i + 1]);
            const int lo = hex_value(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        decoded += encoded;
    }
    return decoded;
}

void parseArgs(const String &input, std::vector<WebQueryArg> &args) {
    size_t start = 0;
    while (start <= input.length()) {
        size_t next = find_char(input, '&', start);
        if (next > input.length()) {
            next = input.length();
        }

        const String entry = slice_copy(input, start, next);
        if (entry.length() != 0) {
            const size_t equals_index = find_char(entry, '=');
            WebQueryArg arg{};
            if (equals_index >= entry.length()) {
                arg.key = urlDecode(entry);
            } else {
                arg.key = urlDecode(slice_copy(entry, 0, equals_index));
                arg.value = urlDecode(slice_copy(entry, equals_index + 1, entry.length()));
            }
            args.push_back(arg);
        }

        if (next >= input.length()) {
            break;
        }
        start = next + 1;
    }
}

} // namespace WebQueryString
