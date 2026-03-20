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

namespace WebEventsApiUtils {

void fillJson(ArduinoJson::JsonObject root,
              const Logger::RecentEntry *entries,
              size_t count,
              uint32_t uptime_s);

} // namespace WebEventsApiUtils
