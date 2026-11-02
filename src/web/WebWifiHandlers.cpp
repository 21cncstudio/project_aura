// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiHandlers.h"

#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"
#include "web/WebWifiPage.h"
#include "web/WebWifiSaveUtils.h"

namespace WebWifiHandlers {

void handleSave(WebHandlerContext &context,
                bool ota_busy,
                uint64_t receipt,
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

    WebWifiSaveUtils::SaveInput save_input{};
    save_input.ssid = server.arg("ssid");
    save_input.pass = server.arg("pass");
    save_input.auth_mode = server.arg("auth_mode");
    save_input.eap_method = server.arg("eap_method");
    save_input.ttls_phase2 = server.arg("ttls_phase2");
    save_input.identity = server.arg("identity");
    save_input.username = server.arg("username");
    save_input.enterprise_password = server.arg("enterprise_password");
    save_input.ca_cert_pem = server.arg("ca_cert_pem");
    save_input.client_cert_pem = server.arg("client_cert_pem");
    save_input.client_key_pem = server.arg("client_key_pem");
    const WebWifiSaveUtils::ParseResult parse_result =
        WebWifiSaveUtils::parseSaveInput(save_input);
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    WebWifiPage::SavePageData page_data{};
    page_data.hostname = connectivity.hostname;
    page_data.receipt = receipt;
    page_data.wait_seconds = 15;
    String html;
    if (!WebWifiPage::renderSaveHtml(WebTemplates::kWifiSavePage, page_data, html)) {
        Logger::log(Logger::Warn, "Web", "WiFi save page preparation failed");
        WebResponseUtils::sendNoStoreText(server, 503, "Unable to prepare WiFi setup. Please retry.");
        return;
    }

    deferred_actions.beginWifiSave(receipt);
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

    if (WebResponseUtils::sendHtmlStream(server, html, stream_context)) {
        deferred_actions.armWifiSave(receipt, millis());
    }
    // No timer fallback: a lost page must leave AP setup reachable.
}

void handleConfirm(WebHandlerContext &context, bool ota_busy, uint32_t delay_ms,
                   WebDeferredActionsState &deferred_actions) {
    if (!context.server || !context.connectivity_runtime) return;
    auto &server = *context.server;
    if (!context.connectivity_runtime->snapshot().wifi_ap_mode) {
        WebResponseUtils::sendNoStoreText(server, 409, "WiFi confirmation allowed only in AP setup mode");
        return;
    }
    if (ota_busy) { WebResponseUtils::sendNoStoreText(server, 503, "OTA in progress"); return; }
    const String text = server.arg("receipt");
    uint64_t receipt = 0;
    if (text.length() == 16) {
        for (unsigned i = 0; i < 16; ++i) {
            const char c = text[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { receipt = 0; break; }
            receipt = (receipt << 4) | (c <= '9' ? c - '0' : c - 'a' + 10);
        }
    }
    if (!deferred_actions.confirmWifiSave(receipt, millis(), delay_ms)) {
        WebResponseUtils::sendNoStoreText(server, 409, "Setup page expired. Return to WiFi setup and save again.");
        return;
    }
    WebResponseUtils::sendNoStoreText(server, 200,
        "Aura is connecting to your WiFi. Wait about 15 seconds, then open the IP shown on its screen.");
}

}  // namespace WebWifiHandlers
