// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebHandlers.h"

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "config/AppConfig.h"
#include "modules/StorageManager.h"
#include "web/WebTemplates.h"
#include "ui/ThemeManager.h"

namespace {

WebHandlerContext *g_ctx = nullptr;

WebHandlerContext *ctx() {
    return g_ctx;
}

String wifi_html_escape(const String &input) {
    String out;
    out.reserve(input.length());
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

int wifi_rssi_to_quality(int rssi) {
    if (rssi <= -100) {
        return 0;
    }
    if (rssi >= -50) {
        return 100;
    }
    return 2 * (rssi + 100);
}

String theme_color_to_hex(lv_color_t color) {
    lv_color32_t c32;
    c32.full = lv_color_to32(color);
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", c32.ch.red, c32.ch.green, c32.ch.blue);
    return String(buf);
}

bool parse_hex_color(const String &value, lv_color_t &out) {
    String s = value;
    s.trim();
    if (s.startsWith("#")) {
        s = s.substring(1);
    }
    if (s.length() != 6) {
        return false;
    }
    char *end = nullptr;
    long rgb = strtol(s.c_str(), &end, 16);
    if (!end || *end != '\0') {
        return false;
    }
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    out = lv_color_hex((r << 16) | (g << 8) | b);
    return true;
}

} // namespace

void WebHandlersInit(WebHandlerContext *context) {
    g_ctx = context;
}

bool wifi_is_ascii_printable(const String &value, size_t max_len) {
    size_t len = value.length();
    if (len == 0 || len > max_len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        uint8_t c = static_cast<uint8_t>(value[i]);
        if (c < 32 || c > 126) {
            return false;
        }
    }
    return true;
}

String wifi_label_safe(const String &value) {
    if (value.isEmpty()) {
        return "---";
    }
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); i++) {
        uint8_t c = static_cast<uint8_t>(value[i]);
        if (c >= 32 && c <= 126) {
            out += static_cast<char>(c);
        } else {
            out += '?';
        }
    }
    return out;
}

void wifi_build_scan_items(int count) {
    WebHandlerContext *context = ctx();
    if (!context || !context->wifi_scan_options) {
        return;
    }
    context->wifi_scan_options->clear();
    if (count <= 0) {
        return;
    }
    context->wifi_scan_options->reserve(count * 220);
    for (int i = 0; i < count; i++) {
        String ssid_raw = WiFi.SSID(i);
        if (ssid_raw.isEmpty()) {
            continue;
        }
        String ssid_label = wifi_label_safe(ssid_raw);
        String ssid_html = wifi_html_escape(ssid_label);
        int quality = wifi_rssi_to_quality(WiFi.RSSI(i));
        bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        const char *security = open ? "Open" : "Secure";

        (*context->wifi_scan_options) += "<div class=\"network-item\" data-ssid=\"";
        (*context->wifi_scan_options) += ssid_html;
        (*context->wifi_scan_options) += "\"><div class=\"network-icon\">";
        (*context->wifi_scan_options) += FPSTR(WebTemplates::kWifiIconSvg);
        (*context->wifi_scan_options) += "</div><div class=\"network-info\"><span class=\"network-name\">";
        (*context->wifi_scan_options) += ssid_html;
        (*context->wifi_scan_options) += "</span><span class=\"network-meta\">";
        (*context->wifi_scan_options) += security;
        (*context->wifi_scan_options) += "</span></div><div class=\"network-signal\">";
        (*context->wifi_scan_options) += String(quality);
        (*context->wifi_scan_options) += "%</div></div>";
    }
}

