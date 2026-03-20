// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiSaveUtils.h"

#include "web/WebInputValidation.h"

namespace WebWifiSaveUtils {

ParseResult parseSaveInput(const String &ssid, const String &pass) {
    ParseResult result{};

    if (ssid.length() == 0) {
        result.status_code = 400;
        result.error_message = "SSID required";
        return result;
    }

    if (!WebInputValidation::isWifiSsidValid(ssid, WebInputValidation::kWifiSsidMaxBytes)) {
        result.status_code = 400;
        result.error_message = "SSID must be 1-32 bytes";
        return result;
    }

    if (WebInputValidation::hasControlChars(pass)) {
        result.status_code = 400;
        result.error_message = "Password contains unsupported control characters";
        return result;
    }

    result.success = true;
    result.status_code = 200;
    result.update.ssid = ssid;
    result.update.pass = pass;
    result.update.enabled = true;
    return result;
}

} // namespace WebWifiSaveUtils
