#pragma once

typedef enum {
    WL_IDLE_STATUS = 0,
    WL_CONNECTED = 3,
    WL_DISCONNECTED = 6,
} wl_status_t;

class WiFiClass {
public:
    wl_status_t status();
    int RSSI();
};

extern WiFiClass WiFi;
