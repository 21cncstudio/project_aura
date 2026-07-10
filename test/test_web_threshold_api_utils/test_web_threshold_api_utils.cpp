#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebThresholdApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_threshold_api_utils_parse_rejects_missing_and_bad_json() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();

    WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody("", current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Missing body", result.error_message.c_str());

    result = WebThresholdApiUtils::parseUpdateRequestBody("{bad", current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", result.error_message.c_str());

    result = WebThresholdApiUtils::parseUpdateRequestBody("{\"version\":\"1\"}", current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.length() > 0);
}

void test_web_threshold_api_utils_parse_rejects_invalid_without_update() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();
    const WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody(
            "{\"metrics\":{\"co\":{\"green\":50,\"yellow\":10,\"orange\":100}}}",
            current);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.length() > 0);
}

void test_web_threshold_api_utils_parse_rejects_non_numeric_fields() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();
    WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody(
            "{\"metrics\":{\"co2\":{\"green\":\"800\"}}}",
            current);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.length() > 0);

    result = WebThresholdApiUtils::parseUpdateRequestBody(
        "{\"metrics\":{\"co2\":\"bad\"}}",
        current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.length() > 0);
}

void test_web_threshold_api_utils_parse_rejects_non_boolean_switches() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();
    const WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody(
            "{\"background_alerts\":{\"hcho_enabled\":\"false\"}}",
            current);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.length() > 0);
}

void test_web_threshold_api_utils_parse_rejects_negative_rh_and_ah_thresholds() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();

    WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody(
            "{\"metrics\":{\"rh\":{\"orange_min\":-1,\"yellow_min\":1,\"good_min\":2,"
            "\"good_max\":3,\"yellow_max\":4,\"orange_max\":5}}}",
            current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("non-negative") != String::npos);

    result = WebThresholdApiUtils::parseUpdateRequestBody(
        "{\"metrics\":{\"ah\":{\"orange_min\":-1,\"yellow_min\":1,\"good_min\":2,"
        "\"good_max\":3,\"yellow_max\":4,\"orange_max\":5}}}",
        current);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(result.error_message.find("non-negative") != String::npos);
}

void test_web_threshold_api_utils_parse_accepts_switch_and_metric_update() {
    const DisplayThresholds::Config current = DisplayThresholds::defaults();
    const WebThresholdApiUtils::ParseResult result =
        WebThresholdApiUtils::parseUpdateRequestBody(
            "{\"metrics\":{\"hcho\":{\"green\":25,\"yellow\":50,\"orange\":90},"
            "\"voc\":{\"green\":120,\"yellow\":220,\"orange\":320},"
            "\"nox\":{\"green\":40,\"yellow\":90,\"orange\":180}},"
            "\"background_alerts\":{\"hcho_enabled\":false,\"co2_enabled\":false}}",
            current);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, result.update.hcho.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 220.0f, result.update.voc.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, result.update.nox.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, current.co.orange, result.update.co.orange);
    TEST_ASSERT_FALSE(result.update.background_alerts.hcho_enabled);
    TEST_ASSERT_TRUE(result.update.background_alerts.co_enabled);
    TEST_ASSERT_FALSE(result.update.background_alerts.co2_enabled);
}

void test_web_threshold_api_utils_fill_state_json_includes_factory_and_active() {
    DisplayThresholds::Config active = DisplayThresholds::defaults();
    active.co2.green = 700.0f;
    active.background_alerts.co_enabled = false;

    ArduinoJson::JsonDocument doc;
    WebThresholdApiUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(),
                                        active,
                                        DisplayThresholds::defaults());

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_INT(1, doc["version"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 700.0f, doc["metrics"]["co2"]["green"].as<float>());
    TEST_ASSERT_FALSE(doc["background_alerts"]["co_enabled"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, doc["metrics"]["voc"]["green"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, doc["metrics"]["nox"]["orange"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, doc["factory_metrics"]["co2"]["green"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, doc["factory_metrics"]["voc"]["yellow"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, doc["factory_metrics"]["nox"]["green"].as<float>());
    TEST_ASSERT_TRUE(doc["factory_background_alerts"]["co_enabled"].as<bool>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_threshold_api_utils_parse_rejects_missing_and_bad_json);
    RUN_TEST(test_web_threshold_api_utils_parse_rejects_invalid_without_update);
    RUN_TEST(test_web_threshold_api_utils_parse_rejects_non_numeric_fields);
    RUN_TEST(test_web_threshold_api_utils_parse_rejects_non_boolean_switches);
    RUN_TEST(test_web_threshold_api_utils_parse_rejects_negative_rh_and_ah_thresholds);
    RUN_TEST(test_web_threshold_api_utils_parse_accepts_switch_and_metric_update);
    RUN_TEST(test_web_threshold_api_utils_fill_state_json_includes_factory_and_active);
    return UNITY_END();
}
