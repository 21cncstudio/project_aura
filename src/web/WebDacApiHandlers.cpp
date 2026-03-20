// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDacApiHandlers.h"

#include <ArduinoJson.h>

#include "core/WebRuntimeState.h"
#include "web/WebDacApiUtils.h"
#include "web/WebResponseUtils.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace {

void send_ota_busy_if_needed(WebRequest &server, bool ota_busy, const char *ota_busy_json) {
    if (!ota_busy) {
        return;
    }
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", ota_busy_json);
}

}  // namespace

namespace WebDacApiHandlers {

void handleState(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server || !context.web_runtime) {
        return;
    }
    WebRequest &server = *context.server;
    if (ota_busy) {
        send_ota_busy_if_needed(server, ota_busy, ota_busy_json);
        return;
    }

    const WebRuntimeSnapshot runtime = context.web_runtime->snapshot();
    const FanControl::Snapshot &fan = runtime.fan;
    const SensorData &data = runtime.data;

    WebDacApiUtils::StatePayload payload{};
    payload.now_ms = millis();
    payload.gas_warmup = runtime.gas_warmup;
    payload.dac.available = fan.available;
    payload.dac.faulted = fan.faulted;
    payload.dac.running = fan.running;
    payload.dac.manual_override = fan.manual_override_active;
    payload.dac.auto_resume_blocked = fan.auto_resume_blocked;
    payload.dac.output_known = fan.output_known;
    payload.dac.auto_mode = (fan.mode == FanControl::Mode::Auto);
    payload.dac.manual_step = fan.manual_step;
    payload.dac.selected_timer_s = fan.selected_timer_s;
    payload.dac.output_mv = fan.output_mv;
    payload.dac.stop_at_ms = fan.stop_at_ms;
    payload.dac.auto_config = fan.auto_config;
    payload.sensors.co2 = data.co2;
    payload.sensors.co2_valid = data.co2_valid;
    payload.sensors.co_ppm = data.co_ppm;
    payload.sensors.co_valid = data.co_valid && data.co_sensor_present;
    payload.sensors.pm05 = data.pm05;
    payload.sensors.pm05_valid = data.pm05_valid;
    payload.sensors.pm1 = data.pm1;
    payload.sensors.pm1_valid = data.pm1_valid;
    payload.sensors.pm4 = data.pm4;
    payload.sensors.pm4_valid = data.pm4_valid;
    payload.sensors.pm25 = data.pm25;
    payload.sensors.pm25_valid = data.pm25_valid;
    payload.sensors.pm10 = data.pm10;
    payload.sensors.pm10_valid = data.pm10_valid;
    payload.sensors.hcho = data.hcho;
    payload.sensors.hcho_valid = data.hcho_valid;
    payload.sensors.voc_index = data.voc_index;
    payload.sensors.voc_valid = data.voc_valid;
    payload.sensors.nox_index = data.nox_index;
    payload.sensors.nox_valid = data.nox_valid;

    ArduinoJson::JsonDocument doc;
    WebDacApiUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(), payload);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

void handleAction(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server || !context.web_ui_bridge) {
        return;
    }
    WebRequest &server = *context.server;
    if (ota_busy) {
        send_ota_busy_if_needed(server, ota_busy, ota_busy_json);
        return;
    }

    const WebDacApiUtils::DacActionParseResult parse_result =
        WebDacApiUtils::parseActionRequestBody(server.arg("plain"));
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applyDacAction(
            WebUiBridgeAdapters::toUiDacActionUpdate(parse_result.update));
    if (!apply_result.success) {
        WebResponseUtils::sendNoStoreText(server,
                                          apply_result.status_code,
                                          apply_result.error_message.isEmpty()
                                              ? "Failed to apply DAC action"
                                              : apply_result.error_message.c_str());
        return;
    }

    ArduinoJson::JsonDocument doc;
    WebDacApiUtils::fillSuccessJson(doc.to<ArduinoJson::JsonObject>());
    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

void handleAuto(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server || !context.web_runtime || !context.web_ui_bridge) {
        return;
    }
    WebRequest &server = *context.server;
    if (ota_busy) {
        send_ota_busy_if_needed(server, ota_busy, ota_busy_json);
        return;
    }

    const WebRuntimeSnapshot runtime = context.web_runtime->snapshot();
    const WebDacApiUtils::DacAutoParseResult parse_result =
        WebDacApiUtils::parseAutoRequestBody(server.arg("plain"), runtime.fan.auto_config);
    if (!parse_result.success) {
        WebResponseUtils::sendNoStoreText(
            server, parse_result.status_code, parse_result.error_message.c_str());
        return;
    }

    const WebUiBridge::ApplyResult apply_result =
        context.web_ui_bridge->applyDacAuto(
            WebUiBridgeAdapters::toUiDacAutoUpdate(parse_result.update));
    if (!apply_result.success) {
        WebResponseUtils::sendNoStoreText(server,
                                          apply_result.status_code,
                                          apply_result.error_message.isEmpty()
                                              ? "Failed to apply auto config"
                                              : apply_result.error_message.c_str());
        return;
    }

    ArduinoJson::JsonDocument doc;
    WebDacApiUtils::fillSuccessJson(doc.to<ArduinoJson::JsonObject>());
    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

}  // namespace WebDacApiHandlers
