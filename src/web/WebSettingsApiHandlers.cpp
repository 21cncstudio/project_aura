// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSettingsApiHandlers.h"

#include <ArduinoJson.h>

#include "core/Logger.h"
#include "web/WebResponseUtils.h"
#include "web/WebSettingsApiUtils.h"
#include "web/WebSettingsUtils.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace {

constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

void send_settings_error(WebRequest &server, int status_code, const char *message) {
    WebResponseUtils::sendNoStoreText(server, status_code, message);
}

}  // namespace

namespace WebSettingsApiHandlers {

void handleUpdate(WebHandlerContext &context,
                  bool ota_busy,
                  size_t display_name_max_len,
                  uint32_t deferred_restart_delay_ms,
                  OtaDeferredRestart::Controller &restart_controller) {
    if (!context.server) {
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    WebRequest &server = *context.server;
    if (!context.web_ui_bridge) {
        send_settings_error(server, 503, "UI bridge unavailable");
        return;
    }
    const WebUiBridge::Snapshot current_settings = context.web_ui_bridge->snapshot();
    if (!current_settings.available) {
        send_settings_error(server, 503, "UI bridge unavailable");
        return;
    }

    const WebSettingsApiUtils::ParseResult parse_result =
        WebSettingsApiUtils::parseUpdateRequestBody(server.arg("plain"),
                                                   WebUiBridgeAdapters::captureSettingsSnapshot(
                                                       current_settings),
                                                   context.storage != nullptr,
                                                   display_name_max_len);
    if (!parse_result.success) {
        send_settings_error(server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applySettings(
            WebUiBridgeAdapters::toUiSettingsUpdate(parse_result.update));
    if (!apply_result.success) {
        send_settings_error(server,
                            apply_result.status_code,
                            apply_result.error_message.isEmpty()
                                ? "Failed to apply settings"
                                : apply_result.error_message.c_str());
        return;
    }

    ArduinoJson::JsonDocument response_doc;
    const WebSettingsUtils::SettingsSnapshot applied_snapshot =
        WebUiBridgeAdapters::captureSettingsSnapshot(apply_result.snapshot);
    WebSettingsApiUtils::fillUpdateSuccessJson(
        response_doc.to<ArduinoJson::JsonObject>(),
        applied_snapshot,
        apply_result.restart_requested);

    String json;
    serializeJson(response_doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);

    if (apply_result.restart_requested) {
        restart_controller.schedule(millis(), deferred_restart_delay_ms);
        LOGI("Web", "settings restart requested, deferred reboot in %u ms",
             static_cast<unsigned>(deferred_restart_delay_ms));
    }
}

}  // namespace WebSettingsApiHandlers
