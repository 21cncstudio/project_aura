#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebOtaApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_ota_api_utils_build_update_result_reports_success_payload() {
    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(true, true, 1234, 4096, true, 1234, "");

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(200, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Firmware uploaded. Device will reboot.", result.message.c_str());
    TEST_ASSERT_TRUE(result.error.length() == 0);

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillUpdateJson(doc.to<ArduinoJson::JsonObject>(), result);
    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_TRUE(doc["rebooting"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(1234, doc["written"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(4096, doc["slot_size"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(1234, doc["expected"].as<uint32_t>());
}

void test_web_ota_api_utils_build_update_result_reports_missing_file_as_bad_request() {
    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(false, false, 0, 8192, false, 0, "");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Firmware file is missing", result.error.c_str());

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillUpdateJson(doc.to<ArduinoJson::JsonObject>(), result);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_FALSE(doc["rebooting"].as<bool>());
    TEST_ASSERT_TRUE(doc["expected"].isNull());
    TEST_ASSERT_EQUAL_STRING("Firmware file is missing", doc["error"].as<const char *>());
}

void test_web_ota_api_utils_build_update_result_reports_oversized_image_as_payload_too_large() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 2048, 4096, true, 8192, "Firmware too large for OTA slot: 8192 > 4096");

    TEST_ASSERT_EQUAL_INT(413, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Firmware too large for OTA slot: 8192 > 4096",
                             result.error.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_success_payload);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_missing_file_as_bad_request);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_oversized_image_as_payload_too_large);
    return UNITY_END();
}
