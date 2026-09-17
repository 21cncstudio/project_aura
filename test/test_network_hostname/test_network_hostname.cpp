#include <unity.h>

#include <cstring>
#include "core/NetworkHostname.h"

namespace {
constexpr const char *kExpected = "aura-1fc944";
struct Netif {
    const char *name = "esp32s3-1FC944";
    int32_t set_error = 0;
    int32_t read_error = 0;
    bool accept_name = true;
    static int32_t set(void *ctx, const char *name) {
        auto &self = *static_cast<Netif *>(ctx);
        if (self.set_error == 0 && self.accept_name) self.name = name;
        return self.set_error;
    }
    static int32_t get(void *ctx, const char **name) {
        auto &self = *static_cast<Netif *>(ctx);
        *name = self.name;
        return self.read_error;
    }
};
}

void setUp() {}
void tearDown() {}

void test_success_requires_live_readback_not_just_setter_success() {
    NetworkHostname host;
    Netif netif;
    netif.accept_name = false;
    TEST_ASSERT_FALSE(host.apply(kExpected, &netif, Netif::set, Netif::get, 100));
    TEST_ASSERT_EQUAL_STRING("esp32s3-1FC944", host.snapshot().actual);
    TEST_ASSERT_FALSE(host.snapshot().matches);
    TEST_ASSERT_EQUAL_INT32(NetworkHostname::kVerificationFailed, host.snapshot().apply_error);
    TEST_ASSERT_EQUAL_UINT32(1, host.snapshot().apply_failures);
}

void test_setter_error_is_failure_even_when_previous_name_matches() {
    NetworkHostname host;
    Netif netif;
    netif.name = kExpected;
    netif.set_error = 0x101;
    TEST_ASSERT_FALSE(host.apply(kExpected, &netif, Netif::set, Netif::get, 100));
    TEST_ASSERT_TRUE(host.snapshot().matches);
    TEST_ASSERT_EQUAL_INT32(0x101, host.snapshot().apply_error);
}

void test_failed_readback_or_null_name_cannot_pass_verification() {
    NetworkHostname host;
    Netif netif;
    netif.read_error = 0x103;
    TEST_ASSERT_FALSE(host.apply(kExpected, &netif, Netif::set, Netif::get, 100));
    TEST_ASSERT_FALSE(host.snapshot().available);
    TEST_ASSERT_EQUAL_INT32(0x103, host.snapshot().apply_error);
    netif.read_error = 0;
    netif.name = nullptr;
    netif.accept_name = false;
    TEST_ASSERT_FALSE(host.apply(kExpected, &netif, Netif::set, Netif::get, 200));
    TEST_ASSERT_FALSE(host.snapshot().matches);
    TEST_ASSERT_EQUAL_UINT32(2, host.snapshot().apply_failures);
}

void test_recovery_keeps_failure_history_and_clears_stale_live_name_on_stop() {
    NetworkHostname host;
    Netif netif;
    netif.set_error = 0x101;
    TEST_ASSERT_FALSE(host.apply(kExpected, &netif, Netif::set, Netif::get, 100));
    netif.set_error = 0;
    TEST_ASSERT_TRUE(host.apply(kExpected, &netif, Netif::set, Netif::get, 200));
    TEST_ASSERT_EQUAL_STRING(kExpected, host.snapshot().actual);
    TEST_ASSERT_TRUE(host.snapshot().matches);
    TEST_ASSERT_EQUAL_INT32(0, host.snapshot().apply_error);
    TEST_ASSERT_EQUAL_UINT32(1, host.snapshot().apply_failures);
    TEST_ASSERT_EQUAL_INT32(0x101, host.snapshot().last_failure_error);
    host.clearLive();
    TEST_ASSERT_FALSE(host.snapshot().available);
    TEST_ASSERT_FALSE(host.snapshot().checked);
    TEST_ASSERT_EQUAL_STRING("", host.snapshot().actual);
    TEST_ASSERT_EQUAL_UINT32(1, host.snapshot().apply_failures);
    TEST_ASSERT_EQUAL_INT32(0x101, host.snapshot().last_failure_error);
}

void test_periodic_observation_detects_reconnect_drift_without_rewriting_it() {
    NetworkHostname host;
    Netif netif;
    TEST_ASSERT_TRUE(host.apply(kExpected, &netif, Netif::set, Netif::get, 100));
    char changed[] = "esp32s3-1FC944";
    netif.name = changed;
    host.observe(kExpected, &netif, Netif::get, 200);
    TEST_ASSERT_FALSE(host.snapshot().matches);
    TEST_ASSERT_EQUAL_STRING(changed, host.snapshot().actual);
    host.observe(kExpected, &netif, Netif::get, 300);
    TEST_ASSERT_EQUAL_UINT32(1, host.snapshot().mismatch_count);
    changed[0] = 'X';
    TEST_ASSERT_EQUAL_STRING("esp32s3-1FC944", host.snapshot().actual);
    netif.name = kExpected;
    host.observe(kExpected, &netif, Netif::get, 400);
    netif.name = "esp32s3-1FC944";
    host.observe(kExpected, &netif, Netif::get, 500);
    TEST_ASSERT_EQUAL_UINT32(2, host.snapshot().mismatch_count);
    TEST_ASSERT_EQUAL_UINT32(500, host.snapshot().checked_at_ms);
    TEST_ASSERT_EQUAL_UINT32(0, host.snapshot().apply_failures);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_success_requires_live_readback_not_just_setter_success);
    RUN_TEST(test_setter_error_is_failure_even_when_previous_name_matches);
    RUN_TEST(test_failed_readback_or_null_name_cannot_pass_verification);
    RUN_TEST(test_recovery_keeps_failure_history_and_clears_stale_live_name_on_stop);
    RUN_TEST(test_periodic_observation_detects_reconnect_drift_without_rewriting_it);
    return UNITY_END();
}
