// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebThemeApiUtils.h"

#include "web/WebColorUtils.h"

namespace WebThemeApiUtils {

namespace {

#ifdef UNIT_TEST
constexpr lv_grad_dir_t kGradNone = 0;
constexpr lv_grad_dir_t kGradVer = 1;

uint32_t color_to_rgb(lv_color_t color) {
    return static_cast<uint32_t>(color) & 0xFFFFFFu;
}

lv_color_t color_from_rgb(uint32_t rgb) {
    return static_cast<lv_color_t>(rgb & 0xFFFFFFu);
}
#else
constexpr lv_grad_dir_t kGradNone = LV_GRAD_DIR_NONE;
constexpr lv_grad_dir_t kGradVer = LV_GRAD_DIR_VER;

uint32_t color_to_rgb(lv_color_t color) {
    return lv_color_to32(color) & 0xFFFFFFu;
}

lv_color_t color_from_rgb(uint32_t rgb) {
    return lv_color_hex(rgb & 0xFFFFFFu);
}
#endif

void maybe_parse_color(ArduinoJson::JsonVariantConst root,
                       const char *key,
                       lv_color_t &target,
                       uint32_t &scratch_rgb) {
    const char *raw = root[key] | "";
    if (WebColorUtils::parseHexColorRgb(String(raw), scratch_rgb)) {
        target = color_from_rgb(scratch_rgb);
    }
}

ApiAccessResult fail_access(uint16_t status_code, const char *message) {
    ApiAccessResult result{};
    result.success = false;
    result.status_code = status_code;
    result.error_message = message;
    return result;
}

} // namespace

void fillStateJson(ArduinoJson::JsonObject root, const ThemeColors &colors) {
    root["success"] = true;
    root["bg_color"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.screen_bg));
    root["card_top"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.card_bg));
    root["card_bottom"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.gradient_color));
    root["card_gradient"] = colors.gradient_enabled;
    root["card_border"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.card_border));
    root["shadow_color"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.shadow_color));
    root["text_color"] = WebColorUtils::rgbToHexString(color_to_rgb(colors.text_primary));
}

void fillErrorJson(ArduinoJson::JsonObject root, const char *message) {
    root["success"] = false;
    root["error"] = message ? message : "";
}

ApiAccessResult validateApiAccess(bool wifi_ready,
                                  bool theme_screen_open,
                                  bool theme_custom_screen_open) {
    if (!wifi_ready) {
        return fail_access(403, "WiFi required");
    }
    if (!theme_screen_open) {
        return fail_access(409, "Open Theme screen to enable");
    }
    if (!theme_custom_screen_open) {
        return fail_access(409, "Open Custom Theme screen to enable");
    }

    ApiAccessResult result{};
    result.success = true;
    result.status_code = 200;
    return result;
}

ParseResult parseApplyRequestBody(const String &body, const ThemeColors &current_colors) {
    if (body.length() == 0) {
        ParseResult result{};
        result.success = false;
        result.status_code = 400;
        result.error_message = "Missing body";
        return result;
    }

    ArduinoJson::JsonDocument doc;
    const ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, body);
    if (err) {
        ParseResult result{};
        result.success = false;
        result.status_code = 400;
        result.error_message = "Invalid JSON";
        return result;
    }

    return parseApplyUpdate(doc.as<ArduinoJson::JsonVariantConst>(), current_colors);
}

ParseResult parseApplyUpdate(ArduinoJson::JsonVariantConst root, const ThemeColors &current_colors) {
    ParseResult result{};
    result.colors = current_colors;

    uint32_t parsed_rgb = 0;

    const char *bg = root["bg"] | "";
    if (WebColorUtils::parseHexColorRgb(String(bg), parsed_rgb)) {
        result.colors.screen_bg = color_from_rgb(parsed_rgb);
        result.colors.screen_gradient_enabled = false;
        result.colors.screen_gradient_color = color_from_rgb(parsed_rgb);
        result.colors.screen_gradient_direction = kGradNone;
    }

    maybe_parse_color(root, "card_top", result.colors.card_bg, parsed_rgb);
    maybe_parse_color(root, "card_bottom", result.colors.gradient_color, parsed_rgb);
    maybe_parse_color(root, "border", result.colors.card_border, parsed_rgb);
    maybe_parse_color(root, "shadow", result.colors.shadow_color, parsed_rgb);
    maybe_parse_color(root, "text", result.colors.text_primary, parsed_rgb);

    int gradient_enabled = root["card_gradient"] | (result.colors.gradient_enabled ? 1 : 0);
    result.colors.gradient_enabled = gradient_enabled != 0;
    result.colors.gradient_direction = result.colors.gradient_enabled ? kGradVer : kGradNone;
    result.colors.shadow_enabled = true;

    result.success = true;
    result.status_code = 200;
    return result;
}

} // namespace WebThemeApiUtils
