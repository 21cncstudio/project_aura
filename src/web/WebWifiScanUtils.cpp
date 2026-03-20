// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiScanUtils.h"

#include "web/WebTextUtils.h"

namespace WebWifiScanUtils {

namespace {

String int_to_string(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return String(buf);
}

} // namespace

void addOrReplaceBestNetwork(WifiScanRow *rows,
                             size_t &row_count,
                             size_t max_rows,
                             const String &ssid_raw,
                             int rssi,
                             bool open) {
    if (!rows || max_rows == 0 || ssid_raw.length() == 0) {
        return;
    }

    const int quality = WebTextUtils::wifiRssiToQuality(rssi);

    for (size_t i = 0; i < row_count; ++i) {
        if (rows[i].ssid != ssid_raw) {
            continue;
        }
        if (rssi > rows[i].rssi) {
            rows[i].rssi = rssi;
            rows[i].quality = quality;
            rows[i].open = open;
        }
        return;
    }

    if (row_count < max_rows) {
        rows[row_count].ssid = ssid_raw;
        rows[row_count].rssi = rssi;
        rows[row_count].quality = quality;
        rows[row_count].open = open;
        ++row_count;
        return;
    }

    size_t weakest_index = 0;
    for (size_t i = 1; i < row_count; ++i) {
        if (rows[i].rssi < rows[weakest_index].rssi) {
            weakest_index = i;
        }
    }
    if (rssi <= rows[weakest_index].rssi) {
        return;
    }

    rows[weakest_index].ssid = ssid_raw;
    rows[weakest_index].rssi = rssi;
    rows[weakest_index].quality = quality;
    rows[weakest_index].open = open;
}

void sortNetworksByRssiDesc(WifiScanRow *rows, size_t row_count) {
    if (!rows || row_count < 2) {
        return;
    }

    for (size_t i = 1; i < row_count; ++i) {
        size_t j = i;
        while (j > 0 && rows[j].rssi > rows[j - 1].rssi) {
            const WifiScanRow tmp = rows[j - 1];
            rows[j - 1] = rows[j];
            rows[j] = tmp;
            --j;
        }
    }
}

String renderNetworkItemsHtml(const WifiScanRow *rows, size_t row_count) {
    String html;
    if (!rows || row_count == 0) {
        return html;
    }

    html.reserve(row_count * 170);
    for (size_t i = 0; i < row_count; ++i) {
        const String ssid_label = WebTextUtils::wifiLabelSafe(rows[i].ssid);
        const String ssid_html = WebTextUtils::htmlEscape(ssid_label);
        const char *security = rows[i].open ? "Open" : "Secure";
        const String rssi_text = int_to_string(rows[i].rssi) + " dBm";

        html += "<div class=\"network-item\" data-ssid=\"";
        html += ssid_html;
        html += "\"><div class=\"network-icon\" aria-hidden=\"true\"></div>";
        html += "<div class=\"network-info\"><span class=\"network-name\">";
        html += ssid_html;
        html += "</span><span class=\"network-meta\">";
        html += security;
        html += " | ";
        html += rssi_text;
        html += "</span></div><div class=\"network-signal\">";
        html += int_to_string(rows[i].quality);
        html += "%</div></div>";
    }
    return html;
}

} // namespace WebWifiScanUtils
