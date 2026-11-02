// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiPage.h"

#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace WebWifiPage {
namespace {

// The native unit suite aliases String to std::string (void reserve); the
// SDK integration suite uses real Arduino String and exercises allocation failure.
template <typename Text>
bool reserve_checked(Text &text, size_t size) {
    if constexpr (std::is_void<decltype(text.reserve(size))>::value) {
        text.reserve(size);
        return true;
    } else {
        return text.reserve(size);
    }
}

struct Replacement { const char *token; const char *value; };

// Measure first and allocate once. Only tokens from the original template are
// interpreted: an SSID containing another token must remain literal text.
bool render(const char *source, const Replacement (&replacements)[2], String &out) {
    out = "";
    if (!source || !*source) return false;
    size_t expected = 0;
    unsigned seen = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        for (const char *pos = source; *pos;) {
            const char *part = pos;
            size_t length = 1;
            size_t consumed = 1;
            for (unsigned i = 0; i < 2; ++i) {
                const auto &r = replacements[i];
                if (strncmp(pos, r.token, strlen(r.token)) == 0) {
                    if (!r.value) { out = ""; return false; }
                    seen |= 1U << i;
                    part = r.value;
                    length = strlen(part);
                    consumed = strlen(r.token);
                    break;
                }
            }
            if (pass == 0) {
                if (length > SIZE_MAX - expected) return false;
                expected += length;
            } else {
                for (size_t i = 0; i < length; ++i) out += part[i];
            }
            pos += consumed;
        }
        if (pass == 0 && (seen != 3 || !reserve_checked(out, expected))) {
            out = "";
            return false;
        }
    }
    if (out.length() != expected) { out = ""; return false; }
    return true;
}

bool dashboard_url(const String &hostname, String &out) {
    const char *name = hostname.length() ? hostname.c_str() : "aura";
    // The network hostname is bounded, but check arithmetic before reserving.
    const size_t length = strlen(name);
    if (length > (SIZE_MAX - 24) / 6 || !reserve_checked(out, length * 6 + 24)) return false;
    out = "http://";
    size_t expected = 7;
    for (; *name; ++name) {
        const char *escaped = nullptr;
        switch (*name) {
            case '&': escaped = "&amp;"; break;
            case '<': escaped = "&lt;"; break;
            case '>': escaped = "&gt;"; break;
            case '"': escaped = "&quot;"; break;
            case '\'': escaped = "&#39;"; break;
            default: out += *name; ++expected; break;
        }
        if (escaped) { out += escaped; expected += strlen(escaped); }
    }
    out += ".local/dashboard";
    return out.length() == expected + strlen(".local/dashboard");
}
} // namespace

String renderScanStatusJson(bool scan_in_progress) {
    return scan_in_progress ? "{\"success\":true,\"scan_in_progress\":true}"
                            : "{\"success\":true,\"scan_in_progress\":false}";
}

String captivePortalRedirectUrl(const String &ap_ip) {
    const bool has_ap_ip = ap_ip.length() > 0 && ap_ip != "0.0.0.0";
    String url = "http://";
    url += has_ap_ip ? ap_ip : String("192.168.4.1");
    url += "/";
    return url;
}

bool renderRootHtml(const char *html_template, const RootPageData &data, String &out) {
    const Replacement replacements[] = {{"{{SSID_ITEMS}}", data.ssid_items},
                                        {"{{SCAN_IN_PROGRESS}}", data.scan_in_progress ? "1" : "0"}};
    return render(html_template, replacements, out);
}

bool renderSaveHtml(const char *html_template, const SavePageData &data, String &out) {
    out = "";
    String url;
    if (!dashboard_url(data.hostname, url)) return false;
    char seconds[8];
    snprintf(seconds, sizeof(seconds), "%u", data.wait_seconds ? data.wait_seconds : 15);
    const Replacement replacements[] = {{"{{HOSTNAME_DASHBOARD_URL}}", url.c_str()},
                                        {"{{WAIT_SECONDS}}", seconds}};
    return render(html_template, replacements, out);
}
} // namespace WebWifiPage
