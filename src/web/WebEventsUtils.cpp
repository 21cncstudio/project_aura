// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebEventsUtils.h"

#include "core/SystemEventPolicy.h"
#include "web/WebApiUtils.h"

namespace WebEventsUtils {

size_t fillRecentErrorsJson(ArduinoJson::JsonArray errors,
                            const Logger::RecentEntry *entries,
                            size_t count,
                            size_t max_items) {
    if (!entries || max_items == 0) {
        return 0;
    }

    // Snapshots are stored oldest to newest. Locate the newest eligible tail
    // first, then emit that tail in snapshot order. Do not sort by uptime:
    // entry.ms can wrap while the snapshot order remains authoritative.
    size_t first = count;
    size_t selected = 0;
    while (first > 0 && selected < max_items) {
        const Logger::RecentEntry &entry = entries[--first];
        if (entry.level == Logger::Error || entry.level == Logger::Warn) {
            ++selected;
        }
    }

    size_t added = 0;
    for (size_t i = first; i < count; ++i) {
        const Logger::RecentEntry &entry = entries[i];
        if (entry.level != Logger::Error && entry.level != Logger::Warn) {
            continue;
        }
        ArduinoJson::JsonObject item = errors.add<ArduinoJson::JsonObject>();
        item["ts_ms"] = entry.ms;
        item["level"] = WebApiUtils::eventLevelText(entry.level);
        item["tag"] = SystemEventPolicy::typeText(entry);
        item["message"] = entry.message[0] ? entry.message : "";
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
        item["type"] = SystemEventPolicy::typeText(entry);
        item["message"] = SystemEventPolicy::messageText(entry);
        emitted_count++;
    }
    return emitted_count;
}

} // namespace WebEventsUtils
