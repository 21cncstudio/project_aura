// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/SystemEventPolicy.h"

#include <string.h>

namespace {

bool event_tag_in_list(const char *tag, const char *const *list, size_t count) {
    if (!tag || tag[0] == '\0' || !list || count == 0) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (list[i] && strcmp(tag, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace SystemEventPolicy {

const char *levelText(Logger::Level level) {
    switch (level) {
        case Logger::Error: return "E";
        case Logger::Warn: return "W";
        case Logger::Info: return "I";
        case Logger::Debug: return "D";
        default: return "?";
    }
}

const char *severityText(Logger::Level level) {
    switch (level) {
        case Logger::Error: return "critical";
        case Logger::Warn: return "warning";
        case Logger::Info: return "info";
        case Logger::Debug: return "info";
        default: return "info";
    }
}

bool shouldEmit(const Logger::RecentEntry &entry) {
    if (strcmp(entry.tag, "Mem") == 0) {
        return false;
    }
    if (entry.level == Logger::Debug) {
        return false;
    }
    if (entry.level == Logger::Error || entry.level == Logger::Warn) {
        return true;
    }

    static const char *const kInfoTags[] = {
        "OTA",
        "WiFi",
        "mDNS",
        "Time",
        "MQTT",
        "Storage",
        "Main",
        "Sensors",
        "PressureHistory",
        "ChartsHistory",
        "UI",
    };
    return event_tag_in_list(entry.tag, kInfoTags, sizeof(kInfoTags) / sizeof(kInfoTags[0]));
}

const char *typeText(const Logger::RecentEntry &entry) {
    return entry.tag[0] ? entry.tag : "SYSTEM";
}

const char *messageText(const Logger::RecentEntry &entry) {
    return entry.message[0] ? entry.message : "Event";
}

} // namespace SystemEventPolicy
