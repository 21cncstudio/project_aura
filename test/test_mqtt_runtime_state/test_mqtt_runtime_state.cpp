#include <unity.h>

#include "core/MqttRuntimeState.h"

#include "../../src/core/MqttRuntimeState.cpp"

void setUp() {}
void tearDown() {}

void test_request_publish_is_released_only_after_update() {
    MqttRuntimeState state;

    state.requestPublish();
    TEST_ASSERT_FALSE(state.consumePublishRequest());

    SensorData data{};
    FanStateSnapshot fan{};
    state.update(data, fan, false, true, false, true, true, true, false);

    TEST_ASSERT_TRUE(state.consumePublishRequest());
    TEST_ASSERT_FALSE(state.consumePublishRequest());
    const MqttRuntimeSnapshot snapshot = state.snapshot();
    TEST_ASSERT_TRUE(snapshot.backlight_on);
    TEST_ASSERT_TRUE(snapshot.backlight_transition_pending);
    TEST_ASSERT_TRUE(snapshot.backlight_target_on);
}

void test_multiple_publish_requests_collapse_until_next_update() {
    MqttRuntimeState state;

    state.requestPublish();
    state.requestPublish();
    state.requestPublish();

    SensorData data{};
    FanStateSnapshot fan{};
    state.update(data, fan, false, false, false, false, false, false, false);

    TEST_ASSERT_TRUE(state.consumePublishRequest());
    TEST_ASSERT_FALSE(state.consumePublishRequest());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_request_publish_is_released_only_after_update);
    RUN_TEST(test_multiple_publish_requests_collapse_until_next_update);
    return UNITY_END();
}
