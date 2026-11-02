#pragma once
#include <Arduino.h>
typedef enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3, WIFI_AUTH_WPA2_ENTERPRISE = 5, WIFI_AUTH_WPA3_ENT_192 = 9 } wifi_auth_mode_t;
typedef enum { WIFI_MODE_NULL = 0, WIFI_MODE_STA = 1, WIFI_MODE_AP = 2, WIFI_MODE_APSTA = 3 } wifi_mode_t;
struct LabWiFi {
    wifi_mode_t mode = WIFI_MODE_APSTA;
    int scanNetworks(bool async);
    int scanComplete();
    void scanDelete();
    wifi_mode_t getMode() { return mode; }
    String SSID(int) { return String(); }
    int RSSI(int) { return 0; }
    wifi_auth_mode_t encryptionType(int) { return WIFI_AUTH_OPEN; }
};
extern LabWiFi WiFi;

#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)
