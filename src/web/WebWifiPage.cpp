// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiPage.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "web/WebTextUtils.h"

namespace WebWifiPage {

namespace {

String uint_to_string(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    return String(buf);
}

String replace_placeholder(const String &input, const char *needle, const String &replacement) {
    const char *src = input.c_str();
    if (!src || !needle || *needle == '\0') {
        return input;
    }

    const size_t needle_len = strlen(needle);
    String out;
    out.reserve(input.length() + replacement.length());

    const char *segment = src;
    while (*segment != '\0') {
        const char *match = strstr(segment, needle);
        if (!match) {
            while (*segment != '\0') {
                out += *segment++;
            }
            break;
        }

        while (segment < match) {
            out += *segment++;
        }
        out += replacement;
        segment = match + needle_len;
    }

    return out;
}

bool string_is_empty(const String &value) {
    return value.length() == 0;
}

String hostname_dashboard_url(const String &hostname) {
    const String safe_hostname = string_is_empty(hostname) ? String("aura") : hostname;
    String url = "http://";
    url += safe_hostname;
    url += ".local/dashboard";
    return url;
}

} // namespace

String renderScanStatusJson(bool scan_in_progress) {
    String json = "{\"success\":true,\"scan_in_progress\":";
    json += scan_in_progress ? "true}" : "false}";
    return json;
}

String captivePortalRedirectUrl(const String &ap_ip) {
    const bool has_ap_ip = ap_ip.length() > 0 && ap_ip != "0.0.0.0";
    String url = "http://";
    url += has_ap_ip ? ap_ip : String("192.168.4.1");
    url += "/";
    return url;
}

String renderRootHtml(const String &html_template, const RootPageData &data) {
    String html = html_template;
    html = replace_placeholder(html, "{{SSID_ITEMS}}", data.ssid_items);
    html = replace_placeholder(html,
                               "{{SCAN_IN_PROGRESS}}",
                               data.scan_in_progress ? String("1") : String("0"));
    return html;
}

String renderSaveHtml(const String &html_template, const SavePageData &data) {
    String html = html_template;
    html = replace_placeholder(html,
                               "{{HOSTNAME_DASHBOARD_URL}}",
                               WebTextUtils::htmlEscape(hostname_dashboard_url(data.hostname)));
    html = replace_placeholder(html,
                               "{{WAIT_SECONDS}}",
                               uint_to_string(data.wait_seconds == 0 ? 15 : data.wait_seconds));
    return html;
}

} // namespace WebWifiPage
