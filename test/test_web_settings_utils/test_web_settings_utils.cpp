#include <unity.h>

#include <ArduinoJson.h>

#include "config/AppConfig.h"
#include "web/WebSettingsUtils.h"

void setUp() {}
void tearDown() {}

void test_web_settings_utils_parse_accepts_valid_payload_and_trims_display_name() {
    ArduinoJson::JsonDocument doc;
    deserializeJson(doc,
                    "{\"night_mode\":true,\"backlight_on\":false,\"ntp_enabled\":false,"
                    "\"ntp_server\":\"  192.168.1.1  \",\"units_c\":false,"
                    "\"temp_offset\":1.2,\"hum_offset\":-3,\"display_name\":\"  Aura  \","
                    "\"restart\":true}");

    WebSettingsUtils::SettingsSnapshot current{};
    current.available = true;

    const WebSettingsUtils::ParseResult result =
        WebSettingsUtils::parseSettingsUpdate(doc.as<ArduinoJson::JsonVariantConst>(),
                                             current,
                                             true,
                                             32);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.update.has_night_mode);
    TEST_ASSERT_TRUE(result.update.night_mode);
    TEST_ASSERT_TRUE(result.update.has_backlight);
    TEST_ASSERT_FALSE(result.update.backlight_on);
    TEST_ASSERT_TRUE(result.update.has_ntp_enabled);
    TEST_ASSERT_FALSE(result.update.ntp_enabled);
    TEST_ASSERT_TRUE(result.update.has_ntp_server);
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", result.update.ntp_server.c_str());
    TEST_ASSERT_TRUE(result.update.has_units_c);
    TEST_ASSERT_FALSE(result.update.units_c);
    TEST_ASSERT_TRUE(result.update.has_temp_offset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.2f, result.update.temp_offset);
    TEST_ASSERT_TRUE(result.update.has_hum_offset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, result.update.hum_offset);
    TEST_ASSERT_TRUE(result.update.has_display_name);
    TEST_ASSERT_EQUAL_STRING("Aura", result.update.display_name.c_str());
    TEST_ASSERT_TRUE(result.update.restart_requested);
}

