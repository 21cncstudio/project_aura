#include <unity.h>

#include "web/WebWifiSaveUtils.h"

void setUp() {}
void tearDown() {}

void test_web_wifi_save_utils_parse_accepts_valid_input() {
    const WebWifiSaveUtils::ParseResult result =
        WebWifiSaveUtils::parseSaveInput("AuraNet", "secret");

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("AuraNet", result.update.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", result.update.pass.c_str());
    TEST_ASSERT_TRUE(result.update.enabled);
}

void test_web_wifi_save_utils_parse_rejects_missing_ssid_and_bad_password() {
    WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput("", "secret");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("SSID required", result.error_message.c_str());

    result = WebWifiSaveUtils::parseSaveInput("AuraNet", "bad\npass");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Password contains unsupported control characters",
                             result.error_message.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_wifi_save_utils_parse_accepts_valid_input);
    RUN_TEST(test_web_wifi_save_utils_parse_rejects_missing_ssid_and_bad_password);
    return UNITY_END();
}
