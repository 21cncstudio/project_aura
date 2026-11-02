#include <unity.h>
#include "web/WebDeferredActionsState.h"

static bool scan_ready;
static unsigned prepare_calls;
static bool prepare_wifi() { ++prepare_calls; return scan_ready; }
void setUp() { scan_ready = true; prepare_calls = 0; }
void tearDown() {}

void test_wifi_grace_starts_at_completion_not_request() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    state.scheduleMqttSync(100, 300);
    scan_ready = false;
    TEST_ASSERT_FALSE(state.pollDue(100, prepare_wifi).wifi_start_sta);
    auto due = state.pollDue(600, prepare_wifi);
    TEST_ASSERT_FALSE(due.wifi_start_sta);
    TEST_ASSERT_TRUE(due.mqtt_sync); // MQTT is not held behind Wi-Fi.
    scan_ready = true;
    TEST_ASSERT_FALSE(state.pollDue(1000, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(1199, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(1200, prepare_wifi).wifi_start_sta);
    const auto calls = prepare_calls;
    TEST_ASSERT_FALSE(state.pollDue(9000, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_EQUAL_UINT(calls, prepare_calls);
}
void test_no_scan_still_gets_full_grace() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    TEST_ASSERT_FALSE(state.pollDue(100, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(299, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(300, prepare_wifi).wifi_start_sta);
}
void test_later_scan_invalidates_elapsed_grace() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    state.pollDue(100, prepare_wifi);
    scan_ready = false;
    TEST_ASSERT_FALSE(state.pollDue(299, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(800, prepare_wifi).wifi_start_sta);
    scan_ready = true;
    TEST_ASSERT_FALSE(state.pollDue(900, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(1099, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(1100, prepare_wifi).wifi_start_sta);
}
void test_missing_completion_hook_does_not_switch_radio() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    TEST_ASSERT_FALSE(state.pollDue(10000).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(20000, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(20200, prepare_wifi).wifi_start_sta);
}
void test_reschedule_requires_new_full_grace() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    state.pollDue(100, prepare_wifi);
    state.scheduleWifiStartSta(200);
    TEST_ASSERT_FALSE(state.pollDue(290, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(489, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(490, prepare_wifi).wifi_start_sta);
}
void test_reset_cancels_pending_work() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    state.scheduleMqttSync(0, 100);
    state.pollDue(0, prepare_wifi);
    state.reset();
    const auto calls = prepare_calls;
    const auto due = state.pollDue(1000, prepare_wifi);
    TEST_ASSERT_FALSE(due.wifi_start_sta);
    TEST_ASSERT_FALSE(due.mqtt_sync);
    TEST_ASSERT_EQUAL_UINT(calls, prepare_calls);
}
void test_grace_handles_clock_wrap() {
    WebDeferredActionsState state;
    state.scheduleWifiStartSta(200);
    TEST_ASSERT_FALSE(state.pollDue(0xffffff9cU, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_FALSE(state.pollDue(99, prepare_wifi).wifi_start_sta);
    TEST_ASSERT_TRUE(state.pollDue(100, prepare_wifi).wifi_start_sta);
}
int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_grace_starts_at_completion_not_request);
    RUN_TEST(test_no_scan_still_gets_full_grace);
    RUN_TEST(test_later_scan_invalidates_elapsed_grace);
    RUN_TEST(test_missing_completion_hook_does_not_switch_radio);
    RUN_TEST(test_reschedule_requires_new_full_grace);
    RUN_TEST(test_reset_cancels_pending_work);
    RUN_TEST(test_grace_handles_clock_wrap);
    return UNITY_END();
}
