#pragma once
#include <Arduino.h>
namespace WebInputValidation { constexpr size_t kWifiSsidMaxBytes = 32;
inline bool isWifiSsidValid(const String &v, size_t m = kWifiSsidMaxBytes) { return v.length() <= m; } }
