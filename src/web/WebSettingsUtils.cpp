// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSettingsUtils.h"

#include <ctype.h>

#include "web/WebTextUtils.h"

namespace WebSettingsUtils {

namespace {

String trim_whitespace(const String &value) {
    const char *begin = value.c_str();
    if (!begin) {
        return String();
    }

    const char *end = begin;
    while (*end != '\0') {
        ++end;
    }
    while (begin < end && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    String out;
    out.reserve(static_cast<size_t>(end - begin));
    while (begin < end) {
        out += *begin++;
    }
    return out;
}

ParseResult fail_result(uint16_t status_code, const char *message) {
    ParseResult result;
    result.success = false;
    result.status_code = status_code;
    result.error_message = message;
    return result;
}

void fill_unavailable(ArduinoJson::JsonObject settings) {
    settings["night_mode"] = nullptr;
    settings["night_mode_locked"] = nullptr;
    settings["backlight_on"] = nullptr;
    settings["units_c"] = nullptr;
    settings["temp_offset"] = nullptr;
    settings["hum_offset"] = nullptr;
    settings["display_name"] = nullptr;
}

void fill_from_snapshot(ArduinoJson::JsonObject settings, const SettingsSnapshot &snapshot) {
    settings["night_mode"] = snapshot.night_mode;
    settings["night_mode_locked"] = snapshot.night_mode_locked;
    settings["backlight_on"] = snapshot.backlight_on;
    settings["units_c"] = snapshot.units_c;
    settings["temp_offset"] = snapshot.temp_offset;
    settings["hum_offset"] = snapshot.hum_offset;
    settings["display_name"] = snapshot.display_name;
}

void fill_from_config(ArduinoJson::JsonObject settings, const Config::StoredConfig &cfg) {
    settings["night_mode"] = cfg.night_mode;
    settings["night_mode_locked"] = cfg.auto_night_enabled;
    settings["backlight_on"] = nullptr;
    settings["units_c"] = cfg.units_c;
    settings["temp_offset"] = cfg.temp_offset;
    settings["hum_offset"] = cfg.hum_offset;
    settings["display_name"] = cfg.web_display_name;
}

} // namespace

ParseResult parseSettingsUpdate(ArduinoJson::JsonVariantConst root,
                                const SettingsSnapshot &current_settings,
                                bool storage_available,
                                size_t display_name_max_len) {
    ParseResult result;
    result.success = true;
    result.status_code = 200;

    SettingsUpdate &update = result.update;

    const ArduinoJson::JsonVariantConst night_mode_var = root["night_mode"];
    if (!night_mode_var.isNull()) {
        if (!night_mode_var.is<bool>()) {
            return fail_result(400, "night_mode must be bool");
        }
        update.has_night_mode = true;
        update.night_mode = night_mode_var.as<bool>();
        if (current_settings.night_mode_locked) {
            return fail_result(409, "night_mode is locked by auto mode");
        }
    }

    const ArduinoJson::JsonVariantConst backlight_var = root["backlight_on"];
    if (!backlight_var.isNull()) {
        if (!backlight_var.is<bool>()) {
            return fail_result(400, "backlight_on must be bool");
        }
        update.has_backlight = true;
        update.backlight_on = backlight_var.as<bool>();
    }

    const ArduinoJson::JsonVariantConst units_c_var = root["units_c"];
    if (!units_c_var.isNull()) {
        if (!units_c_var.is<bool>()) {
            return fail_result(400, "units_c must be bool");
        }
        update.has_units_c = true;
        update.units_c = units_c_var.as<bool>();
    }

    const ArduinoJson::JsonVariantConst temp_offset_var = root["temp_offset"];
    const ArduinoJson::JsonVariantConst hum_offset_var = root["hum_offset"];
    update.has_temp_offset = !temp_offset_var.isNull();
    update.has_hum_offset = !hum_offset_var.isNull();
    if (update.has_temp_offset) {
        if (!temp_offset_var.is<float>() && !temp_offset_var.is<int>()) {
            return fail_result(400, "temp_offset must be number");
        }
        update.temp_offset = temp_offset_var.as<float>();
    }
    if (update.has_hum_offset) {
        if (!hum_offset_var.is<float>() && !hum_offset_var.is<int>()) {
            return fail_result(400, "hum_offset must be number");
        }
        update.hum_offset = hum_offset_var.as<float>();
    }

    const ArduinoJson::JsonVariantConst display_name_var = root["display_name"];
    if (!display_name_var.isNull()) {
        if (!display_name_var.is<const char *>()) {
            return fail_result(400, "display_name must be string");
        }
        if (!storage_available) {
            return fail_result(503, "Storage unavailable");
        }

        update.display_name = trim_whitespace(display_name_var.as<String>());
        if (update.display_name.length() > display_name_max_len) {
            return fail_result(400, "display_name is too long");
        }
        if (WebTextUtils::hasControlChars(update.display_name)) {
            return fail_result(400, "display_name contains invalid characters");
        }
        update.has_display_name = true;
    }

    const ArduinoJson::JsonVariantConst restart_var = root["restart"];
    if (!restart_var.isNull()) {
        if (!restart_var.is<bool>()) {
            return fail_result(400, "restart must be bool");
        }
        update.restart_requested = restart_var.as<bool>();
    }

    return result;
}

void fillSettingsJson(ArduinoJson::JsonObject settings,
                      const SettingsSnapshot *snapshot,
                      const Config::StoredConfig *cfg) {
    if (snapshot && snapshot->available) {
        fill_from_snapshot(settings, *snapshot);
        return;
    }
    if (cfg) {
        fill_from_config(settings, *cfg);
        return;
    }
    fill_unavailable(settings);
}

} // namespace WebSettingsUtils
