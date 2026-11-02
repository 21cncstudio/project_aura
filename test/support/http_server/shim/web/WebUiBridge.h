#pragma once
#include <Arduino.h>
#include "config/AppConfig.h"
class WebUiBridge {
public:
 struct WifiSaveUpdate { Config::WifiSettings settings{}; };
 struct ApplyResult { bool success = true; uint16_t status_code = 503; String error_message; };
 bool fail = false; unsigned saves = 0;
 ApplyResult applyWifiSave(const WifiSaveUpdate &) { ++saves; ApplyResult r; r.success=!fail; return r; }
};
