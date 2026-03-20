#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebDacApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_dac_api_utils_fill_success_json_sets_success_flag() {
    ArduinoJson::JsonDocument doc;
    WebDacApiUtils::fillSuccessJson(doc.to<ArduinoJson::JsonObject>());
    TEST_ASSERT_TRUE(doc["success"].as<bool>());
}

void test_web_dac_api_utils_fill_state_json_populates_dac_auto_and_sensor_fields() {
    WebDacApiUtils::StatePayload payload{};
    payload.now_ms = 1000;
    payload.gas_warmup = true;
    payload.dac.available = true;
    payload.dac.running = true;
    payload.dac.output_known = true;
    payload.dac.auto_mode = true;
    payload.dac.manual_override = false;
    payload.dac.auto_resume_blocked = true;
    payload.dac.manual_step = 3;
    payload.dac.selected_timer_s = 600;
    payload.dac.output_mv = 5000;
    payload.dac.stop_at_ms = 4500;
    payload.dac.auto_config.enabled = true;
    payload.sensors.co2 = 742;
    payload.sensors.co2_valid = true;
    payload.sensors.co_ppm = 5.5f;
    payload.sensors.co_valid = true;
    payload.sensors.pm25 = 12.5f;
    payload.sensors.pm25_valid = true;
    payload.sensors.voc_index = 87;
    payload.sensors.voc_valid = true;

    ArduinoJson::JsonDocument doc;
    WebDacApiUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_TRUE(doc["dac"]["available"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("auto", doc["dac"]["mode"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT(4, doc["dac"]["remaining_s"].as<unsigned>());
    TEST_ASSERT_EQUAL_UINT(50, doc["dac"]["output_percent"].as<unsigned>());
    TEST_ASSERT_EQUAL_STRING("RUNNING", doc["dac"]["status"].as<const char *>());
    TEST_ASSERT_TRUE(doc["auto"]["enabled"].as<bool>());
    TEST_ASSERT_TRUE(doc["sensors"]["gas_warmup"].as<bool>());
    TEST_ASSERT_EQUAL_INT(742, doc["sensors"]["co2"].as<int>());
    TEST_ASSERT_EQUAL_INT(87, doc["sensors"]["voc_index"].as<int>());
}

void test_web_dac_api_utils_parse_action_update_supports_known_actions_and_rejects_bad_values() {
    ArduinoJson::JsonDocument mode_doc;
    deserializeJson(mode_doc, "{\"action\":\"set_mode\",\"mode\":\"manual\"}");
    WebDacApiUtils::DacActionParseResult result =
        WebDacApiUtils::parseActionUpdate(mode_doc.as<ArduinoJson::JsonVariantConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebDacApiUtils::DacActionUpdate::Type::SetMode),
                          static_cast<int>(result.update.type));
    TEST_ASSERT_FALSE(result.update.auto_mode);

    ArduinoJson::JsonDocument timer_doc;
    deserializeJson(timer_doc, "{\"action\":\"set_timer\",\"seconds\":600}");
    result = WebDacApiUtils::parseActionUpdate(timer_doc.as<ArduinoJson::JsonVariantConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT32(600, result.update.timer_seconds);

    ArduinoJson::JsonDocument invalid_mode_doc;
    deserializeJson(invalid_mode_doc, "{\"action\":\"set_mode\",\"mode\":\"bad\"}");
    result = WebDacApiUtils::parseActionUpdate(invalid_mode_doc.as<ArduinoJson::JsonVariantConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid mode", result.error_message.c_str());

    ArduinoJson::JsonDocument invalid_timer_doc;
    deserializeJson(invalid_timer_doc, "{\"action\":\"set_timer\",\"seconds\":1234}");
    result = WebDacApiUtils::parseActionUpdate(invalid_timer_doc.as<ArduinoJson::JsonVariantConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid timer value", result.error_message.c_str());
}

void test_web_dac_api_utils_parse_action_request_body_rejects_missing_and_invalid_json() {
    WebDacApiUtils::DacActionParseResult result =
        WebDacApiUtils::parseActionRequestBody("");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing body", result.error_message.c_str());

    result = WebDacApiUtils::parseActionRequestBody("{bad");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", result.error_message.c_str());
}

void test_web_dac_api_utils_parse_auto_update_reads_nested_auto_and_rearm() {
    DacAutoConfig current{};
    current.enabled = false;

    ArduinoJson::JsonDocument doc;
    deserializeJson(doc,
                    "{\"rearm\":true,\"auto\":{\"enabled\":true,\"co2\":{\"enabled\":false,"
                    "\"green\":11,\"yellow\":22,\"orange\":33,\"red\":44}}}");

    const WebDacApiUtils::DacAutoParseResult result =
        WebDacApiUtils::parseAutoUpdate(doc.as<ArduinoJson::JsonVariantConst>(), current);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.update.rearm);
    TEST_ASSERT_TRUE(result.update.config.enabled);
    TEST_ASSERT_FALSE(result.update.config.co2.enabled);
    TEST_ASSERT_EQUAL_UINT8(11, result.update.config.co2.band.green_percent);
    TEST_ASSERT_EQUAL_UINT8(44, result.update.config.co2.band.red_percent);

    ArduinoJson::JsonDocument bad_doc;
    deserializeJson(bad_doc, "[]");
    const WebDacApiUtils::DacAutoParseResult bad_result =
        WebDacApiUtils::parseAutoUpdate(bad_doc.as<ArduinoJson::JsonVariantConst>(), current);
    TEST_ASSERT_FALSE(bad_result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid auto payload", bad_result.error_message.c_str());
}

void test_web_dac_api_utils_parse_auto_request_body_rejects_missing_and_invalid_json() {
    DacAutoConfig current{};

    WebDacApiUtils::DacAutoParseResult result =
        WebDacApiUtils::parseAutoRequestBody("", current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing body", result.error_message.c_str());

    result = WebDacApiUtils::parseAutoRequestBody("{bad", current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", result.error_message.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_dac_api_utils_fill_success_json_sets_success_flag);
    RUN_TEST(test_web_dac_api_utils_fill_state_json_populates_dac_auto_and_sensor_fields);
    RUN_TEST(test_web_dac_api_utils_parse_action_update_supports_known_actions_and_rejects_bad_values);
    RUN_TEST(test_web_dac_api_utils_parse_action_request_body_rejects_missing_and_invalid_json);
    RUN_TEST(test_web_dac_api_utils_parse_auto_update_reads_nested_auto_and_rearm);
    RUN_TEST(test_web_dac_api_utils_parse_auto_request_body_rejects_missing_and_invalid_json);
    return UNITY_END();
}
