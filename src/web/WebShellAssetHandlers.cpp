// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebShellAssetHandlers.h"

#include "core/ConnectivityRuntime.h"
#include "web/WebTemplates.h"
#include "web/WebThemePage.h"
#include "web/WebUiBridge.h"

namespace WebShellAssetHandlers {

void handleThemeRoot(WebHandlerContext &context,
                     const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.web_ui_bridge || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    const WebUiBridge::Snapshot web_ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    switch (WebThemePage::rootAccess(connectivity.wifi_connected,
                                     web_ui_snapshot.theme_screen_open,
                                     web_ui_snapshot.theme_custom_screen_open)) {
    case WebThemePage::RootAccess::WifiRequired:
        context.server->send(403, "text/plain", "WiFi required");
        return;
    case WebThemePage::RootAccess::Locked: {
        String html = FPSTR(WebTemplates::kThemeLockedPage);
        WebResponseUtils::sendHtmlStream(*context.server, html, stream_context);
        return;
    }
    case WebThemePage::RootAccess::Ready:
        break;
    }
    WebResponseUtils::sendHtmlStreamProgmem(*context.server,
                                            WebTemplates::kThemeShellHtmlGzip,
                                            WebTemplates::kThemeShellHtmlGzipSize,
                                            true,
                                            stream_context);
}

void handleThemeStyles(WebHandlerContext &context,
                       const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "text/css; charset=utf-8",
                                       WebTemplates::kThemeStylesCssGzip,
                                       WebTemplates::kThemeStylesCssGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

void handleThemeApp(WebHandlerContext &context,
                    const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "application/javascript; charset=utf-8",
                                       WebTemplates::kThemeAppJsGzip,
                                       WebTemplates::kThemeAppJsGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

void handleDacRoot(WebHandlerContext &context,
                   const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.web_runtime) {
        return;
    }
    WebResponseUtils::sendHtmlStreamProgmem(*context.server,
                                            WebTemplates::kDacShellHtmlGzip,
                                            WebTemplates::kDacShellHtmlGzipSize,
                                            true,
                                            stream_context);
}

void handleDacStyles(WebHandlerContext &context,
                     const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "text/css; charset=utf-8",
                                       WebTemplates::kDacStylesCssGzip,
                                       WebTemplates::kDacStylesCssGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

void handleDacApp(WebHandlerContext &context,
                  const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "application/javascript; charset=utf-8",
                                       WebTemplates::kDacAppJsGzip,
                                       WebTemplates::kDacAppJsGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

}  // namespace WebShellAssetHandlers
