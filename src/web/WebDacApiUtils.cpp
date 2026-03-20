// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDacApiUtils.h"

#include "web/WebDacUtils.h"

namespace WebDacApiUtils {

namespace {

template <typename TResult, typename TParseFn>
TResult parse_request_body(const String &body, TParseFn parse_fn) {
    TResult result{};
    if (body.length() == 0) {
        result.success = false;
        result.status_code = 400;
        result.error_message = "Missing body";
        return result;
    }

    ArduinoJson::JsonDocument doc;
    const ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, body);
    if (err) {
        result.success = false;
        result.status_code = 400;
        result.error_message = "Invalid JSON";
        return result;
    }

    return parse_fn(doc.as<ArduinoJson::JsonVariantConst>());
}

} // namespace

void fillStateJson(ArduinoJson::JsonObject root, const StatePayload &payload) {
    root["success"] = true;

    const DacStateSnapshot &dac_snapshot = payload.dac;
    ArduinoJson::JsonObject dac = root["dac"].to<ArduinoJson::JsonObject>();
    dac["available"] = dac_snapshot.available;
    dac["faulted"] = dac_snapshot.faulted;
    dac["running"] = dac_snapshot.running;
    dac["manual_override"] = dac_snapshot.manual_override;
    dac["auto_resume_blocked"] = dac_snapshot.auto_resume_blocked;
    dac["output_known"] = dac_snapshot.output_known;
    dac["mode"] = dac_snapshot.auto_mode ? "auto" : "manual";
    dac["manual_step"] = dac_snapshot.manual_step;
    dac["selected_timer_s"] = dac_snapshot.selected_timer_s;
    dac["remaining_s"] =
        WebDacUtils::remainingSeconds(dac_snapshot.running, dac_snapshot.stop_at_ms, payload.now_ms);
    dac["output_mv"] = dac_snapshot.output_mv;
    dac["output_percent"] = WebDacUtils::outputPercent(dac_snapshot.output_mv);
    dac["status"] = WebDacUtils::statusText(dac_snapshot.available,
                                            dac_snapshot.faulted,
                                            dac_snapshot.running);

    ArduinoJson::JsonObject auto_cfg = root["auto"].to<ArduinoJson::JsonObject>();
    DacAutoConfigJson::writeJson(auto_cfg, dac_snapshot.auto_config);

    const SensorSnapshot &sensor_snapshot = payload.sensors;
    ArduinoJson::JsonObject sensors = root["sensors"].to<ArduinoJson::JsonObject>();
    sensors["gas_warmup"] = payload.gas_warmup;
    sensors["co2"] = sensor_snapshot.co2;
    sensors["co2_valid"] = sensor_snapshot.co2_valid;
    sensors["co_ppm"] = sensor_snapshot.co_ppm;
    sensors["co_valid"] = sensor_snapshot.co_valid;
    sensors["pm05"] = sensor_snapshot.pm05;
    sensors["pm05_valid"] = sensor_snapshot.pm05_valid;
    sensors["pm1"] = sensor_snapshot.pm1;
    sensors["pm1_valid"] = sensor_snapshot.pm1_valid;
    sensors["pm4"] = sensor_snapshot.pm4;
    sensors["pm4_valid"] = sensor_snapshot.pm4_valid;
    sensors["pm25"] = sensor_snapshot.pm25;
    sensors["pm25_valid"] = sensor_snapshot.pm25_valid;
    sensors["pm10"] = sensor_snapshot.pm10;
    sensors["pm10_valid"] = sensor_snapshot.pm10_valid;
    sensors["hcho"] = sensor_snapshot.hcho;
    sensors["hcho_valid"] = sensor_snapshot.hcho_valid;
    sensors["voc_index"] = sensor_snapshot.voc_index;
    sensors["voc_valid"] = sensor_snapshot.voc_valid;
    sensors["nox_index"] = sensor_snapshot.nox_index;
    sensors["nox_valid"] = sensor_snapshot.nox_valid;
}

DacActionParseResult parseActionRequestBody(const String &body) {
    return parse_request_body<DacActionParseResult>(
        body,
        [](ArduinoJson::JsonVariantConst root) {
            return parseActionUpdate(root);
        });
}

DacActionParseResult parseActionUpdate(ArduinoJson::JsonVariantConst root) {
    DacActionParseResult result{};

    const String action = String(root["action"] | "");
    if (action == "set_mode") {
        const String mode = String(root["mode"] | "");
        if (mode == "manual") {
            result.success = true;
            result.update.type = DacActionUpdate::Type::SetMode;
            result.update.auto_mode = false;
            return result;
        }
        if (mode == "auto") {
            result.success = true;
            result.update.type = DacActionUpdate::Type::SetMode;
            result.update.auto_mode = true;
            return result;
        }
        result.status_code = 400;
        result.error_message = "Invalid mode";
        return result;
    }

    if (action == "set_manual_step") {
        result.success = true;
        result.update.type = DacActionUpdate::Type::SetManualStep;
        result.update.manual_step = static_cast<uint8_t>(root["step"] | 1);
        return result;
    }

    if (action == "set_timer") {
        const ArduinoJson::JsonVariantConst seconds = root["seconds"];
        uint32_t timer_seconds = 0;
        if ((!seconds.is<int>() && !seconds.is<unsigned int>()) ||
            !WebDacUtils::normalizeTimerSeconds(seconds.as<int32_t>(), timer_seconds)) {
            result.status_code = 400;
            result.error_message = "Invalid timer value";
            return result;
        }
        result.success = true;
        result.update.type = DacActionUpdate::Type::SetTimerSeconds;
        result.update.timer_seconds = timer_seconds;
        return result;
    }

    if (action == "start") {
        result.success = true;
        result.update.type = DacActionUpdate::Type::Start;
        return result;
    }

    if (action == "stop") {
        result.success = true;
        result.update.type = DacActionUpdate::Type::Stop;
        return result;
    }

    if (action == "start_auto") {
        result.success = true;
        result.update.type = DacActionUpdate::Type::StartAuto;
        return result;
    }

    result.status_code = 400;
    result.error_message = "Unsupported action";
    return result;
}

DacAutoParseResult parseAutoRequestBody(const String &body, const DacAutoConfig &current_config) {
    return parse_request_body<DacAutoParseResult>(
        body,
        [&current_config](ArduinoJson::JsonVariantConst root) {
            return parseAutoUpdate(root, current_config);
        });
}

DacAutoParseResult parseAutoUpdate(ArduinoJson::JsonVariantConst root,
                                   const DacAutoConfig &current_config) {
    DacAutoParseResult result{};

    ArduinoJson::JsonObjectConst source = root.as<ArduinoJson::JsonObjectConst>();
    if (root["auto"].is<ArduinoJson::JsonObjectConst>()) {
        source = root["auto"].as<ArduinoJson::JsonObjectConst>();
    }

    DacAutoConfig config = current_config;
    if (!DacAutoConfigJson::readJson(source, config)) {
        result.status_code = 400;
        result.error_message = "Invalid auto payload";
        return result;
    }

    result.success = true;
    result.update.config = config;
    result.update.rearm = root["rearm"] | false;
    return result;
}

void fillSuccessJson(ArduinoJson::JsonObject root) {
    root["success"] = true;
}

} // namespace WebDacApiUtils