void test_web_settings_utils_parse_rejects_locked_night_mode_and_invalid_types() {
    ArduinoJson::JsonDocument locked_doc;
    deserializeJson(locked_doc, "{\"night_mode\":false}");

    WebSettingsUtils::SettingsSnapshot locked{};
    locked.available = true;
    locked.night_mode_locked = true;

    WebSettingsUtils::ParseResult result =
        WebSettingsUtils::parseSettingsUpdate(locked_doc.as<ArduinoJson::JsonVariantConst>(),
                                             locked,
                                             true,
                                             32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(409, result.status_code);
    TEST_ASSERT_EQUAL_STRING("night_mode is locked by auto mode", result.error_message.c_str());

    ArduinoJson::JsonDocument bad_doc;
    deserializeJson(bad_doc, "{\"backlight_on\":\"yes\"}");
    locked.night_mode_locked = false;
    result = WebSettingsUtils::parseSettingsUpdate(bad_doc.as<ArduinoJson::JsonVariantConst>(),
                                                   locked,
                                                   true,
                                                   32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("backlight_on must be bool", result.error_message.c_str());
}

void test_web_settings_utils_parse_rejects_bad_ntp_server() {
    WebSettingsUtils::SettingsSnapshot current{};
    current.available = true;

    ArduinoJson::JsonDocument type_doc;
    deserializeJson(type_doc, "{\"ntp_server\":123}");
    WebSettingsUtils::ParseResult result =
        WebSettingsUtils::parseSettingsUpdate(type_doc.as<ArduinoJson::JsonVariantConst>(),
                                             current,
                                             true,
                                             32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("ntp_server must be string", result.error_message.c_str());

    ArduinoJson::JsonDocument space_doc;
    deserializeJson(space_doc, "{\"ntp_server\":\"bad host\"}");
    result = WebSettingsUtils::parseSettingsUpdate(space_doc.as<ArduinoJson::JsonVariantConst>(),
                                                   current,
                                                   true,
                                                   32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("ntp_server contains invalid characters", result.error_message.c_str());

    ArduinoJson::JsonDocument long_doc;
    deserializeJson(
        long_doc,
        "{\"ntp_server\":\"12345678901234567890123456789012345678901234567890123456789012345\"}");
    result = WebSettingsUtils::parseSettingsUpdate(long_doc.as<ArduinoJson::JsonVariantConst>(),
                                                   current,
                                                   true,
                                                   32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("ntp_server is too long", result.error_message.c_str());
}

void test_web_settings_utils_parse_rejects_bad_display_name() {
    WebSettingsUtils::SettingsSnapshot current{};
    current.available = true;

    ArduinoJson::JsonDocument missing_storage_doc;
    deserializeJson(missing_storage_doc, "{\"display_name\":\"Aura\"}");
    WebSettingsUtils::ParseResult result =
        WebSettingsUtils::parseSettingsUpdate(missing_storage_doc.as<ArduinoJson::JsonVariantConst>(),
                                             current,
                                             false,
                                             32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(503, result.status_code);

    ArduinoJson::JsonDocument long_doc;
    deserializeJson(long_doc, "{\"display_name\":\"123456\"}");
    result = WebSettingsUtils::parseSettingsUpdate(long_doc.as<ArduinoJson::JsonVariantConst>(),
                                                   current,
                                                   true,
                                                   5);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("display_name is too long", result.error_message.c_str());

    ArduinoJson::JsonDocument control_doc;
    deserializeJson(control_doc, "{\"display_name\":\"bad\\nname\"}");
    result = WebSettingsUtils::parseSettingsUpdate(control_doc.as<ArduinoJson::JsonVariantConst>(),
                                                   current,
                                                   true,
                                                   32);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("display_name contains invalid characters",
                             result.error_message.c_str());
}

void test_web_settings_utils_fill_settings_json_prefers_snapshot_then_config_then_nulls() {
    ArduinoJson::JsonDocument snapshot_doc;
    WebSettingsUtils::SettingsSnapshot snapshot{};
    snapshot.available = true;
    snapshot.night_mode = true;
    snapshot.night_mode_locked = true;
    snapshot.backlight_on = false;
    snapshot.backlight_transition_pending = true;
    snapshot.backlight_target_on = true;
    snapshot.ntp_enabled = false;
    snapshot.units_c = false;
    snapshot.time_format_24h = false;
    snapshot.temp_offset = 1.5f;
    snapshot.hum_offset = -2.0f;
    snapshot.ntp_server = "router.local";
    snapshot.display_name = "Aura";

    WebSettingsUtils::fillSettingsJson(snapshot_doc.to<ArduinoJson::JsonObject>(), &snapshot, nullptr);
    TEST_ASSERT_TRUE(snapshot_doc["night_mode"].as<bool>());
    TEST_ASSERT_TRUE(snapshot_doc["night_mode_locked"].as<bool>());
    TEST_ASSERT_FALSE(snapshot_doc["backlight_on"].as<bool>());
    TEST_ASSERT_TRUE(snapshot_doc["backlight_transition_pending"].as<bool>());
    TEST_ASSERT_TRUE(snapshot_doc["backlight_target_on"].as<bool>());
    TEST_ASSERT_FALSE(snapshot_doc["ntp_enabled"].as<bool>());
    TEST_ASSERT_FALSE(snapshot_doc["units_c"].as<bool>());
    TEST_ASSERT_FALSE(snapshot_doc["time_format_24h"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("router.local", snapshot_doc["ntp_server"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Aura", snapshot_doc["display_name"].as<const char *>());

    ArduinoJson::JsonDocument cfg_doc;
    Config::StoredConfig cfg;
    cfg.night_mode = false;
    cfg.auto_night_enabled = true;
    cfg.ntp_enabled = true;
    cfg.units_c = true;
    cfg.time_format_24h = false;
    cfg.temp_offset = 0.5f;
    cfg.hum_offset = 4.0f;
    cfg.ntp_server = "time.local";
    cfg.web_display_name = "Stored";
    WebSettingsUtils::fillSettingsJson(cfg_doc.to<ArduinoJson::JsonObject>(), nullptr, &cfg);
    TEST_ASSERT_FALSE(cfg_doc["night_mode"].as<bool>());
    TEST_ASSERT_TRUE(cfg_doc["night_mode_locked"].as<bool>());
    TEST_ASSERT_TRUE(cfg_doc["backlight_on"].isNull());
    TEST_ASSERT_TRUE(cfg_doc["backlight_transition_pending"].isNull());
    TEST_ASSERT_TRUE(cfg_doc["backlight_target_on"].isNull());
    TEST_ASSERT_TRUE(cfg_doc["ntp_enabled"].as<bool>());
    TEST_ASSERT_FALSE(cfg_doc["time_format_24h"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("time.local", cfg_doc["ntp_server"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Stored", cfg_doc["display_name"].as<const char *>());

    ArduinoJson::JsonDocument empty_doc;
    WebSettingsUtils::fillSettingsJson(empty_doc.to<ArduinoJson::JsonObject>(), nullptr, nullptr);
    TEST_ASSERT_TRUE(empty_doc["night_mode"].isNull());
    TEST_ASSERT_TRUE(empty_doc["backlight_transition_pending"].isNull());
    TEST_ASSERT_TRUE(empty_doc["backlight_target_on"].isNull());
    TEST_ASSERT_TRUE(empty_doc["ntp_enabled"].isNull());
    TEST_ASSERT_TRUE(empty_doc["time_format_24h"].isNull());
    TEST_ASSERT_TRUE(empty_doc["ntp_server"].isNull());
    TEST_ASSERT_TRUE(empty_doc["display_name"].isNull());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_settings_utils_parse_accepts_valid_payload_and_trims_display_name);
    RUN_TEST(test_web_settings_utils_parse_rejects_locked_night_mode_and_invalid_types);
    RUN_TEST(test_web_settings_utils_parse_rejects_bad_ntp_server);
    RUN_TEST(test_web_settings_utils_parse_rejects_bad_display_name);
    RUN_TEST(test_web_settings_utils_fill_settings_json_prefers_snapshot_then_config_then_nulls);
    return UNITY_END();
}
