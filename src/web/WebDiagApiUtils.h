// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Logger.h"
#include "web/WebNetworkUtils.h"
#include "web/WebStreamState.h"

namespace WebDiagApiUtils {

struct Payload {
    uint32_t uptime_s = 0;
    bool ota_busy = false;
    uint32_t heap_free = 0;
    uint32_t heap_min_free = 0;
    WebNetworkUtils::Snapshot network{};
    WebTransferSnapshot web_stream{};
};

bool accessAllowed(bool ap_mode, bool sta_connected);
void fillJson(ArduinoJson::JsonObject root,
              const Payload &payload,
              const Logger::RecentEntry *recent_errors,
              size_t recent_error_count,
              size_t max_error_items);

} // namespace WebDiagApiUtils
