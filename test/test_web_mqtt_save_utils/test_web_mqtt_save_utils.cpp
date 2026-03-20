#include <unity.h>

#include "config/AppConfig.h"
#include "web/WebMqttSaveUtils.h"

void setUp() {}
void tearDown() {}

void test_web_mqtt_save_utils_parse_accepts_and_normalizes_valid_payload() {
    WebMqttSaveUtils::SaveInput input{};
    input.host = "  mqtt.local  ";
    input.port = " 1884 ";
    input.user = "";
    input.pass = "";
    input.device_name = "  Aura Screen ";
    input.base_topic = " aura/main/ ";
    input.anonymous = true;
    input.discovery = true;

    WebMqttSaveUtils::CurrentCredentials current{};
    current.user = "stored-user";
    current.pass = "stored-pass";

    const WebMqttSaveUtils::ParseResult result =
        WebMqttSaveUtils::parseSaveInput(input, current);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("mqtt.local", result.update.host.c_str());
    TEST_ASSERT_EQUAL_UINT16(1884, result.update.port);
    TEST_ASSERT_EQUAL_STRING("Aura Screen", result.update.device_name.c_str());
    TEST_ASSERT_EQUAL_STRING("aura/main", result.update.base_topic.c_str());
    TEST_ASSERT_EQUAL_STRING("stored-user", result.update.user.c_str());
    TEST_ASSERT_EQUAL_STRING("stored-pass", result.update.pass.c_str());
    TEST_ASSERT_TRUE(result.update.anonymous);
    TEST_ASSERT_TRUE(result.update.discovery);
}

void test_web_mqtt_save_utils_parse_rejects_invalid_payloads() {
    WebMqttSaveUtils::SaveInput input{};
    input.device_name = "Aura";
    input.base_topic = "aura/main";

    WebMqttSaveUtils::ParseResult result =
        WebMqttSaveUtils::parseSaveInput(input, {});
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Broker address required", result.error_message.c_str());

    input.host = "mqtt.local";
    input.base_topic = "aura/+";
    result = WebMqttSaveUtils::parseSaveInput(input, {});
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Base topic must not include MQTT wildcards (+ or #)",
                             result.error_message.c_str());

    input.base_topic = "aura/main";
    input.port = "70000";
    result = WebMqttSaveUtils::parseSaveInput(input, {});
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Port must be in range 1-65535", result.error_message.c_str());

    input.port = "";
    input.anonymous = false;
    result = WebMqttSaveUtils::parseSaveInput(input, {});
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Username and password are required when anonymous mode is disabled",
                             result.error_message.c_str());

    input.anonymous = true;
    input.device_name = "bad\nname";
    result = WebMqttSaveUtils::parseSaveInput(input, {});
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Fields contain unsupported control characters",
                             result.error_message.c_str());
}

void test_web_mqtt_save_utils_parse_uses_default_port_when_empty() {
    WebMqttSaveUtils::SaveInput input{};
    input.host = "mqtt.local";
    input.user = "user";
    input.pass = "pass";
    input.device_name = "Aura";
    input.base_topic = "aura/main";

    const WebMqttSaveUtils::ParseResult result =
        WebMqttSaveUtils::parseSaveInput(input, {});

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT16(Config::MQTT_DEFAULT_PORT, result.update.port);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_mqtt_save_utils_parse_accepts_and_normalizes_valid_payload);
    RUN_TEST(test_web_mqtt_save_utils_parse_rejects_invalid_payloads);
    RUN_TEST(test_web_mqtt_save_utils_parse_uses_default_port_when_empty);
    return UNITY_END();
}
