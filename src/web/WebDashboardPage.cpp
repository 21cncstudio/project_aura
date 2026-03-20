// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDashboardPage.h"

namespace WebDashboardPage {

RootAction decideRootAction(bool ap_mode, bool wifi_connected, const String &uri) {
    if (ap_mode && uri == "/") {
        return RootAction::WifiPortal;
    }
    if (!ap_mode && !wifi_connected) {
        return RootAction::NotFound;
    }
    return RootAction::Dashboard;
}

} // namespace WebDashboardPage
