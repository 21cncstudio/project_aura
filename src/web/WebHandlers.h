#pragma once

#include <Arduino.h>

class WebServer;
class PubSubClient;
class StorageManager;
class ThemeManager;

struct WebHandlerContext {
    WebServer *server = nullptr;
    StorageManager *storage = nullptr;
    ThemeManager *theme_manager = nullptr;

    String *wifi_ssid = nullptr;
    String *wifi_pass = nullptr;
    bool *wifi_enabled = nullptr;
    bool *wifi_enabled_dirty = nullptr;
    bool *wifi_ui_dirty = nullptr;
    bool *wifi_scan_in_progress = nullptr;
    String *wifi_scan_options = nullptr;
    bool (*wifi_is_connected)() = nullptr;
    void (*wifi_start_scan)() = nullptr;
    void (*wifi_start_sta)() = nullptr;

    PubSubClient *mqtt_client = nullptr;
    bool *mqtt_user_enabled = nullptr;
    uint8_t *mqtt_connect_fail_count = nullptr;
    String *mqtt_host = nullptr;
    uint16_t *mqtt_port = nullptr;
    String *mqtt_user = nullptr;
    String *mqtt_pass = nullptr;
    String *mqtt_device_name = nullptr;
    String *mqtt_base_topic = nullptr;
    String *mqtt_device_id = nullptr;
    bool *mqtt_discovery = nullptr;
    bool *mqtt_anonymous = nullptr;
    void (*mqtt_sync_with_wifi)() = nullptr;
    bool *mqtt_ui_open = nullptr;
};

void WebHandlersInit(WebHandlerContext *context);

bool wifi_is_ascii_printable(const String &value, size_t max_len);
String wifi_label_safe(const String &value);
void wifi_build_scan_items(int count);

void wifi_handle_root();
void wifi_handle_save();
void wifi_handle_not_found();
void mqtt_handle_root();
void mqtt_handle_save();
void theme_handle_root();
void theme_handle_apply();
