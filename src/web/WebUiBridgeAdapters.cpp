// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebUiBridgeAdapters.h"

namespace WebUiBridgeAdapters {

WebSettingsUtils::SettingsSnapshot captureSettingsSnapshot(const WebUiBridge::Snapshot &snapshot) {
    return {
        snapshot.available,
        snapshot.night_mode,
        snapshot.night_mode_locked,
        snapshot.backlight_on,
        snapshot.units_c,
        snapshot.temp_offset,
        snapshot.hum_offset,
        snapshot.display_name,
    };
}

WebUiBridge::SettingsUpdate toUiSettingsUpdate(const WebSettingsUtils::SettingsUpdate &update) {
    WebUiBridge::SettingsUpdate ui_update{};
    ui_update.has_night_mode = update.has_night_mode;
    ui_update.night_mode = update.night_mode;
    ui_update.has_backlight = update.has_backlight;
    ui_update.backlight_on = update.backlight_on;
    ui_update.has_units_c = update.has_units_c;
    ui_update.units_c = update.units_c;
    ui_update.has_temp_offset = update.has_temp_offset;
    ui_update.temp_offset = update.temp_offset;
    ui_update.has_hum_offset = update.has_hum_offset;
    ui_update.hum_offset = update.hum_offset;
    ui_update.has_display_name = update.has_display_name;
    ui_update.display_name = update.display_name;
    ui_update.restart_requested = update.restart_requested;
    return ui_update;
}

WebUiBridge::WifiSaveUpdate toUiWifiSaveUpdate(const WebWifiSaveUtils::SaveUpdate &update) {
    WebUiBridge::WifiSaveUpdate ui_update{};
    ui_update.ssid = update.ssid;
    ui_update.pass = update.pass;
    ui_update.enabled = update.enabled;
    return ui_update;
}

WebUiBridge::MqttSaveUpdate toUiMqttSaveUpdate(const WebMqttSaveUtils::SaveUpdate &update) {
    WebUiBridge::MqttSaveUpdate ui_update{};
    ui_update.host = update.host;
    ui_update.port = update.port;
    ui_update.user = update.user;
    ui_update.pass = update.pass;
    ui_update.base_topic = update.base_topic;
    ui_update.device_name = update.device_name;
    ui_update.discovery = update.discovery;
    ui_update.anonymous = update.anonymous;
    return ui_update;
}

WebUiBridge::DacActionUpdate toUiDacActionUpdate(const WebDacApiUtils::DacActionUpdate &update) {
    WebUiBridge::DacActionUpdate ui_update{};
    switch (update.type) {
        case WebDacApiUtils::DacActionUpdate::Type::SetMode:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::SetMode;
            break;
        case WebDacApiUtils::DacActionUpdate::Type::SetManualStep:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::SetManualStep;
            break;
        case WebDacApiUtils::DacActionUpdate::Type::SetTimerSeconds:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::SetTimerSeconds;
            break;
        case WebDacApiUtils::DacActionUpdate::Type::Start:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::Start;
            break;
        case WebDacApiUtils::DacActionUpdate::Type::Stop:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::Stop;
            break;
        case WebDacApiUtils::DacActionUpdate::Type::StartAuto:
            ui_update.type = WebUiBridge::DacActionUpdate::Type::StartAuto;
            break;
    }
    ui_update.auto_mode = update.auto_mode;
    ui_update.manual_step = update.manual_step;
    ui_update.timer_seconds = update.timer_seconds;
    return ui_update;
}

WebUiBridge::DacAutoUpdate toUiDacAutoUpdate(const WebDacApiUtils::DacAutoUpdate &update) {
    WebUiBridge::DacAutoUpdate ui_update{};
    ui_update.config = update.config;
    ui_update.rearm = update.rearm;
    return ui_update;
}

WebUiBridge::ThemeUpdate toUiThemeUpdate(const ThemeColors &colors) {
    WebUiBridge::ThemeUpdate ui_update{};
    ui_update.colors = colors;
    return ui_update;
}

} // namespace WebUiBridgeAdapters
