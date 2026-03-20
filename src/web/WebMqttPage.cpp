// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebMqttPage.h"

#include <stdio.h>
#include <string.h>

#include "config/AppConfig.h"
#include "web/WebTextUtils.h"

namespace WebMqttPage {

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

} // namespace

RootAccess rootAccess(bool wifi_connected, bool mqtt_screen_open) {
    if (!wifi_connected) {
        return RootAccess::NotFound;
    }
    if (!mqtt_screen_open) {
        return RootAccess::Locked;
    }
    return RootAccess::Ready;
}

StatusView statusFor(const PageData &data) {
    if (!data.mqtt_enabled) {
        return {"Disabled", "status-disconnected"};
    }
    if (!data.wifi_enabled || !data.wifi_connected) {
        return {"No WiFi", "status-error"};
    }
    if (data.mqtt_connected) {
        return {"Connected", "status-connected"};
    }
    if (data.mqtt_retry_stage == 0) {
        return {"Connecting", "status-error"};
    }
    if (data.mqtt_retry_stage == 1) {
        return {"Retrying", "status-error"};
    }
    return {"Delayed retry", "status-error"};
}

String renderHtml(const String &html_template, const PageData &data) {
    const StatusView status = statusFor(data);
    String html = html_template;

    html = replace_placeholder(html, "{{STATUS}}", String(status.text));
    html = replace_placeholder(html, "{{STATUS_CLASS}}", String(status.css_class));
    html = replace_placeholder(html, "{{DEVICE_ID}}", WebTextUtils::htmlEscape(data.device_id));
    html = replace_placeholder(html, "{{DEVICE_IP}}", WebTextUtils::htmlEscape(data.device_ip));
    html = replace_placeholder(html, "{{MQTT_HOST}}", WebTextUtils::htmlEscape(data.host));
    html = replace_placeholder(html, "{{MQTT_PORT}}",
                               uint_to_string(data.port == 0 ? Config::MQTT_DEFAULT_PORT
                                                             : data.port));
    html = replace_placeholder(html, "{{MQTT_USER}}", WebTextUtils::htmlEscape(data.user));
    html = replace_placeholder(html, "{{MQTT_PASS}}", WebTextUtils::htmlEscape(data.pass));
    html = replace_placeholder(html, "{{MQTT_NAME}}", WebTextUtils::htmlEscape(data.device_name));
    html = replace_placeholder(html, "{{MQTT_TOPIC}}", WebTextUtils::htmlEscape(data.base_topic));
    html = replace_placeholder(html, "{{ANONYMOUS_CHECKED}}",
                               data.anonymous ? String("checked") : String(""));
    html = replace_placeholder(html, "{{DISCOVERY_CHECKED}}",
                               data.discovery ? String("checked") : String(""));

    return html;
}

} // namespace WebMqttPage
