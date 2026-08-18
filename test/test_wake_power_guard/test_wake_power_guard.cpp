// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "core/WakePowerGuard.h"

using namespace WakePowerGuard;

void setUp() {
    resetForTest();
}

void tearDown() {}

void test_idle_guard_admits_and_releases_background_activity() {
    TEST_ASSERT_EQUAL_UINT32(0, activeOperations());
    {
        Activity activity = tryAcquireActivity(100U);
        TEST_ASSERT_TRUE(static_cast<bool>(activity));
        TEST_ASSERT_EQUAL_UINT32(1, activeOperations());
    }
    TEST_ASSERT_EQUAL_UINT32(0, activeOperations());
}

void test_request_closes_admission_and_waits_for_existing_activity() {
    Activity active = tryAcquireActivity(100U);
    TEST_ASSERT_TRUE(static_cast<bool>(active));
    TEST_ASSERT_TRUE(request(105U));
    TEST_ASSERT_EQUAL(Phase::PreQuiet, phase(105U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(105U)));
    TEST_ASSERT_FALSE(readyToSwitch(125U, 20U, 75U));

    active = Activity{};
    TEST_ASSERT_FALSE(readyToSwitch(126U, 20U, 75U));
    TEST_ASSERT_FALSE(readyToSwitch(145U, 20U, 75U));
    TEST_ASSERT_TRUE(readyToSwitch(146U, 20U, 75U));
}