void wifi_handle_root() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server) {
        return;
    }
    WebServer &server = *context->server;
    if (server.hasArg("scan") && context->wifi_start_scan) {
        context->wifi_start_scan();
    }
    String list_items;
    if (context->wifi_scan_in_progress && *context->wifi_scan_in_progress) {
        list_items = FPSTR(WebTemplates::kWifiListScanning);
    } else if (context->wifi_scan_options && !context->wifi_scan_options->isEmpty()) {
        list_items = *context->wifi_scan_options;
    } else {
        list_items = FPSTR(WebTemplates::kWifiListEmpty);
    }
    String html = FPSTR(WebTemplates::kWifiPageTemplate);
    html.reserve(html.length() + list_items.length() + 64);
    html.replace("{{SSID_ITEMS}}", list_items);
    html.replace("{{SCAN_IN_PROGRESS}}",
                 (context->wifi_scan_in_progress && *context->wifi_scan_in_progress) ? "1" : "0");
    server.send(200, "text/html", html);
}

void wifi_handle_save() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server || !context->storage) {
        return;
    }
    WebServer &server = *context->server;
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    pass.trim();
    if (ssid.isEmpty()) {
        server.send(400, "text/plain", "SSID required");
        return;
    }
    if (!wifi_is_ascii_printable(ssid, 32)) {
        server.send(400, "text/plain", "SSID must be ASCII (32 chars max)");
        return;
    }

    context->storage->saveWiFiSettings(ssid, pass, true);
    if (context->wifi_ssid) *context->wifi_ssid = ssid;
    if (context->wifi_pass) *context->wifi_pass = pass;
    if (context->wifi_enabled) *context->wifi_enabled = true;
    if (context->wifi_enabled_dirty) *context->wifi_enabled_dirty = false;
    if (context->wifi_ui_dirty) *context->wifi_ui_dirty = true;

    String html = FPSTR(WebTemplates::kWifiSavePage);
    server.send(200, "text/html", html);
    delay(200);
    if (context->wifi_start_sta) {
        context->wifi_start_sta();
    }
}

void wifi_handle_not_found() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server) {
        return;
    }
    context->server->send(404, "text/plain", "Not found");
}

void mqtt_handle_root() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server || !context->mqtt_client) {
        return;
    }
    if (!context->mqtt_ui_open || !*context->mqtt_ui_open) {
        String html = FPSTR(WebTemplates::kMqttLockedPage);
        context->server->send(200, "text/html", html);
        return;
    }
    WebServer &server = *context->server;
    PubSubClient &client = *context->mqtt_client;

    bool wifi_connected = context->wifi_is_connected ? context->wifi_is_connected() : false;
    bool wifi_enabled = context->wifi_enabled ? *context->wifi_enabled : false;
    bool mqtt_enabled = context->mqtt_user_enabled ? *context->mqtt_user_enabled : false;
    uint8_t mqtt_fail_count = context->mqtt_connect_fail_count ? *context->mqtt_connect_fail_count : 0;

    String status_text;
    String status_class;
    if (!mqtt_enabled) {
        status_text = "Disabled";
        status_class = "status-disconnected";
    } else if (!wifi_enabled || !wifi_connected) {
        status_text = "No WiFi";
        status_class = "status-error";
    } else if (client.connected()) {
        status_text = "Connected";
        status_class = "status-connected";
    } else if (mqtt_fail_count < Config::MQTT_CONNECT_MAX_FAILS) {
        status_text = "Connecting";
        status_class = "status-error";
    } else {
        status_text = "Error";
        status_class = "status-error";
    }

    String device_ip = "---";
    if (wifi_connected) {
        device_ip = WiFi.localIP().toString();
    }

    String mqtt_user = context->mqtt_user ? *context->mqtt_user : String();
    String mqtt_pass = context->mqtt_pass ? *context->mqtt_pass : String();
    bool is_anonymous = false;
    if (context->mqtt_anonymous) {
        is_anonymous = *context->mqtt_anonymous;
    } else {
        is_anonymous = mqtt_user.isEmpty() && mqtt_pass.isEmpty();
    }
    String anonymous_checked = is_anonymous ? "checked" : "";
    bool discovery = context->mqtt_discovery ? *context->mqtt_discovery : false;
    String discovery_checked = discovery ? "checked" : "";

    String html = FPSTR(WebTemplates::kMqttPageTemplate);
    html.reserve(html.length() + 512);

    html.replace("{{STATUS}}", status_text);
    html.replace("{{STATUS_CLASS}}", status_class);
    html.replace("{{DEVICE_ID}}", context->mqtt_device_id ? *context->mqtt_device_id : String());
    html.replace("{{DEVICE_IP}}", device_ip);
    html.replace("{{MQTT_HOST}}", context->mqtt_host ? *context->mqtt_host : String());
    html.replace("{{MQTT_PORT}}",
                 String(context->mqtt_port ? *context->mqtt_port : Config::MQTT_DEFAULT_PORT));
    html.replace("{{MQTT_USER}}", mqtt_user);
    html.replace("{{MQTT_PASS}}", mqtt_pass);
    html.replace("{{MQTT_NAME}}", context->mqtt_device_name ? *context->mqtt_device_name : String());
    html.replace("{{MQTT_TOPIC}}", context->mqtt_base_topic ? *context->mqtt_base_topic : String());
    html.replace("{{ANONYMOUS_CHECKED}}", anonymous_checked);
    html.replace("{{DISCOVERY_CHECKED}}", discovery_checked);

    server.send(200, "text/html", html);
}

