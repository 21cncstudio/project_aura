#include <unity.h>

#include "core/LastGoodHealthPolicy.h"

namespace {

LastGoodHealthPolicy::Inputs healthyInputs() {
    LastGoodHealthPolicy::Inputs inputs = {};
    inputs.board_ready = true;
    inputs.lvgl_ready = true;
    inputs.display_bus_ready = true;
    inputs.ui_runtime_healthy = true;
    return inputs;
}

void assertUnhealthy(const LastGoodHealthPolicy::Inputs &inputs) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(OperationalHealth::Unhealthy),
        static_cast<int>(LastGoodHealthPolicy::classify(inputs)));
}

} // namespace

void setUp() {}
void tearDown() {}

void test_all_required_runtime_signals_produce_healthy() {
    const LastGoodHealthPolicy::Inputs inputs = healthyInputs();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(OperationalHealth::Healthy),
        static_cast<int>(LastGoodHealthPolicy::classify(inputs)));

    LastGoodHealthPolicy::Inputs unhealthy_ui = inputs;
    unhealthy_ui.ui_runtime_healthy = false;
    assertUnhealthy(unhealthy_ui);
}

void test_each_structural_gate_is_fail_closed() {
    LastGoodHealthPolicy::Inputs inputs = healthyInputs();
    inputs.board_ready = false;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.lvgl_ready = false;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.display_bus_ready = false;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.critical_runtime_fault = true;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.recovery_or_restart_pending = true;
    assertUnhealthy(inputs);
}

void test_transient_pause_is_unavailable_only_for_an_otherwise_healthy_runtime() {
    LastGoodHealthPolicy::Inputs inputs = healthyInputs();
    inputs.transient_pause = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(OperationalHealth::Unavailable),
        static_cast<int>(LastGoodHealthPolicy::classify(inputs)));

    inputs.board_ready = false;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.transient_pause = true;
    inputs.critical_runtime_fault = true;
    assertUnhealthy(inputs);

    inputs = healthyInputs();
    inputs.transient_pause = true;
    inputs.ui_runtime_healthy = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(OperationalHealth::Unavailable),
        static_cast<int>(LastGoodHealthPolicy::classify(inputs)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_all_required_runtime_signals_produce_healthy);
    RUN_TEST(test_each_structural_gate_is_fail_closed);
    RUN_TEST(test_transient_pause_is_unavailable_only_for_an_otherwise_healthy_runtime);
    return UNITY_END();
}
