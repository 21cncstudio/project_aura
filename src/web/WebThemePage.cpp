// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebThemePage.h"

namespace WebThemePage {

RootAccess rootAccess(bool wifi_ready, bool theme_screen_open, bool theme_custom_screen_open) {
    if (!wifi_ready) {
        return RootAccess::WifiRequired;
    }
    if (!theme_screen_open || !theme_custom_screen_open) {
        return RootAccess::Locked;
    }
    return RootAccess::Ready;
}

} // namespace WebThemePage
