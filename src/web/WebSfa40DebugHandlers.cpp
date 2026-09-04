// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSfa40DebugHandlers.h"

#include <math.h>

#include <ArduinoJson.h>

#include "core/AppVersion.h"
#include "core/ConnectivityRuntime.h"
#include "core/WebRuntimeState.h"
#include "modules/SensorManager.h"
#include "web/WebDiagApiUtils.h"
#include "web/WebResponseUtils.h"

namespace {

const char *bool_text(bool value) {
    return value ? "yes" : "no";
}

const char *status_text(Sfa40::Status status) {
    switch (status) {
        case Sfa40::Status::Absent:
            return "absent";
        case Sfa40::Status::Ok:
            return "ok";
        case Sfa40::Status::Fault:
            return "fault";
    }
    return "unknown";
}

const char *hcho_type_text(SensorManager::HchoSensorType type) {
    switch (type) {
        case SensorManager::HCHO_SENSOR_SFA30:
            return "SFA30";
        case SensorManager::HCHO_SENSOR_SFA40:
            return "SFA40";
        case SensorManager::HCHO_SENSOR_NONE:
            return "none";
    }
    return "unknown";
}

String serial_text(const Sfa40::Diagnostics &diag) {
    if (!diag.serial_valid || diag.serial_word_count == 0U ||
        diag.serial_word_count > 5U) {
        return "n/a";
    }
    String out;
    out.reserve(static_cast<unsigned>(diag.serial_word_count) * 5U);
    for (size_t i = 0; i < diag.serial_word_count; ++i) {
        if (i != 0U) {
            out += '-';
        }
        char word[5];
        snprintf(word,
                 sizeof(word),
                 "%04X",
                 static_cast<unsigned>(diag.serial_words[i]));
        out += word;
    }
    return out;
}

String seconds_text(uint32_t millis_value) {
    if (millis_value == 0) {
        return "n/a";
    }
    return String(millis_value / 1000UL) + " s";
}

String seconds_after_start_text(uint32_t start_ms, uint32_t event_ms) {
    if (start_ms == 0 || event_ms == 0) {
        return "n/a";
    }
    return String((event_ms - start_ms) / 1000UL) + " s after start";
}

String float_text(bool valid, float value, unsigned decimals, const char *unit) {
    if (!valid || !isfinite(value)) {
        return "n/a";
    }
    String out(value, decimals);
    if (unit && unit[0] != '\0') {
        out += ' ';
        out += unit;
    }
    return out;
}

String hex_byte_text(uint8_t value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned>(value));
    return String(buf);
}

void append_line(String &out, const char *label, const String &value) {
    out += label;
    out += ": ";
    out += value;
    out += '\n';
}

void append_line(String &out, const char *label, const char *value) {
    append_line(out, label, String(value ? value : ""));
}

void append_line(String &out, const char *label, uint32_t value) {
    append_line(out, label, String(value));
}

