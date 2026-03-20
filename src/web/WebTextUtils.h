// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace WebTextUtils {

String htmlEscape(const String &input);
int wifiRssiToQuality(int rssi);
bool hasControlChars(const String &value);
bool mqttTopicHasWildcards(const String &topic);
bool parsePositiveSize(const String &value, size_t &out);
String wifiLabelSafe(const String &value);

} // namespace WebTextUtils
