// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace WebWifiScanUtils {

struct WifiScanRow {
    String ssid;
    int rssi = 0;
    int quality = 0;
    bool open = false;
};

void addOrReplaceBestNetwork(WifiScanRow *rows,
                             size_t &row_count,
                             size_t max_rows,
                             const String &ssid_raw,
                             int rssi,
                             bool open);

void sortNetworksByRssiDesc(WifiScanRow *rows, size_t row_count);
String renderNetworkItemsHtml(const WifiScanRow *rows, size_t row_count);

} // namespace WebWifiScanUtils
