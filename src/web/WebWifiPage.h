// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace WebWifiPage {

struct RootPageData {
    const char *ssid_items = "";
    bool scan_in_progress = false;
};

struct SavePageData {
    String hostname;
    uint16_t wait_seconds = 15;
};

String renderScanStatusJson(bool scan_in_progress);
String captivePortalRedirectUrl(const String &ap_ip);
// On failure out is empty; callers must send an explicit error response.
bool renderRootHtml(const char *html_template, const RootPageData &data, String &out);
bool renderSaveHtml(const char *html_template, const SavePageData &data, String &out);

} // namespace WebWifiPage
