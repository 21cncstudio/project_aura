#include <unity.h>

#include "core/MqttClientLifecycle.h"

void setUp() {}
void tearDown() {}

namespace {

struct FakeClient {
    int id = 0;
};

} // namespace

void test_destroy_owned_is_noop_without_client() {
    FakeClient *client = nullptr;
    unsigned calls = 0;

    TEST_ASSERT_TRUE(MqttClientLifecycle::destroyOwned(
        client, [&calls](FakeClient *) {
            ++calls;
            return true;
        }));
    TEST_ASSERT_NULL(client);
    TEST_ASSERT_EQUAL_UINT32(0, calls);
}

void test_destroy_owned_clears_handle_only_after_success() {
    FakeClient storage{42};
    FakeClient *client = &storage;
    unsigned calls = 0;

    TEST_ASSERT_TRUE(MqttClientLifecycle::destroyOwned(
        client, [&calls](FakeClient *observed) {
            ++calls;
            TEST_ASSERT_EQUAL_INT(42, observed->id);
            return true;
        }));
    TEST_ASSERT_NULL(client);
    TEST_ASSERT_EQUAL_UINT32(1, calls);
}

void test_destroy_owned_retains_handle_for_retry_after_failure() {
    FakeClient storage{7};
    FakeClient *client = &storage;
    unsigned calls = 0;

    TEST_ASSERT_FALSE(MqttClientLifecycle::destroyOwned(
        client, [&calls](FakeClient *) {
            ++calls;
            return false;
        }));
    TEST_ASSERT_EQUAL_PTR(&storage, client);
    TEST_ASSERT_EQUAL_UINT32(1, calls);

    TEST_ASSERT_TRUE(MqttClientLifecycle::destroyOwned(
        client, [&calls](FakeClient *) {
            ++calls;
            return true;
        }));
    TEST_ASSERT_NULL(client);
    TEST_ASSERT_EQUAL_UINT32(2, calls);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_destroy_owned_is_noop_without_client);
    RUN_TEST(test_destroy_owned_clears_handle_only_after_success);
    RUN_TEST(test_destroy_owned_retains_handle_for_retry_after_failure);
    return UNITY_END();
}