bool access_allowed(WebHandlerContext &context) {
    if (!context.connectivity_runtime) {
        return false;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    return WebDiagApiUtils::accessAllowed(connectivity.wifi_ap_mode, connectivity.wifi_connected);
}

String build_report(WebHandlerContext &context) {
    const WebRuntimeSnapshot runtime =
        context.web_runtime ? context.web_runtime->snapshot() : WebRuntimeSnapshot{};
    const SensorData &data = runtime.data;
    SensorManager &manager = *context.sensor_manager;
    const SensorManager::HchoSensorType hcho_type = manager.hchoSensorType();
    const Sfa40::Diagnostics diag = manager.sfa40Diagnostics();
    const bool active_sfa40 = hcho_type == SensorManager::HCHO_SENSOR_SFA40;
    const bool not_ready = (diag.status_byte & 0x01U) != 0U;
    const bool not_within_spec = (diag.status_byte & 0x02U) != 0U;

    String out;
    out.reserve(4096);
    out += "Project Aura SFA40 Diagnostics\n";
    out += "================================\n\n";
    append_line(out, "Firmware", AppVersion::fullVersion());
    append_line(out, "Uptime", seconds_text(millis()));
    append_line(out, "HCHO sensor label", manager.hchoSensorLabel());
    append_line(out, "HCHO sensor type", hcho_type_text(hcho_type));
    append_line(out, "Active SFA40 path", bool_text(active_sfa40));
    out += '\n';

    out += "Displayed HCHO state\n";
    out += "--------------------\n";
    append_line(out, "HCHO sensor present", bool_text(data.hcho_sensor_present));
    append_line(out, "HCHO warmup", bool_text(data.hcho_warmup));
    append_line(out, "HCHO valid", bool_text(data.hcho_valid));
    append_line(out,
                "Displayed HCHO",
                float_text(data.hcho_valid, data.hcho, 1, "ppb"));
    out += '\n';

    out += "SFA40 state\n";
    out += "-----------\n";
    append_line(out, "Status", status_text(diag.status));
    append_line(out, "Warmup active", bool_text(diag.warmup_active));
    append_line(out, "Measuring", bool_text(diag.measuring));
    append_line(out, "Measurement state unknown", bool_text(diag.measurement_state_unknown));
    append_line(out, "Self-test active", bool_text(diag.selftest_active));
    append_line(out, "Last error", diag.last_error);
    append_line(out, "Protocol", Sfa40::protocolLabel(diag.protocol));
    append_line(out, "Serial", serial_text(diag));
    append_line(out, "Started", seconds_text(diag.start_ms));
    append_line(out, "First ready", seconds_after_start_text(diag.start_ms, diag.first_ready_ms));
    append_line(out, "First within spec", seconds_after_start_text(diag.start_ms, diag.first_within_spec_ms));
    append_line(out, "Last measurement", seconds_text(diag.last_measurement_ms));
    append_line(out, "Last valid data", seconds_text(diag.last_data_ms));
    out += '\n';

    out += "Last SFA40 measurement frame\n";
    out += "----------------------------\n";
    append_line(out, "Status valid", bool_text(diag.last_status_valid));
    append_line(out, "Status byte", hex_byte_text(diag.status_byte));
    append_line(out, "Status reserved byte", hex_byte_text(diag.status_reserved));
    append_line(out, "Status not ready", bool_text(not_ready));
    append_line(out, "Status not within spec", bool_text(not_within_spec));
    append_line(out, "Raw HCHO word", String(diag.raw_hcho));
    append_line(out, "Raw RH word", String(diag.raw_humidity));
    append_line(out, "Raw T word", String(diag.raw_temperature));
    append_line(out,
                "Direct HCHO",
                float_text(diag.last_status_valid, static_cast<float>(diag.raw_hcho) / 10.0f, 1, "ppb"));
    append_line(out,
                "SFA40 local RH",
                float_text(diag.last_status_valid, diag.humidity_percent, 1, "%"));
    append_line(out,
                "SFA40 local temperature",
                float_text(diag.last_status_valid, diag.temperature_c, 1, "C"));
    out += '\n';

    out += "Communication counters\n";
    out += "----------------------\n";
    append_line(out, "Measurement read attempts", diag.measurement_reads);
    append_line(out, "Measurement frames OK", diag.measurement_frames_ok);
    append_line(out, "Measurement data OK", diag.measurement_data_ok);
    append_line(out, "Measurement read failures", diag.measurement_read_failures);
    append_line(out, "Read command errors", diag.read_command_errors);
    append_line(out, "Read byte errors", diag.read_bytes_errors);
    append_line(out, "CRC errors", diag.read_crc_errors);
    append_line(out, "Status not-ready count", diag.status_not_ready_count);
    append_line(out, "Status not-within-spec count", diag.status_not_within_spec_count);
    append_line(out, "Invalid status count", diag.status_invalid_count);
    out += '\n';

    out += "Reference sensors\n";
    out += "-----------------\n";
    append_line(out,
                "SEN66 temperature",
                float_text(data.temp_valid, data.temperature, 1, "C"));
    append_line(out,
                "SEN66 humidity",
                float_text(data.hum_valid, data.humidity, 1, "%"));
    return out;
}

void append_escaped_html(String &out, const String &text) {
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text.charAt(i);
        switch (c) {
            case '&':
                out += F("&amp;");
                break;
            case '<':
                out += F("&lt;");
                break;
            case '>':
                out += F("&gt;");
                break;
            case '"':
                out += F("&quot;");
                break;
            default:
                out += c;
                break;
        }
    }
}

