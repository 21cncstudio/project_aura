#include <unity.h>

#include "web/WebDeferredActionsState.h"

void setUp() {}
void tearDown() {}

void test_deferred_actions_fire_only_after_deadline() {
    WebDeferredActionsState state;

    state.scheduleWifiStartSta(100, 200);
    state.scheduleMqttSync(100, 300);

    WebDeferredActionsDue due = state.pollDue(299);
    TEST_ASSERT_FALSE(due.wifi_start_sta);
    TEST_ASSERT_FALSE(due.mqtt_sync);

    due = state.pollDue(300);
    TEST_ASSERT_TRUE(due.wifi_start_sta);
    TEST_ASSERT_FALSE(due.mqtt_sync);

    due = state.pollDue(400);
    TEST_ASSERT_FALSE(due.wifi_start_sta);
    TEST_ASSERT_TRUE(due.mqtt_sync);
}

void test_deferred_actions_reset_clears_pending_work() {
    WebDeferredActionsState state;

    state.scheduleWifiStartSta(10, 20);
    state.scheduleMqttSync(10, 20);
    state.reset();

    const WebDeferredActionsDue due = state.pollDue(1000);
    TEST_ASSERT_FALSE(due.wifi_start_sta);
    TEST_ASSERT_FALSE(due.mqtt_sync);
}

void test_deferred_actions_reschedule_replaces_deadline() {
    WebDeferredActionsState state;

    state.scheduleWifiStartSta(10, 20);
    state.scheduleWifiStartSta(15, 50);

    WebDeferredActionsDue due = state.pollDue(35);
    TEST_ASSERT_FALSE(due.wifi_start_sta);

    due = state.pollDue(65);
    TEST_ASSERT_TRUE(due.wifi_start_sta);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_deferred_actions_fire_only_after_deadline);
    RUN_TEST(test_deferred_actions_reset_clears_pending_work);
    RUN_TEST(test_deferred_actions_reschedule_replaces_deadline);
    return UNITY_END();
}
