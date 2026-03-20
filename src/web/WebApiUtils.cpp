// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebApiUtils.h"

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

namespace WebApiUtils {

const char *eventLevelText(Logger::Level level) {
    switch (level) {
        case Logger::Error: return "E";
        case Logger::Warn: return "W";
        case Logger::Info: return "I";
        case Logger::Debug: return "D";
        default: return "?";
    }
}

const char *eventSeverityText(Logger::Level level) {
    switch (level) {
        case Logger::Error: return "critical";
        case Logger::Warn: return "warning";
        case Logger::Info: return "info";
        case Logger::Debug: return "info";
        default: return "info";
    }
}

bool shouldEmitWebEvent(const Logger::RecentEntry &entry) {
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

String formatUptimeHuman(uint32_t uptime_seconds) {
    uint32_t days = uptime_seconds / 86400UL;
    uptime_seconds %= 86400UL;
    uint32_t hours = uptime_seconds / 3600UL;
    uptime_seconds %= 3600UL;
    uint32_t minutes = uptime_seconds / 60UL;

    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "%lud %luh %lum",
                 static_cast<unsigned long>(days),
                 static_cast<unsigned long>(hours),
                 static_cast<unsigned long>(minutes));
    } else {
        snprintf(buf, sizeof(buf), "%luh %lum",
                 static_cast<unsigned long>(hours),
                 static_cast<unsigned long>(minutes));
    }
    return String(buf);
}

} // namespace WebApiUtils