void mqtt_handle_save() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server || !context->storage || !context->mqtt_client) {
        return;
    }
    WebServer &server = *context->server;
    PubSubClient &client = *context->mqtt_client;
    if (!context->mqtt_ui_open || !*context->mqtt_ui_open) {
        server.send(409, "text/plain", "Open MQTT screen to enable");
        return;
    }

    if (server.hasArg("test")) {
        bool success = client.connected();
        String json = success ? "{\"success\":true}" : "{\"success\":false}";
        server.send(200, "application/json", json);
        return;
    }

    String host = server.arg("host");
    String port_str = server.arg("port");
    String user = server.arg("user");
    String pass = server.arg("pass");
    String name = server.arg("name");
    String topic = server.arg("topic");
    bool anonymous = server.hasArg("anonymous");
    bool discovery = server.hasArg("discovery");

    host.trim();
    port_str.trim();
    user.trim();
    pass.trim();
    name.trim();
    topic.trim();

    if (host.isEmpty()) {
        server.send(400, "text/plain", "Broker address required");
        return;
    }

    if (name.isEmpty()) {
        server.send(400, "text/plain", "Device name required");
        return;
    }

    if (topic.isEmpty()) {
        server.send(400, "text/plain", "Base topic required");
        return;
    }

    uint16_t port = port_str.toInt();
    if (port == 0 || port > 65535) {
        port = Config::MQTT_DEFAULT_PORT;
    }

    if (!anonymous && (user.isEmpty() || pass.isEmpty())) {
        server.send(400, "text/plain",
                    "Username and password are required when anonymous mode is disabled");
        return;
    }

    if (anonymous) {
        if (user.isEmpty() && context->mqtt_user) {
            user = *context->mqtt_user;
        }
        if (pass.isEmpty() && context->mqtt_pass) {
            pass = *context->mqtt_pass;
        }
    }

    if (topic.endsWith("/")) {
        topic.remove(topic.length() - 1);
    }

    context->storage->saveMqttSettings(host, port, user, pass, topic, name, discovery, anonymous);

    if (context->mqtt_host) *context->mqtt_host = host;
    if (context->mqtt_port) *context->mqtt_port = port;
    if (context->mqtt_user) *context->mqtt_user = user;
    if (context->mqtt_pass) *context->mqtt_pass = pass;
    if (context->mqtt_device_name) *context->mqtt_device_name = name;
    if (context->mqtt_base_topic) *context->mqtt_base_topic = topic;
    if (context->mqtt_discovery) *context->mqtt_discovery = discovery;
    if (context->mqtt_anonymous) *context->mqtt_anonymous = anonymous;

    String html = FPSTR(WebTemplates::kMqttSavePage);
    server.send(200, "text/html", html);

    delay(200);
    if (context->mqtt_sync_with_wifi) {
        context->mqtt_sync_with_wifi();
    }
}

