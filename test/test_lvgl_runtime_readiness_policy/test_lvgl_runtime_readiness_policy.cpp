#include <unity.h>

#include "ui/LvglRuntimeReadinessPolicy.h"

using LvglRuntimeReadinessPolicy::Diagnostics;

namespace {

Diagnostics healthyDiagnostics() {
    Diagnostics diagnostics;
    diagnostics.timer_handler_count = 12;
    diagnostics.timer_handler_age_ms = 25;
    diagnostics.flush_count = 3;
    diagnostics.flush_age_ms = 250;
    diagnostics.vsync_count = 24;
    diagnostics.vsync_age_ms = 16;
    return diagnostics;
}

bool isReady(const Diagnostics &diagnostics,
             bool lvgl_ready = true,
             bool stall_active = false,
             bool recovery_restart_requested = false,
             bool recovery_suppressed = false) {
    return LvglRuntimeReadinessPolicy::isReady(
        lvgl_ready,
        stall_active,
        recovery_restart_requested,
        recovery_suppressed,
        diagnostics);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_fresh_handler_flush_and_vsync_are_ready() {
    TEST_ASSERT_TRUE(isReady(healthyDiagnostics()));
}

void test_each_activity_must_have_occurred() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.timer_handler_count = 0;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.flush_count = 0;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.vsync_count = 0;
    TEST_ASSERT_FALSE(isReady(diagnostics));
}

void test_handler_and_vsync_age_must_be_known() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.timer_handler_age_ms = LvglRuntimeReadinessPolicy::AGE_UNKNOWN_MS;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.vsync_age_ms = LvglRuntimeReadinessPolicy::AGE_UNKNOWN_MS;
    TEST_ASSERT_FALSE(isReady(diagnostics));
}

void test_activity_at_or_beyond_stall_threshold_is_not_ready() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.timer_handler_age_ms =
        LvglRuntimeReadinessPolicy::HANDLER_MAX_AGE_MS;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.vsync_age_ms =
        LvglRuntimeReadinessPolicy::VSYNC_MAX_AGE_MS;
    TEST_ASSERT_FALSE(isReady(diagnostics));
}

void test_activity_just_inside_threshold_is_ready() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.timer_handler_age_ms =
        LvglRuntimeReadinessPolicy::HANDLER_MAX_AGE_MS - 1;
    diagnostics.flush_age_ms =
        LvglRuntimeReadinessPolicy::FLUSH_MAX_AGE_MS - 1;
    diagnostics.vsync_age_ms =
        LvglRuntimeReadinessPolicy::VSYNC_MAX_AGE_MS - 1;

    TEST_ASSERT_TRUE(isReady(diagnostics));
}

void test_old_flush_is_allowed_after_first_render() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.flush_age_ms = LvglRuntimeReadinessPolicy::AGE_UNKNOWN_MS;
    TEST_ASSERT_TRUE(isReady(diagnostics));
}

void test_runtime_fault_pause_and_recovery_states_are_not_ready() {
    Diagnostics diagnostics = healthyDiagnostics();
    diagnostics.display_sync_fault = true;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.touch_offline = true;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    diagnostics.paused = true;
    TEST_ASSERT_FALSE(isReady(diagnostics));

    diagnostics = healthyDiagnostics();
    TEST_ASSERT_FALSE(isReady(diagnostics, false));
    TEST_ASSERT_FALSE(isReady(diagnostics, true, true));
    TEST_ASSERT_FALSE(isReady(diagnostics, true, false, true));
    TEST_ASSERT_FALSE(isReady(diagnostics, true, false, false, true));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_handler_flush_and_vsync_are_ready);
    RUN_TEST(test_each_activity_must_have_occurred);
    RUN_TEST(test_handler_and_vsync_age_must_be_known);
    RUN_TEST(test_activity_at_or_beyond_stall_threshold_is_not_ready);
    RUN_TEST(test_activity_just_inside_threshold_is_ready);
    RUN_TEST(test_old_flush_is_allowed_after_first_render);
    RUN_TEST(test_runtime_fault_pause_and_recovery_states_are_not_ready);
    return UNITY_END();
}
