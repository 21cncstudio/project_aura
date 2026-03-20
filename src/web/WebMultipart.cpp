// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebMultipart.h"

namespace {

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

bool starts_with(const String &value, const String &prefix) {
    if (prefix.length() > value.length()) {
        return false;
    }
    for (size_t i = 0; i < prefix.length(); ++i) {
        if (value[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

void ascii_lower_in_place(String &value) {
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c >= 'A' && c <= 'Z') {
            value[i] = static_cast<char>(c - 'A' + 'a');
        }
    }
}

String trim_copy(const String &value) {
    size_t start = 0;
    while (start < value.length()) {
        const char c = value[start];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        ++start;
    }

    size_t end = value.length();
    while (end > start) {
        const char c = value[end - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        --end;
    }

    return slice_copy(value, start, end);
}

String unquote_copy(const String &value) {
    if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"') {
        return slice_copy(value, 1, value.length() - 1);
    }
    return value;
}

String header_param_value(const String &header, const char *key) {
    if (!key || !key[0]) {
        return String();
    }

    const String token = String(key) + "=";
    size_t start = 0;
    while (start < header.length()) {
        const size_t next = find_char(header, ';', start);
        String part = trim_copy(slice_copy(header, start, next));
        if (starts_with(part, token)) {
            return unquote_copy(trim_copy(slice_copy(part, token.length(), part.length())));
        }
        start = next + 1;
    }
    return String();
}

} // namespace

namespace WebMultipart {

String parseBoundary(const String &content_type) {
    return trim_copy(header_param_value(content_type, "boundary"));
}

bool parseContentDisposition(const String &header, String &name, String &filename) {
    const size_t colon = find_char(header, ':');
    if (colon >= header.length()) {
        return false;
    }

    String header_name = trim_copy(slice_copy(header, 0, colon));
    ascii_lower_in_place(header_name);
    if (header_name != "content-disposition") {
        return false;
    }

    const String value = trim_copy(slice_copy(header, colon + 1, header.length()));
    String kind = value;
    const size_t semicolon = find_char(kind, ';');
    if (semicolon < kind.length()) {
        kind = slice_copy(kind, 0, semicolon);
    }
    kind = trim_copy(kind);
    ascii_lower_in_place(kind);
    if (kind != "form-data") {
        return false;
    }

    name = header_param_value(value, "name");
    filename = header_param_value(value, "filename");
    return true;
}

} // namespace WebMultipart
