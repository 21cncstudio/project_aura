#pragma once
#include <Arduino.h>
typedef enum { WIFI_AUTH_OPEN=0, WIFI_AUTH_WPA2_PSK=3, WIFI_AUTH_WPA2_ENTERPRISE=5, WIFI_AUTH_WPA3_ENT_192=9 } wifi_auth_mode_t;
typedef enum { WIFI_MODE_NULL=0, WIFI_MODE_STA=1, WIFI_MODE_AP=2, WIFI_MODE_APSTA=3 } wifi_mode_t;
#define WIFI_OFF WIFI_MODE_NULL
#define WIFI_STA WIFI_MODE_STA
#define WIFI_AP WIFI_MODE_AP
#define WIFI_AP_STA WIFI_MODE_APSTA
typedef enum { WL_IDLE_STATUS=0, WL_NO_SSID_AVAIL=1, WL_CONNECTED=3, WL_CONNECT_FAILED=4, WL_DISCONNECTED=6 } wl_status_t;
typedef enum { WIFI_REASON_AUTH_EXPIRE=2, WIFI_REASON_AUTH_LEAVE=3, WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY=4, WIFI_REASON_NO_AP_FOUND=201 } wifi_err_reason_t;
typedef enum { ARDUINO_EVENT_WIFI_STA_START, ARDUINO_EVENT_WIFI_STA_STOP, ARDUINO_EVENT_WIFI_STA_GOT_IP, ARDUINO_EVENT_WIFI_STA_DISCONNECTED } arduino_event_id_t;
typedef struct { uint8_t ssid[33]; uint8_t ssid_len; uint8_t bssid[6]; uint8_t reason; int8_t rssi; } wifi_event_sta_disconnected_t;
typedef struct { wifi_event_sta_disconnected_t wifi_sta_disconnected; } arduino_event_info_t;
typedef int wifi_event_id_t;
typedef enum { WPA2_AUTH_PEAP, WPA2_AUTH_TTLS, WPA2_AUTH_TLS } wpa2_auth_method_t;
struct IPAddress { String toString() const { return String("192.168.4.1"); } };
struct MockSta { bool started(); void *netif(); };
struct MockWiFi {
    MockSta STA;
    void persistent(bool);
    wifi_event_id_t onEvent(void (*)(arduino_event_id_t, arduino_event_info_t));
    bool setHostname(const char *);
    bool mode(wifi_mode_t m);
    wifi_mode_t getMode();
    bool disconnect(bool wifioff = false, bool erase = false);
    bool softAP(const char *ssid);
    bool enableAP(bool on);
    IPAddress softAPIP() { return {}; }
    IPAddress localIP() { return {}; }
    void setAutoReconnect(bool);
    wl_status_t status();
    int8_t RSSI() { return -50; }
    wl_status_t begin(const char *ssid, const char *pass, int32_t channel, const uint8_t *bssid, bool connect);
    wl_status_t begin(const char *ssid, wpa2_auth_method_t, const char *, const char *, const char *, const char *, const char *, const char *, int, int32_t, const uint8_t *, bool);
    const char *disconnectReasonName(wifi_err_reason_t) { return "x"; }
    int scanNetworks(bool async, bool hidden);
    void scanDelete();
    String SSID(int) { return String(); }
    int32_t RSSI(int) { return 0; }
    const uint8_t *BSSID(int) { return nullptr; }
    int32_t channel(int) { return 0; }
};
extern MockWiFi WiFi;
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)