void theme_handle_root() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server || !context->theme_manager) {
        return;
    }
    bool wifi_ready = context->wifi_is_connected && context->wifi_is_connected();
    if (!wifi_ready) {
        context->server->send(403, "text/plain", "WiFi required");
        return;
    }
    if (!context->theme_manager->isCustomScreenOpen()) {
        String html = FPSTR(WebTemplates::kThemeLockedPage);
        context->server->send(200, "text/html", html);
        return;
    }
    ThemeColors colors = context->theme_manager->previewOrCurrent();
    String html = FPSTR(WebTemplates::kThemePageTemplate);
    html.replace("{{BG_COLOR}}", theme_color_to_hex(colors.screen_bg));
    html.replace("{{CARD_TOP}}", theme_color_to_hex(colors.card_bg));
    html.replace("{{CARD_BOTTOM}}", theme_color_to_hex(colors.gradient_color));
    html.replace("{{CARD_BORDER}}", theme_color_to_hex(colors.card_border));
    html.replace("{{SHADOW_COLOR}}", theme_color_to_hex(colors.shadow_color));
    html.replace("{{TEXT_COLOR}}", theme_color_to_hex(colors.text_primary));
    html.replace("{{CARD_GRADIENT_BOOL}}", colors.gradient_enabled ? "true" : "false");
    context->server->send(200, "text/html", html);
}

void theme_handle_apply() {
    WebHandlerContext *context = ctx();
    if (!context || !context->server || !context->theme_manager) {
        return;
    }
    WebServer &server = *context->server;
    bool wifi_ready = context->wifi_is_connected && context->wifi_is_connected();
    if (!wifi_ready) {
        server.send(403, "text/plain", "WiFi required");
        return;
    }
    if (!context->theme_manager->isCustomScreenOpen()) {
        server.send(409, "text/plain", "Open Custom Theme screen to enable");
        return;
    }
    String body = server.arg("plain");
    if (body.isEmpty()) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    ArduinoJson::JsonDocument doc;
    ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, body);
    if (err) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    ThemeColors colors = context->theme_manager->previewOrCurrent();

    lv_color_t parsed = {};
    const char *bg = doc["bg"] | "";
    if (parse_hex_color(String(bg), parsed)) {
        colors.screen_bg = parsed;
        colors.screen_gradient_enabled = false;
        colors.screen_gradient_color = parsed;
        colors.screen_gradient_direction = LV_GRAD_DIR_NONE;
    }
    const char *card_top = doc["card_top"] | "";
    if (parse_hex_color(String(card_top), parsed)) {
        colors.card_bg = parsed;
    }
    const char *card_bottom = doc["card_bottom"] | "";
    if (parse_hex_color(String(card_bottom), parsed)) {
        colors.gradient_color = parsed;
    }
    const char *border = doc["border"] | "";
    if (parse_hex_color(String(border), parsed)) {
        colors.card_border = parsed;
    }
    const char *shadow = doc["shadow"] | "";
    if (parse_hex_color(String(shadow), parsed)) {
        colors.shadow_color = parsed;
    }
    const char *text = doc["text"] | "";
    if (parse_hex_color(String(text), parsed)) {
        colors.text_primary = parsed;
    }

    int gradient_enabled = doc["card_gradient"] | (colors.gradient_enabled ? 1 : 0);
    colors.gradient_enabled = gradient_enabled != 0;
    colors.gradient_direction = colors.gradient_enabled ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE;
    colors.shadow_enabled = true;

    lvgl_port_lock(-1);
    context->theme_manager->applyPreviewCustom(colors);
    lvgl_port_unlock();

    server.send(200, "text/plain", "OK");
}
