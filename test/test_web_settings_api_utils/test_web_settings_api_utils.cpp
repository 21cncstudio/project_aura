#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebSettingsApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_settings_api_utils_parse_update_request_body_rejects_missing_body_and_bad_json() {
    WebSettingsUtils::SettingsSnapshot current{};
    current.available = true;

    const WebSettingsApiUtils::ParseResult missing =
        WebSettingsApiUtils::parseUpdateRequestBody("", current, true, 32);
    TEST_ASSERT_FALSE(missing.success);
    TEST_ASSERT_EQUAL_INT(400, missing.status_code);
    TEST_ASSERT_EQUAL_STRING("Missing body", missing.error_message.c_str());

    const WebSettingsApiUtils::ParseResult invalid =
        WebSettingsApiUtils::parseUpdateRequestBody("{bad", current, true, 32);
    TEST_ASSERT_FALSE(invalid.success);
    TEST_ASSERT_EQUAL_INT(400, invalid.status_code);
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", invalid.error_message.c_str());
}

void test_web_settings_api_utils_parse_update_request_body_returns_settings_update() {
    WebSettingsUtils::SettingsSnapshot current{};
    current.available = true;
    current.night_mode_locked = false;

    const WebSettingsApiUtils::ParseResult result = WebSettingsApiUtils::parseUpdateRequestBody(
        "{\"night_mode\":true,\"units_c\":false,\"display_name\":\"  Aura  \",\"restart\":true}",
        current,
        true,
        32);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(200, result.status_code);
    TEST_ASSERT_TRUE(result.update.has_night_mode);
    TEST_ASSERT_TRUE(result.update.night_mode);
    TEST_ASSERT_TRUE(result.update.has_units_c);
    TEST_ASSERT_FALSE(result.update.units_c);
    TEST_ASSERT_TRUE(result.update.has_display_name);
    TEST_ASSERT_EQUAL_STRING("Aura", result.update.display_name.c_str());
    TEST_ASSERT_TRUE(result.update.restart_requested);
}

void test_web_settings_api_utils_fill_update_success_json_embeds_applied_snapshot() {
    WebSettingsUtils::SettingsSnapshot snapshot{};
    snapshot.available = true;
    snapshot.night_mode = true;
    snapshot.night_mode_locked = false;
    snapshot.backlight_on = true;
    snapshot.units_c = false;
    snapshot.temp_offset = 1.25f;
    snapshot.hum_offset = -2.5f;
    snapshot.display_name = "Aura";

    ArduinoJson::JsonDocument doc;
    WebSettingsApiUtils::fillUpdateSuccessJson(doc.to<ArduinoJson::JsonObject>(), snapshot, true);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_TRUE(doc["restart"].as<bool>());
    TEST_ASSERT_TRUE(doc["settings"]["night_mode"].as<bool>());
    TEST_ASSERT_TRUE(doc["settings"]["backlight_on"].as<bool>());
    TEST_ASSERT_FALSE(doc["settings"]["units_c"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Aura", doc["settings"]["display_name"].as<const char *>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_settings_api_utils_parse_update_request_body_rejects_missing_body_and_bad_json);
    RUN_TEST(test_web_settings_api_utils_parse_update_request_body_returns_settings_update);
    RUN_TEST(test_web_settings_api_utils_fill_update_success_json_embeds_applied_snapshot);
    return UNITY_END();
}