void test_wait_warning_never_permits_switch_while_activity_remains_busy() {
    Activity active = tryAcquireActivity(1000U);
    TEST_ASSERT_TRUE(request(1001U));
    const SwitchDecision waiting = evaluateSwitch(1075U, 20U, 75U);
    TEST_ASSERT_FALSE(waiting.ready);
    TEST_ASSERT_EQUAL_UINT32(74U, waiting.elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(1U, waiting.active_operations);
    TEST_ASSERT_FALSE(waiting.wait_exceeded);

    const SwitchDecision exceeded = evaluateSwitch(1076U, 20U, 75U);
    TEST_ASSERT_FALSE(exceeded.ready);
    TEST_ASSERT_TRUE(exceeded.wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(75U, exceeded.elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(1U, exceeded.active_operations);
    TEST_ASSERT_EQUAL_UINT32(1, activeOperations());

    active = Activity{};
    const SwitchDecision drained = evaluateSwitch(1077U, 20U, 75U);
    TEST_ASSERT_FALSE(drained.ready);
    TEST_ASSERT_FALSE(drained.wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(0U, drained.active_operations);
    TEST_ASSERT_FALSE(evaluateSwitch(1096U, 20U, 75U).ready);
    TEST_ASSERT_TRUE(evaluateSwitch(1097U, 20U, 75U).ready);
}

void test_long_activity_gets_full_quiet_window_after_drain() {
    Activity active = tryAcquireActivity(100U);
    TEST_ASSERT_TRUE(request(101U));
    TEST_ASSERT_FALSE(evaluateSwitch(600U, 100U, 500U).ready);

    active = Activity{};
    TEST_ASSERT_FALSE(evaluateSwitch(601U, 100U, 500U).ready);
    TEST_ASSERT_FALSE(evaluateSwitch(700U, 100U, 500U).ready);
    TEST_ASSERT_TRUE(evaluateSwitch(701U, 100U, 500U).ready);
}

void test_first_zero_activity_evaluation_starts_full_quiet_window() {
    TEST_ASSERT_TRUE(request(3000U));
    const SwitchDecision first = evaluateSwitch(3100U, 100U, 500U);
    TEST_ASSERT_FALSE(first.ready);
    TEST_ASSERT_FALSE(first.wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(100U, first.elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(0U, first.active_operations);

    TEST_ASSERT_FALSE(evaluateSwitch(3199U, 100U, 500U).ready);
    const SwitchDecision ready = evaluateSwitch(3200U, 100U, 500U);
    TEST_ASSERT_TRUE(ready.ready);
    TEST_ASSERT_FALSE(ready.wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(200U, ready.elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(0U, ready.active_operations);
}

void test_first_zero_activity_evaluation_rebases_quiet_across_wraparound() {
    const uint32_t requested_at = UINT32_MAX - 10U;
    TEST_ASSERT_TRUE(request(requested_at));

    TEST_ASSERT_FALSE(evaluateSwitch(requested_at, 20U, 75U).ready);
    TEST_ASSERT_FALSE(evaluateSwitch(8U, 20U, 75U).ready);
    TEST_ASSERT_TRUE(evaluateSwitch(9U, 20U, 75U).ready);
}

void test_settle_then_render_wait_keeps_background_paused_until_completion() {
    TEST_ASSERT_TRUE(request(200U));
    TEST_ASSERT_FALSE(readyToSwitch(200U, 20U, 75U));
    TEST_ASSERT_TRUE(readyToSwitch(220U, 20U, 75U));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(220U, 200U);

    TEST_ASSERT_EQUAL(Phase::Settle, phase(220U));
    TEST_ASSERT_TRUE(backgroundPaused(419U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(419U)));
    TEST_ASSERT_FALSE(settleReady(419U));
    TEST_ASSERT_FALSE(beginRenderWait(419U));
    TEST_ASSERT_TRUE(backgroundPaused(420U));
    TEST_ASSERT_TRUE(uiPaused(420U));
    TEST_ASSERT_EQUAL(Phase::Settle, phase(420U));
    TEST_ASSERT_TRUE(settleReady(420U));
    TEST_ASSERT_TRUE(beginRenderWait(420U));
    TEST_ASSERT_FALSE(uiPaused(420U));
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(420U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(420U)));
    TEST_ASSERT_TRUE(completeRenderWait());
    TEST_ASSERT_FALSE(backgroundPaused(420U));
    TEST_ASSERT_EQUAL(Phase::Idle, phase(420U));
    TEST_ASSERT_TRUE(static_cast<bool>(tryAcquireActivity(420U)));
}

void test_settle_deadline_handles_millisecond_wraparound() {
    const uint32_t started = UINT32_MAX - 10U;
    TEST_ASSERT_TRUE(request(started));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(started, 20U);

    TEST_ASSERT_TRUE(backgroundPaused(8U));
    TEST_ASSERT_EQUAL(Phase::Settle, phase(9U));
    TEST_ASSERT_TRUE(beginRenderWait(9U));
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(9U));
    TEST_ASSERT_TRUE(backgroundPaused(9U));
    TEST_ASSERT_TRUE(completeRenderWait());
    TEST_ASSERT_FALSE(backgroundPaused(9U));
}

void test_zero_settle_still_requires_owner_to_publish_render_wait() {
    TEST_ASSERT_TRUE(request(10U));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(10U, 0U);

    TEST_ASSERT_EQUAL(Phase::Settle, phase(10U));
    TEST_ASSERT_TRUE(settleReady(10U));
    TEST_ASSERT_TRUE(uiPaused(10U));
    TEST_ASSERT_TRUE(beginRenderWait(10U));
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(10U));
    TEST_ASSERT_FALSE(uiPaused(10U));
}

void test_settle_deadline_exactly_wrapping_to_zero_is_not_ready_early() {
    const uint32_t started = UINT32_MAX - 199U;
    TEST_ASSERT_TRUE(request(started));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(started, 200U);

    TEST_ASSERT_FALSE(settleReady(started));
    TEST_ASSERT_FALSE(settleReady(UINT32_MAX));
    TEST_ASSERT_EQUAL(Phase::Settle, phase(UINT32_MAX));
    TEST_ASSERT_TRUE(settleReady(0U));
    TEST_ASSERT_TRUE(beginRenderWait(0U));
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(0U));
}

void test_switching_phase_never_reopens_background_admission() {
    TEST_ASSERT_TRUE(request(100U));
    TEST_ASSERT_FALSE(readyToSwitch(100U, 100U, 500U));
    TEST_ASSERT_TRUE(readyToSwitch(200U, 100U, 500U));
    TEST_ASSERT_TRUE(beginSwitch());
    TEST_ASSERT_EQUAL(Phase::Switching, phase(200U));

    TEST_ASSERT_TRUE(backgroundPaused(10000U));
    TEST_ASSERT_EQUAL(Phase::Switching, phase(10000U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(10000U)));
    TEST_ASSERT_FALSE(beginSwitch());

    beginSettle(10000U, 200U);
    TEST_ASSERT_TRUE(backgroundPaused(10199U));
    TEST_ASSERT_EQUAL(Phase::Settle, phase(10200U));
    TEST_ASSERT_TRUE(beginRenderWait(10200U));
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(10200U));
    TEST_ASSERT_TRUE(backgroundPaused(10200U));
    TEST_ASSERT_TRUE(completeRenderWait());
    TEST_ASSERT_FALSE(backgroundPaused(10200U));
}

void test_cancel_and_prequiet_failsafe_restore_admission() {
    TEST_ASSERT_TRUE(request(100U));
    cancel();
    TEST_ASSERT_FALSE(backgroundPaused(101U));

    TEST_ASSERT_TRUE(request(200U));
    TEST_ASSERT_TRUE(backgroundPaused(2199U));
    TEST_ASSERT_FALSE(backgroundPaused(2200U));
}

void test_fail_closed_latch_blocks_idle_and_prequiet_until_cancel() {
    TEST_ASSERT_EQUAL(Phase::Idle, phase(10U));
    latchFailClosed();
    TEST_ASSERT_EQUAL(Phase::FailClosed, phase(10U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(10U)));
    TEST_ASSERT_FALSE(request(10U));

    cancel();
    TEST_ASSERT_TRUE(request(20U));
    latchFailClosed();
    TEST_ASSERT_EQUAL(Phase::FailClosed, phase(20U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(20U)));

    cancel();
    TEST_ASSERT_EQUAL(Phase::Idle, phase(20U));
}

void test_fail_closed_latch_preserves_critical_wake_phases() {
    TEST_ASSERT_TRUE(request(100U));
    TEST_ASSERT_TRUE(beginSwitch());
    latchFailClosed();
    TEST_ASSERT_EQUAL(Phase::Switching, phase(10000U));
    TEST_ASSERT_TRUE(backgroundPaused(10000U));
    cancel();

    TEST_ASSERT_TRUE(request(200U));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(200U, 100U);
    latchFailClosed();
    TEST_ASSERT_EQUAL(Phase::Settle, phase(10000U));
    TEST_ASSERT_TRUE(backgroundPaused(10000U));
    cancel();

    TEST_ASSERT_TRUE(request(300U));
    TEST_ASSERT_TRUE(beginSwitch());
    beginSettle(300U, 0U);
    TEST_ASSERT_TRUE(beginRenderWait(300U));
    latchFailClosed();
    TEST_ASSERT_EQUAL(Phase::RenderWait, phase(10000U));
    TEST_ASSERT_TRUE(backgroundPaused(10000U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(10000U)));
    cancel();
    TEST_ASSERT_EQUAL(Phase::Idle, phase(10000U));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_guard_admits_and_releases_background_activity);
    RUN_TEST(test_request_closes_admission_and_waits_for_existing_activity);
    RUN_TEST(test_wait_warning_never_permits_switch_while_activity_remains_busy);
    RUN_TEST(test_long_activity_gets_full_quiet_window_after_drain);
    RUN_TEST(test_first_zero_activity_evaluation_starts_full_quiet_window);
    RUN_TEST(test_first_zero_activity_evaluation_rebases_quiet_across_wraparound);
    RUN_TEST(test_settle_then_render_wait_keeps_background_paused_until_completion);
    RUN_TEST(test_settle_deadline_handles_millisecond_wraparound);
    RUN_TEST(test_zero_settle_still_requires_owner_to_publish_render_wait);
    RUN_TEST(test_settle_deadline_exactly_wrapping_to_zero_is_not_ready_early);
    RUN_TEST(test_switching_phase_never_reopens_background_admission);
    RUN_TEST(test_cancel_and_prequiet_failsafe_restore_admission);
    RUN_TEST(test_fail_closed_latch_blocks_idle_and_prequiet_until_cancel);
    RUN_TEST(test_fail_closed_latch_preserves_critical_wake_phases);
    return UNITY_END();
}
