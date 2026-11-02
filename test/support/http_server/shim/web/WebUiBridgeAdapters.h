#pragma once
#include "web/WebUiBridge.h"
#include "web/WebWifiSaveUtils.h"
namespace WebUiBridgeAdapters {
inline WebUiBridge::WifiSaveUpdate toUiWifiSaveUpdate(const WebWifiSaveUtils::SaveUpdate &u) {
 WebUiBridge::WifiSaveUpdate out;out.settings=u.settings;return out;
}
}
