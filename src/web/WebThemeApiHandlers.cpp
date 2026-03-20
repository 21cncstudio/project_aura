// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebThemeApiHandlers.h"

#include <ArduinoJson.h>

#include "core/ConnectivityRuntime.h"
#include "web/WebResponseUtils.h"
#include "web/WebThemeApiUtils.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace {

void send_theme_error_json(WebRequest &server, int status_code, const char *message) {
    ArduinoJson::JsonDocument doc;
    WebThemeApiUtils::fillErrorJson(doc.to<ArduinoJson::JsonObject>(), message);
    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(status_code, "application/json", json);
}

}  // namespace

namespace WebThemeApiHandlers {

void handleState(WebHandlerContext &context) {
    if (!context.server || !context.web_ui_bridge || !context.connectivity_runtime) {
        return;
    }

    WebRequest &server = *context.server;
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    const bool wifi_ready = connectivity.wifi_connected;
    const WebUiBridge::Snapshot web_ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    const WebThemeApiUtils::ApiAccessResult access_result =
        WebThemeApiUtils::validateApiAccess(
            wifi_ready, web_ui_snapshot.theme_screen_open, web_ui_snapshot.theme_custom_screen_open);
    if (!access_result.success) {
        send_theme_error_json(server, access_result.status_code, access_result.error_message.c_str());
        return;
    }

    ArduinoJson::JsonDocument doc;
    WebThemeApiUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(),
                                    web_ui_snapshot.theme_preview_colors);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

void handleApply(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server || !context.web_ui_bridge || !context.connectivity_runtime) {
        return;
    }

    WebRequest &server = *context.server;
    if (ota_busy) {
        WebResponseUtils::sendNoStoreHeaders(server);
        server.send(503, "application/json", ota_busy_json);
        return;
    }

    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    const bool wifi_ready = connectivity.wifi_connected;
    const WebUiBridge::Snapshot web_ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    const WebThemeApiUtils::ApiAccessResult access_result =
        WebThemeApiUtils::validateApiAccess(
            wifi_ready, web_ui_snapshot.theme_screen_open, web_ui_snapshot.theme_custom_screen_open);
    if (!access_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, access_result.status_code, access_result.error_message.c_str());
        return;
    }

    const WebThemeApiUtils::ParseResult parse_result =
        WebThemeApiUtils::parseApplyRequestBody(server.arg("plain"),
                                               web_ui_snapshot.theme_preview_colors);
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applyTheme(
            WebUiBridgeAdapters::toUiThemeUpdate(parse_result.colors));
    if (!apply_result.success) {
        WebResponseUtils::sendNoStoreText(server,
                                          apply_result.status_code,
                                          apply_result.error_message.isEmpty()
                                              ? "Failed to apply theme"
                                              : apply_result.error_message.c_str());
        return;
    }

    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "text/plain", "OK");
}

}  // namespace WebThemeApiHandlers
