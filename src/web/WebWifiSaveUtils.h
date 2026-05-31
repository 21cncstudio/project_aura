// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "config/AppConfig.h"

namespace WebWifiSaveUtils {

struct SaveInput {
    String ssid;
    String pass;
    String auth_mode;
    String eap_method;
    String ttls_phase2;
    String identity;
    String username;
    String enterprise_password;
    String ca_cert_pem;
    String client_cert_pem;
    String client_key_pem;
};

struct SaveUpdate {
    Config::WifiSettings settings{};
};

struct ParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    SaveUpdate update{};
};

ParseResult parseSaveInput(const String &ssid, const String &pass);
ParseResult parseSaveInput(const SaveInput &input);

} // namespace WebWifiSaveUtils
