#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebThemeApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_theme_api_utils_fill_state_json_writes_theme_colors() {
    ThemeColors colors{};
    colors.screen_bg = 0x112233;
    colors.card_bg = 0x223344;
    colors.gradient_color = 0x334455;
    colors.gradient_enabled = true;
    colors.card_border = 0x445566;
    colors.shadow_color = 0x556677;
    colors.text_primary = 0x667788;

    ArduinoJson::JsonDocument doc;
    WebThemeApiUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(), colors);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("#112233", doc["bg_color"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("#223344", doc["card_top"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("#334455", doc["card_bottom"].as<const char *>());
    TEST_ASSERT_TRUE(doc["card_gradient"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("#445566", doc["card_border"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("#556677", doc["shadow_color"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("#667788", doc["text_color"].as<const char *>());
}

void test_web_theme_api_utils_fill_error_json_writes_failure_payload() {
    ArduinoJson::JsonDocument doc;
    WebThemeApiUtils::fillErrorJson(doc.to<ArduinoJson::JsonObject>(), "WiFi required");

    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("WiFi required", doc["error"].as<const char *>());
}

void test_web_theme_api_utils_validate_api_access_checks_wifi_and_open_screens() {
    WebThemeApiUtils::ApiAccessResult result =
        WebThemeApiUtils::validateApiAccess(false, true, true);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(403, result.status_code);
    TEST_ASSERT_EQUAL_STRING("WiFi required", result.error_message.c_str());

    result = WebThemeApiUtils::validateApiAccess(true, false, true);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(409, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Open Theme screen to enable", result.error_message.c_str());

    result = WebThemeApiUtils::validateApiAccess(true, true, false);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(409, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Open Custom Theme screen to enable", result.error_message.c_str());

    result = WebThemeApiUtils::validateApiAccess(true, true, true);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(200, result.status_code);
}

void test_web_theme_api_utils_parse_apply_update_updates_colors_and_gradient_flags() {
    ThemeColors colors{};
    colors.screen_bg = 0x000000;
    colors.card_bg = 0x111111;
    colors.gradient_color = 0x222222;
    colors.gradient_enabled = false;
    colors.card_border = 0x333333;
    colors.shadow_color = 0x444444;
    colors.text_primary = 0x555555;
    colors.screen_gradient_enabled = true;
    colors.screen_gradient_color = 0x999999;
    colors.screen_gradient_direction = 7;
    colors.shadow_enabled = false;

    ArduinoJson::JsonDocument doc;
    deserializeJson(doc,
                    "{\"bg\":\"#ABCDEF\",\"card_top\":\"#010203\",\"card_bottom\":\"#040506\","
                    "\"border\":\"#070809\",\"shadow\":\"#0A0B0C\",\"text\":\"#0D0E0F\","
                    "\"card_gradient\":1}");

    const WebThemeApiUtils::ParseResult result =
        WebThemeApiUtils::parseApplyUpdate(doc.as<ArduinoJson::JsonVariantConst>(), colors);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF, static_cast<uint32_t>(result.colors.screen_bg));
    TEST_ASSERT_FALSE(result.colors.screen_gradient_enabled);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF, static_cast<uint32_t>(result.colors.screen_gradient_color));
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(result.colors.screen_gradient_direction));
    TEST_ASSERT_EQUAL_UINT32(0x010203, static_cast<uint32_t>(result.colors.card_bg));
    TEST_ASSERT_EQUAL_UINT32(0x040506, static_cast<uint32_t>(result.colors.gradient_color));
    TEST_ASSERT_EQUAL_UINT32(0x070809, static_cast<uint32_t>(result.colors.card_border));
    TEST_ASSERT_EQUAL_UINT32(0x0A0B0C, static_cast<uint32_t>(result.colors.shadow_color));
    TEST_ASSERT_EQUAL_UINT32(0x0D0E0F, static_cast<uint32_t>(result.colors.text_primary));
    TEST_ASSERT_TRUE(result.colors.gradient_enabled);
    TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(result.colors.gradient_direction));
    TEST_ASSERT_TRUE(result.colors.shadow_enabled);
}

void test_web_theme_api_utils_parse_apply_update_keeps_current_values_when_payload_invalid_or_missing() {
    ThemeColors colors{};
    colors.screen_bg = 0x123456;
    colors.card_bg = 0x654321;
    colors.gradient_enabled = true;
    colors.gradient_direction = 4;

    ArduinoJson::JsonDocument doc;
    deserializeJson(doc, "{\"bg\":\"bad-color\",\"card_gradient\":0}");

    const WebThemeApiUtils::ParseResult result =
        WebThemeApiUtils::parseApplyUpdate(doc.as<ArduinoJson::JsonVariantConst>(), colors);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT32(0x123456, static_cast<uint32_t>(result.colors.screen_bg));
    TEST_ASSERT_EQUAL_UINT32(0x654321, static_cast<uint32_t>(result.colors.card_bg));
    TEST_ASSERT_FALSE(result.colors.gradient_enabled);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(result.colors.gradient_direction));
}

void test_web_theme_api_utils_parse_apply_request_body_rejects_missing_and_invalid_json() {
    ThemeColors colors{};

    WebThemeApiUtils::ParseResult result = WebThemeApiUtils::parseApplyRequestBody("", colors);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Missing body", result.error_message.c_str());

    result = WebThemeApiUtils::parseApplyRequestBody("{bad", colors);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", result.error_message.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_theme_api_utils_fill_state_json_writes_theme_colors);
    RUN_TEST(test_web_theme_api_utils_fill_error_json_writes_failure_payload);
    RUN_TEST(test_web_theme_api_utils_validate_api_access_checks_wifi_and_open_screens);
    RUN_TEST(test_web_theme_api_utils_parse_apply_update_updates_colors_and_gradient_flags);
    RUN_TEST(test_web_theme_api_utils_parse_apply_update_keeps_current_values_when_payload_invalid_or_missing);
    RUN_TEST(test_web_theme_api_utils_parse_apply_request_body_rejects_missing_and_invalid_json);
    return UNITY_END();
}
