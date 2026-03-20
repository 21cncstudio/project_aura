// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebPortalHandlers.h"

#include <WiFi.h>

#include "core/ConnectivityRuntime.h"
#include "web/WebDashboardPage.h"
#include "web/WebTemplates.h"
#include "web/WebWifiPage.h"
#include "web/WebWifiScanUtils.h"

namespace {

constexpr size_t kWifiScanMaxItems = 15;

}  // namespace

namespace WebPortalHandlers {

void buildWifiScanItems(WebHandlerContext &context, int count) {
    if (!context.wifi_scan_options) {
        return;
    }
    context.wifi_scan_options->clear();
    if (count <= 0) {
        return;
    }
    WebWifiScanUtils::WifiScanRow rows[kWifiScanMaxItems];
    size_t row_count = 0;

    for (int i = 0; i < count; i++) {
        String ssid_raw = WiFi.SSID(i);
        const int rssi = WiFi.RSSI(i);
        const bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        WebWifiScanUtils::addOrReplaceBestNetwork(
            rows, row_count, kWifiScanMaxItems, ssid_raw, rssi, open);
    }

    WebWifiScanUtils::sortNetworksByRssiDesc(rows, row_count);
    *context.wifi_scan_options = WebWifiScanUtils::renderNetworkItemsHtml(rows, row_count);
}

void handleWifiRoot(WebHandlerContext &context,
                    const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!connectivity.wifi_ap_mode) {
        WebResponseUtils::sendNoStoreHeaders(*context.server);
        context.server->send(404, "text/plain", "Not found");
        return;
    }
    WebRequest &server = *context.server;

    if (server.hasArg("scan_status")) {
        const String json = WebWifiPage::renderScanStatusJson(connectivity.wifi_scan_in_progress);
        WebResponseUtils::sendNoStoreHeaders(server);
        server.send(200, "application/json", json);
        return;
    }

    if (server.hasArg("scan") && context.wifi_start_scan) {
        context.wifi_start_scan();
    }
    String list_items;
    if (connectivity.wifi_scan_in_progress) {
        list_items = FPSTR(WebTemplates::kWifiListScanning);
    } else if (!connectivity.wifi_scan_options.isEmpty()) {
        list_items = connectivity.wifi_scan_options;
    } else {
        list_items = FPSTR(WebTemplates::kWifiListEmpty);
    }
    WebWifiPage::RootPageData page_data{};
    page_data.ssid_items = list_items;
    page_data.scan_in_progress = connectivity.wifi_scan_in_progress;
    const String html =
        WebWifiPage::renderRootHtml(FPSTR(WebTemplates::kWifiPageTemplate), page_data);
    WebResponseUtils::sendHtmlStream(server, html, stream_context);
}

void handleDashboardRoot(WebHandlerContext &context,
                         const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();

    switch (WebDashboardPage::decideRootAction(connectivity.wifi_ap_mode,
                                               connectivity.wifi_connected,
                                               context.server->uri())) {
    case WebDashboardPage::RootAction::WifiPortal:
        handleWifiRoot(context, stream_context);
        return;
    case WebDashboardPage::RootAction::NotFound:
        context.server->send(404, "text/plain", "Not found");
        return;
    case WebDashboardPage::RootAction::Dashboard:
        break;
    }

    WebResponseUtils::sendHtmlStreamProgmem(*context.server,
                                            WebTemplates::kDashboardShellHtmlGzip,
                                            WebTemplates::kDashboardShellHtmlGzipSize,
                                            true,
                                            stream_context);
}

void handleDashboardStyles(WebHandlerContext &context,
                           const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "text/css; charset=utf-8",
                                       WebTemplates::kDashboardStylesCssGzip,
                                       WebTemplates::kDashboardStylesCssGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

void handleDashboardApp(WebHandlerContext &context,
                        const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server) {
        return;
    }
    WebResponseUtils::sendProgmemAsset(*context.server,
                                       "application/javascript; charset=utf-8",
                                       WebTemplates::kDashboardAppJsGzip,
                                       WebTemplates::kDashboardAppJsGzipSize,
                                       true,
                                       WebResponseUtils::AssetCacheMode::Immutable,
                                       stream_context);
}

void handleWifiNotFound(WebHandlerContext &context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (connectivity.wifi_ap_mode) {
        const String portal_url = WebWifiPage::captivePortalRedirectUrl(connectivity.ap_ip);
        WebResponseUtils::sendNoStoreHeaders(*context.server);
        context.server->sendHeader("Location", portal_url, true);
        context.server->send(302, "text/plain", "Redirecting to captive portal");
        return;
    }

    context.server->send(404, "text/plain", "Not found");
}

}  // namespace WebPortalHandlers
