#pragma once
#include "config/AppConfig.h"
class StorageManager { public: Config::WifiSettings stored; int clears = 0;
    void loadWiFiSettings(Config::WifiSettings &s) { s = stored; }
    void clearWiFiCredentials() { stored.ssid = ""; stored.pass = ""; ++clears; }
    void saveWiFiEnabled(bool e) { stored.enabled = e; } };
