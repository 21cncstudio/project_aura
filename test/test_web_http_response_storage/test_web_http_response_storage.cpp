#include <unity.h>

#include <string>

#include "web/WebHttpResponseStorage.h"

namespace {

WebHttpResponseStorage::HeaderView store_temporary_header(
    WebHttpResponseStorage::Storage &storage,
    const char *name,
    const char *value) {
    WebHttpResponseStorage::HeaderView stored;
    TEST_ASSERT_TRUE(storage.storeHeader(name, String(value), stored));
    return stored;
}

const char *store_temporary_status(WebHttpResponseStorage::Storage &storage,
                                   const char *value) {
    const char *stored = nullptr;
    TEST_ASSERT_TRUE(storage.storeStatus(String(value), stored));
    return stored;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_response_storage_keeps_temporary_metadata_alive_until_send() {
    WebHttpResponseStorage::Storage storage;

    const char *status = store_temporary_status(storage, "200 OK");
    const char *content_type = nullptr;
    TEST_ASSERT_TRUE(storage.storeContentType("application/json", content_type));
    const WebHttpResponseStorage::HeaderView cache_control =
        store_temporary_header(storage, "Cache-Control", "no-store");

    TEST_ASSERT_EQUAL_STRING("200 OK", status);
    TEST_ASSERT_EQUAL_STRING("application/json", content_type);
    TEST_ASSERT_EQUAL_STRING("Cache-Control", cache_control.name);
    TEST_ASSERT_EQUAL_STRING("no-store", cache_control.value);
}

void test_response_storage_does_not_relocate_earlier_headers() {
    WebHttpResponseStorage::Storage storage;
    const WebHttpResponseStorage::HeaderView first =
        store_temporary_header(storage, "Cache-Control", "no-store, no-cache");

    for (size_t i = 1; i < WebHttpResponseStorage::Storage::kMaxHeaders; ++i) {
        const String name = String("X-Test-") + std::to_string(i);
        const String value = String("value-") + std::to_string(i);
        WebHttpResponseStorage::HeaderView stored;
        TEST_ASSERT_TRUE(storage.storeHeader(name.c_str(), value, stored));
    }

    TEST_ASSERT_EQUAL_STRING("Cache-Control", first.name);
    TEST_ASSERT_EQUAL_STRING("no-store, no-cache", first.value);
    TEST_ASSERT_EQUAL_UINT32(WebHttpResponseStorage::Storage::kMaxHeaders,
                             storage.headerCount());
}

void test_response_storage_rejects_overflow_and_reuses_after_reset() {
    WebHttpResponseStorage::Storage storage;
    for (size_t i = 0; i < WebHttpResponseStorage::Storage::kMaxHeaders; ++i) {
        WebHttpResponseStorage::HeaderView stored;
        TEST_ASSERT_TRUE(storage.storeHeader("X-Test", std::to_string(i), stored));
    }

    WebHttpResponseStorage::HeaderView overflow;
    TEST_ASSERT_FALSE(storage.storeHeader("X-Overflow", "no", overflow));
    TEST_ASSERT_NULL(overflow.name);
    TEST_ASSERT_NULL(overflow.value);

    storage.reset();
    TEST_ASSERT_EQUAL_UINT32(0, storage.headerCount());
    const WebHttpResponseStorage::HeaderView reused =
        store_temporary_header(storage, "Pragma", "no-cache");
    TEST_ASSERT_EQUAL_STRING("Pragma", reused.name);
    TEST_ASSERT_EQUAL_STRING("no-cache", reused.value);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_response_storage_keeps_temporary_metadata_alive_until_send);
    RUN_TEST(test_response_storage_does_not_relocate_earlier_headers);
    RUN_TEST(test_response_storage_rejects_overflow_and_reuses_after_reset);
    return UNITY_END();
}
