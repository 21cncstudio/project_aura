#include <unity.h>

#include <cstdint>

#include "ui/UiDeferredUnload.h"

void setUp() {}
void tearDown() {}

void test_unload_waits_for_deadline_across_millis_wraparound() {
    UiDeferredUnload unload;
    unload.reset();
    const int screen_id = unload.screenId(0);
    const uint32_t started_at = UINT32_MAX - 149U;

    unload.scheduleOnSwitch(screen_id, 999, started_at);

    TEST_ASSERT_FALSE(unload.ready(0, started_at + 299U, 0, 999));
    TEST_ASSERT_TRUE(unload.ready(0, started_at + 300U, 0, 999));
}

void test_wrapped_zero_deadline_remains_scheduled() {
    UiDeferredUnload unload;
    unload.reset();
    const int screen_id = unload.screenId(0);
    const uint32_t started_at = UINT32_MAX - 299U;

    unload.scheduleOnSwitch(screen_id, 999, started_at);

    TEST_ASSERT_FALSE(unload.ready(0, UINT32_MAX, 0, 999));
    TEST_ASSERT_TRUE(unload.ready(0, 0, 0, 999));
}

void test_pending_or_reentered_screen_cancels_unload() {
    UiDeferredUnload unload;
    unload.reset();
    const int screen_id = unload.screenId(0);

    unload.scheduleOnSwitch(screen_id, 999, 1000);
    TEST_ASSERT_FALSE(unload.ready(0, 1300, screen_id, 999));
    TEST_ASSERT_FALSE(unload.ready(0, 1300, 0, screen_id));

    unload.scheduleOnSwitch(999, screen_id, 1300);
    TEST_ASSERT_FALSE(unload.ready(0, 2000, 0, 999));
}

void test_retry_uses_wrap_safe_deadline() {
    UiDeferredUnload unload;
    unload.reset();
    const uint32_t started_at = UINT32_MAX - 49U;

    unload.retry(0, started_at);

    TEST_ASSERT_FALSE(unload.ready(0, started_at + 99U, 0, 999));
    TEST_ASSERT_TRUE(unload.ready(0, started_at + 100U, 0, 999));
    unload.clear(0);
    TEST_ASSERT_FALSE(unload.ready(0, started_at + 1000U, 0, 999));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_unload_waits_for_deadline_across_millis_wraparound);
    RUN_TEST(test_wrapped_zero_deadline_remains_scheduled);
    RUN_TEST(test_pending_or_reentered_screen_cancels_unload);
    RUN_TEST(test_retry_uses_wrap_safe_deadline);
    return UNITY_END();
}