void fill_json(ArduinoJson::JsonObject root, WebHandlerContext &context) {
    const WebRuntimeSnapshot runtime =
        context.web_runtime ? context.web_runtime->snapshot() : WebRuntimeSnapshot{};
    const SensorData &data = runtime.data;
    SensorManager &manager = *context.sensor_manager;
    const SensorManager::HchoSensorType hcho_type = manager.hchoSensorType();
    const Sfa40::Diagnostics diag = manager.sfa40Diagnostics();

    root["success"] = true;
    root["firmware"] = AppVersion::fullVersion();
    root["uptime_s"] = millis() / 1000UL;
    root["hcho_sensor_label"] = manager.hchoSensorLabel();
    root["hcho_sensor_type"] = hcho_type_text(hcho_type);
    root["active_sfa40"] = hcho_type == SensorManager::HCHO_SENSOR_SFA40;

    ArduinoJson::JsonObject displayed = root["displayed"].to<ArduinoJson::JsonObject>();
    displayed["hcho_sensor_present"] = data.hcho_sensor_present;
    displayed["hcho_warmup"] = data.hcho_warmup;
    displayed["hcho_valid"] = data.hcho_valid;
    if (data.hcho_valid) {
        displayed["hcho_ppb"] = data.hcho;
    } else {
        displayed["hcho_ppb"] = nullptr;
    }
    if (data.temp_valid) {
        displayed["sen66_temperature_c"] = data.temperature;
    } else {
        displayed["sen66_temperature_c"] = nullptr;
    }
    if (data.hum_valid) {
        displayed["sen66_humidity_percent"] = data.humidity;
    } else {
        displayed["sen66_humidity_percent"] = nullptr;
    }

    ArduinoJson::JsonObject sfa = root["sfa40"].to<ArduinoJson::JsonObject>();
    sfa["status"] = status_text(diag.status);
    sfa["warmup_active"] = diag.warmup_active;
    sfa["measuring"] = diag.measuring;
    sfa["measurement_state_unknown"] = diag.measurement_state_unknown;
    sfa["data_valid"] = diag.data_valid;
    sfa["has_new_data"] = diag.has_new_data;
    sfa["selftest_active"] = diag.selftest_active;
    sfa["last_error"] = diag.last_error;
    sfa["protocol"] = Sfa40::protocolLabel(diag.protocol);
    sfa["serial_valid"] = diag.serial_valid;
    sfa["serial_word_count"] = diag.serial_word_count;
    sfa["serial"] = serial_text(diag);
    ArduinoJson::JsonArray serial_words =
        sfa["serial_words"].to<ArduinoJson::JsonArray>();
    for (size_t i = 0; i < diag.serial_word_count && i < 5U; ++i) {
        serial_words.add(diag.serial_words[i]);
    }
    sfa["start_ms"] = diag.start_ms;
    sfa["first_ready_ms"] = diag.first_ready_ms;
    sfa["first_within_spec_ms"] = diag.first_within_spec_ms;
    sfa["last_measurement_ms"] = diag.last_measurement_ms;
    sfa["last_data_ms"] = diag.last_data_ms;

    ArduinoJson::JsonObject frame = sfa["last_frame"].to<ArduinoJson::JsonObject>();
    frame["status_valid"] = diag.last_status_valid;
    frame["raw_hcho"] = diag.raw_hcho;
    frame["raw_humidity"] = diag.raw_humidity;
    frame["raw_temperature"] = diag.raw_temperature;
    frame["status_byte"] = diag.status_byte;
    frame["status_reserved"] = diag.status_reserved;
    frame["status_not_ready"] = (diag.status_byte & 0x01U) != 0U;
    frame["status_not_within_spec"] = (diag.status_byte & 0x02U) != 0U;
    if (diag.last_status_valid) {
        frame["hcho_ppb"] = static_cast<float>(diag.raw_hcho) / 10.0f;
        frame["humidity_percent"] = diag.humidity_percent;
        frame["temperature_c"] = diag.temperature_c;
    } else {
        frame["hcho_ppb"] = nullptr;
        frame["humidity_percent"] = nullptr;
        frame["temperature_c"] = nullptr;
    }

    ArduinoJson::JsonObject counters = sfa["counters"].to<ArduinoJson::JsonObject>();
    counters["measurement_reads"] = diag.measurement_reads;
    counters["measurement_frames_ok"] = diag.measurement_frames_ok;
    counters["measurement_data_ok"] = diag.measurement_data_ok;
    counters["measurement_read_failures"] = diag.measurement_read_failures;
    counters["read_command_errors"] = diag.read_command_errors;
    counters["read_bytes_errors"] = diag.read_bytes_errors;
    counters["read_crc_errors"] = diag.read_crc_errors;
    counters["status_not_ready_count"] = diag.status_not_ready_count;
    counters["status_not_within_spec_count"] = diag.status_not_within_spec_count;
    counters["status_invalid_count"] = diag.status_invalid_count;
}

}  // namespace

