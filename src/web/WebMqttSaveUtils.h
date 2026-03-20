// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace WebMqttSaveUtils {

struct CurrentCredentials {
    String user;
    String pass;
};

struct SaveInput {
    String host;
    String port;
    String user;
    String pass;
    String device_name;
    String base_topic;
    bool anonymous = false;
    bool discovery = false;
};

struct SaveUpdate {
    String host;
    uint16_t port = 1883;
    String user;
    String pass;
    String base_topic;
    String device_name;
    bool discovery = true;
    bool anonymous = false;
};

struct ParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    SaveUpdate update{};
};

ParseResult parseSaveInput(const SaveInput &input, const CurrentCredentials &current_credentials);

} // namespace WebMqttSaveUtils
