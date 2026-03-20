// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <ArduinoJson.h>

namespace WebJsonUtils {

void jsonSetFloatOrNull(ArduinoJson::JsonObject obj, const char *key, bool valid, float value);
void jsonSetIntOrNull(ArduinoJson::JsonObject obj, const char *key, bool valid, int value);

} // namespace WebJsonUtils
