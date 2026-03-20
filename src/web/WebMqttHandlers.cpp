// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebMqttHandlers.h"

#include "config/AppConfig.h"
#include "core/ConnectivityRuntime.h"
#include "web/WebMqttPage.h"
#include "web/WebMqttSaveUtils.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace WebMqttHandlers {

void handleRoot(WebHandlerContext &context,
                const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    const WebUiBridge::Snapshot web_ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    switch (WebMqttPage::rootAccess(connectivity.wifi_connected,
                                    web_ui_snapshot.mqtt_screen_open)) {
        case WebMqttPage::RootAccess::NotFound:
            context.server->send(404, "text/plain", "Not found");
            return;
        case WebMqttPage::RootAccess::Locked: {
            String html = FPSTR(WebTemplates::kMqttLockedPage);
            WebResponseUtils::sendHtmlStreamResilient(*context.server, html, stream_context);
            return;
        }
        case WebMqttPage::RootAccess::Ready:
            break;
    }

    WebMqttPage::PageData page_data;
    page_data.wifi_connected = connectivity.wifi_connected;
    page_data.wifi_enabled = connectivity.wifi_enabled;
    page_data.mqtt_enabled = connectivity.mqtt_user_enabled;
    page_data.mqtt_connected = connectivity.mqtt_connected;
    page_data.mqtt_retry_stage = connectivity.mqtt_retry_stage;
    page_data.device_id = connectivity.mqtt_device_id;
    page_data.device_ip = connectivity.wifi_connected ? connectivity.sta_ip : String("---");
    page_data.host = connectivity.mqtt_host;
    page_data.port = connectivity.mqtt_port == 0 ? Config::MQTT_DEFAULT_PORT : connectivity.mqtt_port;
    page_data.user = connectivity.mqtt_user;
    page_data.pass = connectivity.mqtt_pass;
    page_data.device_name = connectivity.mqtt_device_name;
    page_data.base_topic = connectivity.mqtt_base_topic;
    page_data.anonymous = connectivity.mqtt_anonymous;
    page_data.discovery = connectivity.mqtt_discovery;

    const String html = WebMqttPage::renderHtml(FPSTR(WebTemplates::kMqttPageTemplate), page_data);
    WebResponseUtils::sendHtmlStreamResilient(*context.server, html, stream_context);
}

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
    if (!connectivity.wifi_connected) {
        WebResponseUtils::sendNoStoreText(server, 404, "Not found");
        return;
    }
    const WebUiBridge::Snapshot web_ui_snapshot =
        context.web_ui_bridge->snapshot();
    if (!web_ui_snapshot.mqtt_screen_open) {
        WebResponseUtils::sendNoStoreText(server, 409, "Open MQTT screen to enable");
        return;
    }
    if (ota_busy) {
        WebResponseUtils::sendNoStoreText(server, 503, "OTA in progress");
        return;
    }

    WebMqttSaveUtils::SaveInput save_input{};
    save_input.host = server.arg("host");
    save_input.port = server.arg("port");
    save_input.user = server.arg("user");
    save_input.pass = server.arg("pass");
    save_input.device_name = server.arg("name");
    save_input.base_topic = server.arg("topic");
    save_input.anonymous = server.hasArg("anonymous");
    save_input.discovery = server.hasArg("discovery");

    WebMqttSaveUtils::CurrentCredentials current_credentials{};
    current_credentials.user = connectivity.mqtt_user;
    current_credentials.pass = connectivity.mqtt_pass;

    const WebMqttSaveUtils::ParseResult parse_result =
        WebMqttSaveUtils::parseSaveInput(save_input, current_credentials);
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applyMqttSave(
            WebUiBridgeAdapters::toUiMqttSaveUpdate(parse_result.update));
    if (!apply_result.success) {
        WebResponseUtils::sendNoStoreText(
            server,
            apply_result.status_code,
            apply_result.error_message.isEmpty()
                ? "Failed to persist MQTT settings"
                : apply_result.error_message.c_str());
        return;
    }

    String html = FPSTR(WebTemplates::kMqttSavePage);
    WebResponseUtils::sendHtmlStream(server, html, stream_context);
    deferred_actions.scheduleMqttSync(millis(), deferred_action_delay_ms);
}

}  // namespace WebMqttHandlers
