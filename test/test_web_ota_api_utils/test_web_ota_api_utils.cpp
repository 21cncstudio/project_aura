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
    TEST_ASSERT_TRUE(doc["error_code"].isNull());
}

void test_web_ota_api_utils_build_update_result_reports_missing_file_as_bad_request() {
    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(false, false, 0, 8192, false, 0, "");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("MISSING_FILE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Firmware file is missing", result.error.c_str());

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillUpdateJson(doc.to<ArduinoJson::JsonObject>(), result);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_FALSE(doc["rebooting"].as<bool>());
    TEST_ASSERT_TRUE(doc["expected"].isNull());
    TEST_ASSERT_EQUAL_STRING("MISSING_FILE", doc["error_code"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Firmware file is missing", doc["error"].as<const char *>());
}

void test_web_ota_api_utils_build_update_result_reports_oversized_image_as_payload_too_large() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 2048, 4096, true, 8192, "Firmware too large for OTA slot: 8192 > 4096");

    TEST_ASSERT_EQUAL_INT(413, result.status_code);
    TEST_ASSERT_EQUAL_STRING("IMAGE_TOO_LARGE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Firmware too large for OTA slot: 8192 > 4096",
                             result.error.c_str());
}

void test_web_ota_api_utils_build_update_result_reports_timeout_with_specific_code() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 154048, 6553600, true, 3717792, "Upload timed out after 5000 ms without data");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(408, result.status_code);
    TEST_ASSERT_EQUAL_STRING("UPLOAD_TIMEOUT", result.error_code.c_str());

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillUpdateJson(doc.to<ArduinoJson::JsonObject>(), result);
    TEST_ASSERT_EQUAL_STRING("UPLOAD_TIMEOUT", doc["error_code"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Upload timed out after 5000 ms without data",
                             doc["error"].as<const char *>());
}

void test_web_ota_api_utils_build_update_result_reports_interrupt_with_specific_code() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 154048, 6553600, true, 3717792, "Upload interrupted after 5000 ms without data");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("UPLOAD_ABORTED", result.error_code.c_str());
}

void test_web_ota_api_utils_build_update_result_reports_client_disconnect_separately() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 154048, 6553600, true, 3717792, "Upload interrupted: client disconnected");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(499, result.status_code);
    TEST_ASSERT_EQUAL_STRING("CLIENT_DISCONNECTED", result.error_code.c_str());
}

void test_web_ota_api_utils_build_update_result_reports_total_deadline_as_timeout() {
    const WebOtaApiUtils::Result result = WebOtaApiUtils::buildUpdateResult(
        true, false, 154048, 6553600, true, 3717792, "Upload timed out after total deadline of 300000 ms");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(408, result.status_code);
    TEST_ASSERT_EQUAL_STRING("UPLOAD_TIMEOUT", result.error_code.c_str());
}

void test_web_ota_api_utils_build_update_result_reports_invalid_size() {
    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(true, false, 0, 0, false, 0, "Firmware size is required");

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.rebooting);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("INVALID_SIZE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Firmware size is required", result.error.c_str());
}

void test_web_ota_api_utils_build_prepare_result_reports_success_payload() {
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(true, true, true, 6553600, true, 3713984, 840000);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(200, result.status_code);
    TEST_ASSERT_EQUAL_UINT32(6553600, static_cast<uint32_t>(result.slot_size));
    TEST_ASSERT_EQUAL_UINT32(3713984, static_cast<uint32_t>(result.expected_size));
    TEST_ASSERT_EQUAL_UINT32(840000, result.upload_timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(870000, result.response_wait_ms);
    TEST_ASSERT_EQUAL_STRING("Device ready for firmware upload", result.message.c_str());

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillPrepareJson(doc.to<ArduinoJson::JsonObject>(), result);
    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(6553600, doc["slot_size"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(3713984, doc["expected"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(840000, doc["upload_timeout_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(870000, doc["response_wait_ms"].as<uint32_t>());
}

void test_web_ota_api_utils_build_prepare_result_rejects_oversized_image() {
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(true, true, true, 4096, true, 8192, 600000);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(413, result.status_code);
    TEST_ASSERT_EQUAL_STRING("IMAGE_TOO_LARGE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Firmware too large for OTA slot: 8192 > 4096", result.error.c_str());
}

void test_web_ota_api_utils_build_prepare_result_reports_invalid_size() {
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(true, true, false, 6553600, false, 0, 900000);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("INVALID_SIZE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Invalid firmware size", result.error.c_str());
}

void test_web_ota_api_utils_build_prepare_result_requires_size() {
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(true, false, false, 6553600, false, 0, 900000);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("INVALID_SIZE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("Firmware size is required", result.error.c_str());
}

void test_web_ota_api_utils_build_prepare_result_reports_unavailable_state() {
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(false, true, true, 0, true, 1234, 0);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(503, result.status_code);
    TEST_ASSERT_EQUAL_STRING("OTA_PREPARE_UNAVAILABLE", result.error_code.c_str());
    TEST_ASSERT_EQUAL_STRING("OTA prepare unavailable", result.error.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_success_payload);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_missing_file_as_bad_request);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_oversized_image_as_payload_too_large);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_timeout_with_specific_code);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_interrupt_with_specific_code);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_client_disconnect_separately);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_total_deadline_as_timeout);
    RUN_TEST(test_web_ota_api_utils_build_update_result_reports_invalid_size);
    RUN_TEST(test_web_ota_api_utils_build_prepare_result_reports_success_payload);
    RUN_TEST(test_web_ota_api_utils_build_prepare_result_rejects_oversized_image);
    RUN_TEST(test_web_ota_api_utils_build_prepare_result_reports_invalid_size);
    RUN_TEST(test_web_ota_api_utils_build_prepare_result_requires_size);
    RUN_TEST(test_web_ota_api_utils_build_prepare_result_reports_unavailable_state);
    return UNITY_END();
}
