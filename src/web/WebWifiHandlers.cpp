// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiHandlers.h"

#include "core/ConnectivityRuntime.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"
#include "web/WebWifiPage.h"
#include "web/WebWifiSaveUtils.h"

namespace WebWifiHandlers {

void handleSave(WebHandlerContext &context,
                bool ota_busy,
                uint32_t deferred_action_delay_ms,
                WebDeferredActionsState &deferred_actions,
                const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.web_ui_bridge || !context.connectivity_runtime) {
        return;
    }

    WebRequest &server = *context.server;
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!connectivity.wifi_ap_mode) {
        WebResponseUtils::sendNoStoreText(server, 409, "WiFi save allowed only in AP setup mode");
        return;
    }
    if (ota_busy) {
        WebResponseUtils::sendNoStoreText(server, 503, "OTA in progress");
        return;
    }

    const WebWifiSaveUtils::ParseResult parse_result =
        WebWifiSaveUtils::parseSaveInput(server.arg("ssid"), server.arg("pass"));
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applyWifiSave(
            WebUiBridgeAdapters::toUiWifiSaveUpdate(parse_result.update));
    if (!apply_result.success) {
        WebResponseUtils::sendNoStoreText(
            server,
            apply_result.status_code,
            apply_result.error_message.isEmpty()
                ? "Failed to persist WiFi settings"
                : apply_result.error_message.c_str());
        return;
    }

    WebWifiPage::SavePageData page_data{};
    page_data.hostname = connectivity.hostname.isEmpty() ? String("aura") : connectivity.hostname;
    page_data.wait_seconds = 15;
    const String html = WebWifiPage::renderSaveHtml(FPSTR(WebTemplates::kWifiSavePage), page_data);
    WebResponseUtils::sendHtmlStream(server, html, stream_context);
    deferred_actions.scheduleWifiStartSta(millis(), deferred_action_delay_ms);
}

}  // namespace WebWifiHandlers
