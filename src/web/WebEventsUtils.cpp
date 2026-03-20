// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebEventsUtils.h"

#include "web/WebApiUtils.h"

namespace {

const char *safe_tag(const Logger::RecentEntry &entry) {
    return entry.tag[0] ? entry.tag : "SYSTEM";
}

const char *safe_message(const Logger::RecentEntry &entry, const char *fallback) {
    return entry.message[0] ? entry.message : fallback;
}

} // namespace

namespace WebEventsUtils {

size_t fillRecentErrorsJson(ArduinoJson::JsonArray errors,
                            const Logger::RecentEntry *entries,
                            size_t count,
                            size_t max_items) {
    if (!entries || max_items == 0) {
        return 0;
    }

    size_t added = 0;
    for (size_t i = 0; i < count && added < max_items; ++i) {
        const Logger::RecentEntry &entry = entries[i];
        if (entry.level != Logger::Error && entry.level != Logger::Warn) {
            continue;
        }
        ArduinoJson::JsonObject item = errors.add<ArduinoJson::JsonObject>();
        item["ts_ms"] = entry.ms;
        item["level"] = WebApiUtils::eventLevelText(entry.level);
        item["tag"] = safe_tag(entry);
        item["message"] = safe_message(entry, "");
        added++;
    }
    return added;
}

uint32_t fillEventsJson(ArduinoJson::JsonArray events,
                        const Logger::RecentEntry *entries,
                        size_t count) {
    if (!entries) {
        return 0;
    }

    uint32_t emitted_count = 0;
    for (size_t i = 0; i < count; ++i) {
        const Logger::RecentEntry &entry = entries[i];
        if (!WebApiUtils::shouldEmitWebEvent(entry)) {
            continue;
        }
        ArduinoJson::JsonObject item = events.add<ArduinoJson::JsonObject>();
        item["ts_ms"] = entry.ms;
        item["level"] = WebApiUtils::eventLevelText(entry.level);
        item["severity"] = WebApiUtils::eventSeverityText(entry.level);
        item["type"] = safe_tag(entry);
        item["message"] = safe_message(entry, "Event");
        emitted_count++;
    }
    return emitted_count;
}

} // namespace WebEventsUtils
