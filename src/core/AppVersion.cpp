// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "AppVersion.h"

#if __has_include("AppBuildId.generated.h")
#include "AppBuildId.generated.h"
#endif

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

#ifndef APP_BUILD_ID
#define APP_BUILD_ID "nogit"
#endif

#ifndef APP_HARDWARE_PROFILE
#define APP_HARDWARE_PROFILE "unknown"
#endif

namespace {

bool isNumeric(char c) {
    return c >= '0' && c <= '9';
}

} // namespace

namespace AppVersion {

bool isStableRelease() {
    const char *version = APP_VERSION;
    bool saw_digit = false;
    bool saw_dot = false;

    for (; *version; ++version) {
        if (isNumeric(*version)) {
            saw_digit = true;
            continue;
        }
        if (*version == '.') {
            saw_dot = true;
            continue;
        }
        return false;
    }

    return saw_digit && saw_dot;
}

const char *shortVersion() {
    return APP_VERSION;
}

const char *buildId() {
    return APP_BUILD_ID;
}

const char *hardwareProfile() {
    return APP_HARDWARE_PROFILE;
}

const char *fullVersion() {
    if (isStableRelease()) {
        return APP_VERSION;
    }
    return APP_VERSION "-" APP_BUILD_ID;
}

} // namespace AppVersion
