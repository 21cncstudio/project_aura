#include <unity.h>

#include <vector>

#include "web/WebQueryString.h"

void setUp() {}
void tearDown() {}

void test_url_decode_decodes_percent_and_plus() {
    const String decoded = WebQueryString::urlDecode("Aura+Device%2FOffice");
    TEST_ASSERT_EQUAL_STRING("Aura Device/Office", decoded.c_str());
}

void test_url_decode_keeps_invalid_percent_sequences_literal() {
    const String decoded = WebQueryString::urlDecode("abc%2G%z1");
    TEST_ASSERT_EQUAL_STRING("abc%2G%z1", decoded.c_str());
}

void test_parse_args_extracts_keys_values_and_empty_value() {
    std::vector<WebQueryArg> args;
    WebQueryString::parseArgs("ssid=My+WiFi&pass=abc%20123&empty=", args);
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(args.size()));
    TEST_ASSERT_EQUAL_STRING("ssid", args[0].key.c_str());
    TEST_ASSERT_EQUAL_STRING("My WiFi", args[0].value.c_str());
    TEST_ASSERT_EQUAL_STRING("pass", args[1].key.c_str());
    TEST_ASSERT_EQUAL_STRING("abc 123", args[1].value.c_str());
    TEST_ASSERT_EQUAL_STRING("empty", args[2].key.c_str());
    TEST_ASSERT_EQUAL_STRING("", args[2].value.c_str());
}

void test_parse_args_accepts_flag_without_value_and_skips_empty_segments() {
    std::vector<WebQueryArg> args;
    WebQueryString::parseArgs("flag&&mode=auto&", args);
    TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(args.size()));
    TEST_ASSERT_EQUAL_STRING("flag", args[0].key.c_str());
    TEST_ASSERT_EQUAL_STRING("", args[0].value.c_str());
    TEST_ASSERT_EQUAL_STRING("mode", args[1].key.c_str());
    TEST_ASSERT_EQUAL_STRING("auto", args[1].value.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_url_decode_decodes_percent_and_plus);
    RUN_TEST(test_url_decode_keeps_invalid_percent_sequences_literal);
    RUN_TEST(test_parse_args_extracts_keys_values_and_empty_value);
    RUN_TEST(test_parse_args_accepts_flag_without_value_and_skips_empty_segments);
    return UNITY_END();
}
