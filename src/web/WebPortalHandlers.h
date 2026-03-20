// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "web/WebContext.h"
#include "web/WebResponseUtils.h"

namespace WebPortalHandlers {

void buildWifiScanItems(WebHandlerContext &context, int count);
void handleWifiRoot(WebHandlerContext &context, const WebResponseUtils::StreamContext &stream_context);
void handleDashboardRoot(WebHandlerContext &context,
                         const WebResponseUtils::StreamContext &stream_context);
void handleDashboardStyles(WebHandlerContext &context,
                           const WebResponseUtils::StreamContext &stream_context);
void handleDashboardApp(WebHandlerContext &context,
                        const WebResponseUtils::StreamContext &stream_context);
void handleWifiNotFound(WebHandlerContext &context);

}  // namespace WebPortalHandlers
