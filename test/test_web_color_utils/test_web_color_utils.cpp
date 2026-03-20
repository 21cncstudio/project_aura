#include <unity.h>

#include "web/WebColorUtils.h"

void setUp() {}
void tearDown() {}

void test_web_color_utils_rgb_to_hex_string_formats_uppercase() {
    TEST_ASSERT_EQUAL_STRING("#000000", WebColorUtils::rgbToHexString(0x000000).c_str());
    TEST_ASSERT_EQUAL_STRING("#12ABEF", WebColorUtils::rgbToHexString(0x12ABEF).c_str());
}

void test_web_color_utils_parse_hex_color_rgb_accepts_hash_and_plain() {
    uint32_t rgb = 0;
    TEST_ASSERT_TRUE(WebColorUtils::parseHexColorRgb("#12abEF", rgb));
    TEST_ASSERT_EQUAL_HEX32(0x12ABEF, rgb);
    TEST_ASSERT_TRUE(WebColorUtils::parseHexColorRgb(" 00ff7f ", rgb));
    TEST_ASSERT_EQUAL_HEX32(0x00FF7F, rgb);
}

void test_web_color_utils_parse_hex_color_rgb_rejects_invalid_strings() {
    uint32_t rgb = 0;
    TEST_ASSERT_FALSE(WebColorUtils::parseHexColorRgb("", rgb));
    TEST_ASSERT_FALSE(WebColorUtils::parseHexColorRgb("#12345", rgb));
    TEST_ASSERT_FALSE(WebColorUtils::parseHexColorRgb("#1234567", rgb));
    TEST_ASSERT_FALSE(WebColorUtils::parseHexColorRgb("#ZZ00AA", rgb));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_color_utils_rgb_to_hex_string_formats_uppercase);
    RUN_TEST(test_web_color_utils_parse_hex_color_rgb_accepts_hash_and_plain);
    RUN_TEST(test_web_color_utils_parse_hex_color_rgb_rejects_invalid_strings);
    return UNITY_END();
}