namespace WebSfa40DebugHandlers {

void handleRoot(WebHandlerContext &context) {
    if (!context.server || !context.sensor_manager) {
        return;
    }
    if (!access_allowed(context)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }

    const String report = build_report(context);
    String html;
    html.reserve(report.length() + 1400);
    html += F("<!doctype html><html><head><meta charset=\"utf-8\">"
              "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
              "<title>SFA40 Diagnostics</title>"
              "<style>body{margin:0;background:#0f172a;color:#e5e7eb;font:14px/1.45 ui-monospace,SFMono-Regular,Consolas,monospace}"
              "main{max-width:980px;margin:0 auto;padding:20px}"
              "h1{font:600 22px/1.2 system-ui,sans-serif;margin:0 0 12px}"
              ".actions{display:flex;gap:8px;flex-wrap:wrap;margin:0 0 14px}"
              "button,a{background:#1f2937;color:#e5e7eb;border:1px solid #334155;border-radius:6px;padding:8px 10px;text-decoration:none;cursor:pointer}"
              "button:hover,a:hover{background:#273449}"
              "pre{white-space:pre-wrap;background:#020617;border:1px solid #1e293b;border-radius:8px;padding:14px;overflow:auto}</style></head>"
              "<script>function copyReport(){var t=document.getElementById('report').innerText;"
              "if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t);return;}"
              "var a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();"
              "try{document.execCommand('copy')}catch(e){}document.body.removeChild(a);}</script>"
              "<body><main><h1>SFA40 Diagnostics</h1><div class=\"actions\">"
              "<button onclick=\"copyReport()\">Copy report</button>"
              "<button onclick=\"location.reload()\">Refresh</button>"
              "<a href=\"/api/debug/sfa40\">JSON</a></div><pre id=\"report\">");
    append_escaped_html(html, report);
    html += F("</pre></main></body></html>");

    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "text/html", html);
}

void handleData(WebHandlerContext &context) {
    if (!context.server || !context.sensor_manager) {
        return;
    }
    if (!access_allowed(context)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }

    ArduinoJson::JsonDocument doc;
    fill_json(doc.to<ArduinoJson::JsonObject>(), context);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

}  // namespace WebSfa40DebugHandlers
