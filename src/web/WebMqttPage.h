// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace WebMqttPage {

enum class RootAccess : uint8_t {
    Ready,
    Locked,
    NotFound,
};

struct StatusView {
    const char *text;
    const char *css_class;
};

struct PageData {
    bool wifi_connected = false;
    bool wifi_enabled = false;
    bool mqtt_enabled = false;
    bool mqtt_connected = false;
    uint8_t mqtt_retry_stage = 0;
    String device_id;
    String device_ip;
    String host;
    uint16_t port = 0;
    String user;
    String pass;
    String device_name;
    String base_topic;
    bool anonymous = false;
    bool discovery = false;
};

RootAccess rootAccess(bool wifi_connected, bool mqtt_screen_open);
StatusView statusFor(const PageData &data);
String renderHtml(const String &html_template, const PageData &data);

} // namespace WebMqttPage
