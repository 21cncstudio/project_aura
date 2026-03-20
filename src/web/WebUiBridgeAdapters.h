// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "config/AppData.h"
#include "web/WebDacApiUtils.h"
#include "web/WebMqttSaveUtils.h"
#include "web/WebSettingsUtils.h"
#include "web/WebThemeApiUtils.h"
#include "web/WebUiBridge.h"
#include "web/WebWifiSaveUtils.h"

namespace WebUiBridgeAdapters {

WebSettingsUtils::SettingsSnapshot captureSettingsSnapshot(const WebUiBridge::Snapshot &snapshot);
WebUiBridge::SettingsUpdate toUiSettingsUpdate(const WebSettingsUtils::SettingsUpdate &update);
WebUiBridge::WifiSaveUpdate toUiWifiSaveUpdate(const WebWifiSaveUtils::SaveUpdate &update);
WebUiBridge::MqttSaveUpdate toUiMqttSaveUpdate(const WebMqttSaveUtils::SaveUpdate &update);
WebUiBridge::DacActionUpdate toUiDacActionUpdate(const WebDacApiUtils::DacActionUpdate &update);
WebUiBridge::DacAutoUpdate toUiDacAutoUpdate(const WebDacApiUtils::DacAutoUpdate &update);
WebUiBridge::ThemeUpdate toUiThemeUpdate(const ThemeColors &colors);

} // namespace WebUiBridgeAdapters
