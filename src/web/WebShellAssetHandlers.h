// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "web/WebContext.h"
#include "web/WebResponseUtils.h"

namespace WebShellAssetHandlers {

void handleThemeRoot(WebHandlerContext &context,
                     const WebResponseUtils::StreamContext &stream_context);
void handleThemeStyles(WebHandlerContext &context,
                       const WebResponseUtils::StreamContext &stream_context);
void handleThemeApp(WebHandlerContext &context,
                    const WebResponseUtils::StreamContext &stream_context);
void handleDacRoot(WebHandlerContext &context,
                   const WebResponseUtils::StreamContext &stream_context);
void handleDacStyles(WebHandlerContext &context,
                     const WebResponseUtils::StreamContext &stream_context);
void handleDacApp(WebHandlerContext &context,
                  const WebResponseUtils::StreamContext &stream_context);

}  // namespace WebShellAssetHandlers
