#include <unity.h>

#include "web/WebTextUtils.h"

void setUp() {}
void tearDown() {}

void test_web_text_utils_html_escape_escapes_reserved_chars() {
    const String escaped = WebTextUtils::htmlEscape("<tag attr=\"x\">Tom & 'Jerry'</tag>");
    TEST_ASSERT_EQUAL_STRING("&lt;tag attr=&quot;x&quot;&gt;Tom &amp; &#39;Jerry&#39;&lt;/tag&gt;",
                             escaped.c_str());
}

void test_web_text_utils_wifi_rssi_to_quality_clamps_and_scales() {
    TEST_ASSERT_EQUAL(0, WebTextUtils::wifiRssiToQuality(-120));
    TEST_ASSERT_EQUAL(0, WebTextUtils::wifiRssiToQuality(-100));
    TEST_ASSERT_EQUAL(60, WebTextUtils::wifiRssiToQuality(-70));
    TEST_ASSERT_EQUAL(100, WebTextUtils::wifiRssiToQuality(-50));
    TEST_ASSERT_EQUAL(100, WebTextUtils::wifiRssiToQuality(-20));
}

void test_web_text_utils_detects_control_chars_and_wildcards() {
    TEST_ASSERT_FALSE(WebTextUtils::hasControlChars("Aura"));
    TEST_ASSERT_TRUE(WebTextUtils::hasControlChars("Au\ra"));
    TEST_ASSERT_FALSE(WebTextUtils::mqttTopicHasWildcards("aura/device/state"));
    TEST_ASSERT_TRUE(WebTextUtils::mqttTopicHasWildcards("aura/+/state"));
    TEST_ASSERT_TRUE(WebTextUtils::mqttTopicHasWildcards("aura/#"));
}

void test_web_text_utils_parse_positive_size_trims_and_validates() {
    size_t parsed = 0;
    TEST_ASSERT_TRUE(WebTextUtils::parsePositiveSize(" 4096 ", parsed));
    TEST_ASSERT_EQUAL_UINT32(4096, static_cast<uint32_t>(parsed));
    TEST_ASSERT_FALSE(WebTextUtils::parsePositiveSize("", parsed));
    TEST_ASSERT_FALSE(WebTextUtils::parsePositiveSize("0", parsed));
    TEST_ASSERT_FALSE(WebTextUtils::parsePositiveSize("12KB", parsed));
}

void test_web_text_utils_wifi_label_safe_sanitizes_non_printable() {
    TEST_ASSERT_EQUAL_STRING("---", WebTextUtils::wifiLabelSafe("").c_str());
    TEST_ASSERT_EQUAL_STRING("Aura???", WebTextUtils::wifiLabelSafe("Aura\t\r\n").c_str());
    TEST_ASSERT_EQUAL_STRING("Meotida", WebTextUtils::wifiLabelSafe("Meotida").c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_text_utils_html_escape_escapes_reserved_chars);
    RUN_TEST(test_web_text_utils_wifi_rssi_to_quality_clamps_and_scales);
    RUN_TEST(test_web_text_utils_detects_control_chars_and_wildcards);
    RUN_TEST(test_web_text_utils_parse_positive_size_trims_and_validates);
    RUN_TEST(test_web_text_utils_wifi_label_safe_sanitizes_non_printable);
    return UNITY_END();
}
