#include <unity.h>

#include "web/WebMultipart.h"

void setUp() {}
void tearDown() {}

void test_parse_boundary_extracts_plain_value() {
    const String content_type = "multipart/form-data; boundary=----WebKitFormBoundaryabc123";
    TEST_ASSERT_EQUAL_STRING("----WebKitFormBoundaryabc123",
                             WebMultipart::parseBoundary(content_type).c_str());
}

void test_parse_boundary_extracts_quoted_value_and_trims_spaces() {
    const String content_type = "multipart/form-data; charset=utf-8; boundary=\"  xyz-123  \"";
    TEST_ASSERT_EQUAL_STRING("xyz-123", WebMultipart::parseBoundary(content_type).c_str());
}

void test_parse_boundary_returns_empty_when_missing() {
    TEST_ASSERT_TRUE(WebMultipart::parseBoundary("multipart/form-data").length() == 0);
}

void test_parse_content_disposition_extracts_name_and_filename() {
    String name;
    String filename;
    const bool ok = WebMultipart::parseContentDisposition(
        "Content-Disposition: form-data; name=\"firmware\"; filename=\"aura.bin\"",
        name,
        filename);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("firmware", name.c_str());
    TEST_ASSERT_EQUAL_STRING("aura.bin", filename.c_str());
}

void test_parse_content_disposition_accepts_field_without_filename() {
    String name;
    String filename;
    const bool ok = WebMultipart::parseContentDisposition(
        "content-disposition: form-data; name=\"ota_size\"",
        name,
        filename);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("ota_size", name.c_str());
    TEST_ASSERT_TRUE(filename.length() == 0);
}

void test_parse_content_disposition_rejects_other_headers() {
    String name;
    String filename;
    TEST_ASSERT_FALSE(WebMultipart::parseContentDisposition(
        "Content-Type: application/octet-stream",
        name,
        filename));
    TEST_ASSERT_FALSE(WebMultipart::parseContentDisposition(
        "Content-Disposition: attachment; filename=\"x.bin\"",
        name,
        filename));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_boundary_extracts_plain_value);
    RUN_TEST(test_parse_boundary_extracts_quoted_value_and_trims_spaces);
    RUN_TEST(test_parse_boundary_returns_empty_when_missing);
    RUN_TEST(test_parse_content_disposition_extracts_name_and_filename);
    RUN_TEST(test_parse_content_disposition_accepts_field_without_filename);
    RUN_TEST(test_parse_content_disposition_rejects_other_headers);
    return UNITY_END();
}
