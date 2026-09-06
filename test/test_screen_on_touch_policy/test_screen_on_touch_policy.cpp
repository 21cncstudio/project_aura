#include <unity.h>

#include "core/ScreenOnTouchPolicy.h"

using ScreenOnTouchPolicy::Action;
using ScreenOnTouchPolicy::Mode;
using ScreenOnTouchPolicy::Sample;
using ScreenOnTouchPolicy::State;

void setUp() {}
void tearDown() {}

static void assert_mode(Mode expected, Mode actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

static void assert_action(Action expected, Action actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

static Action action_at(State &state, uint32_t now_ms, bool irq = false) {
    return state.decide(now_ms, irq).action;
}

static void record(State &state,
                   Action source,
                   Sample sample,
                   uint32_t now_ms) {
    const auto effect = state.recordRead(source, sample, now_ms);
    TEST_ASSERT_FALSE(effect.disarm_idle_irq);
    TEST_ASSERT_FALSE(effect.missed_idle_irq);
}

static void enter_idle(State &state, uint32_t release_ms) {
    record(state, Action::ReadFast, Sample::Released, release_ms);
    assert_action(Action::RequestIdleIrq,
                  action_at(state,
                            release_ms +
                                ScreenOnTouchPolicy::IDLE_AFTER_RELEASE_MS));
    state.recordIdleIrqArm(
        true,
        release_ms + ScreenOnTouchPolicy::IDLE_AFTER_RELEASE_MS);
    assert_mode(Mode::IdleIrq, state.mode());
}

void test_agreed_intervals_are_exposed_for_integration() {
    TEST_ASSERT_EQUAL_UINT32(40U, ScreenOnTouchPolicy::CALLBACK_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(40U, ScreenOnTouchPolicy::FAST_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(350U, ScreenOnTouchPolicy::FAST_AFTER_RELEASE_MS);
    TEST_ASSERT_EQUAL_UINT32(80U, ScreenOnTouchPolicy::CALM_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(1000U, ScreenOnTouchPolicy::IDLE_AFTER_RELEASE_MS);
    TEST_ASSERT_EQUAL_UINT32(200U,
                             ScreenOnTouchPolicy::IDLE_FALLBACK_INTERVAL_MS);
}

void test_irq_unavailable_uses_immediate_then_40_ms_polling_only() {
    State state;
    state.reset(100U, false);

    assert_mode(Mode::PollingOnly, state.mode());
    assert_action(Action::ReadPollingOnly, action_at(state, 100U));
    record(state, Action::ReadPollingOnly, Sample::NoData, 100U);
    assert_action(Action::None, action_at(state, 139U));
    assert_action(Action::ReadPollingOnly, action_at(state, 140U));
}

void test_fast_mode_reads_immediately_then_every_40_ms() {
    State state;
    state.reset(100U, true);

    assert_mode(Mode::FastInteraction, state.mode());
    assert_action(Action::ReadFast, action_at(state, 100U));
    record(state, Action::ReadFast, Sample::NoData, 100U);
    assert_action(Action::None, action_at(state, 139U));
    assert_action(Action::ReadFast, action_at(state, 140U));
}

void test_pressed_and_no_data_never_establish_idle_eligibility() {
    State state;
    state.reset(0U, true);

    record(state, Action::ReadFast, Sample::Pressed, 0U);
    record(state, Action::ReadFast, Sample::NoData, 40U);

    TEST_ASSERT_FALSE(state.releaseEligible());
    assert_mode(Mode::FastInteraction, state.mode());
    assert_action(Action::ReadFast, action_at(state, 1040U));
}

void test_explicit_release_keeps_fast_reads_for_350_ms_then_uses_80_ms() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Pressed, 0U);
    record(state, Action::ReadFast, Sample::Released, 40U);

    TEST_ASSERT_TRUE(state.releaseEligible());
    assert_action(Action::ReadFast, action_at(state, 360U));
    record(state, Action::ReadFast, Sample::NoData, 360U);

    assert_action(Action::None, action_at(state, 389U));
    assert_mode(Mode::FastInteraction, state.mode());
    assert_action(Action::None, action_at(state, 390U));
    assert_mode(Mode::ReleasedCalm, state.mode());
    assert_action(Action::None, action_at(state, 439U));
    assert_action(Action::ReadCalm, action_at(state, 440U));
}

void test_repeated_explicit_release_does_not_postpone_idle() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Released, 40U);
    record(state, Action::ReadFast, Sample::Released, 360U);

    TEST_ASSERT_EQUAL_UINT32(40U, state.explicitReleaseMs());
    assert_action(Action::RequestIdleIrq, action_at(state, 1040U));
}

void test_press_after_release_requires_a_new_release_in_fast_or_calm() {
    const uint32_t press_times[] = {200U, 440U};
    for (const uint32_t press_ms : press_times) {
        State state;
        state.reset(0U, true);
        record(state, Action::ReadFast, Sample::Released, 40U);
        if (press_ms >= 390U) {
            (void)state.decide(390U, false);
            assert_mode(Mode::ReleasedCalm, state.mode());
            record(state, Action::ReadCalm, Sample::Pressed, press_ms);
        } else {
            record(state, Action::ReadFast, Sample::Pressed, press_ms);
        }

        TEST_ASSERT_FALSE(state.releaseEligible());
        assert_mode(Mode::FastInteraction, state.mode());
        record(state, Action::ReadFast, Sample::Released, press_ms + 40U);
        TEST_ASSERT_EQUAL_UINT32(press_ms + 40U, state.explicitReleaseMs());
    }
}

void test_no_data_in_calm_does_not_move_release_deadline() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Released, 40U);
    (void)state.decide(390U, false);
    record(state, Action::ReadCalm, Sample::NoData, 440U);
    record(state, Action::ReadCalm, Sample::NoData, 520U);

    TEST_ASSERT_EQUAL_UINT32(40U, state.explicitReleaseMs());
    assert_action(Action::RequestIdleIrq, action_at(state, 1040U));
}

void test_idle_is_requested_exactly_1000_ms_after_explicit_release() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Released, 40U);

    assert_action(Action::ReadCalm, action_at(state, 1039U));
    assert_action(Action::RequestIdleIrq, action_at(state, 1040U));
    state.recordIdleIrqArm(true, 1040U);
    assert_mode(Mode::IdleIrq, state.mode());
}

void test_idle_boundary_sample_blocks_arm_for_press_or_error() {
    const Sample blocked_samples[] = {Sample::Pressed, Sample::Error};
    for (const Sample sample : blocked_samples) {
        State state;
        state.reset(0U, true);
        record(state, Action::ReadFast, Sample::Released, 40U);
        assert_action(Action::RequestIdleIrq, action_at(state, 1040U));

        TEST_ASSERT_FALSE(state.recordIdleBoundarySample(sample, 1040U));
        assert_mode(Mode::FastInteraction, state.mode());
        TEST_ASSERT_FALSE(state.releaseEligible());
    }

    const Sample allowed_samples[] = {Sample::NoData, Sample::Released};
    for (const Sample sample : allowed_samples) {
        State state;
        state.reset(0U, true);
        record(state, Action::ReadFast, Sample::Released, 40U);
        assert_action(Action::RequestIdleIrq, action_at(state, 1040U));

        TEST_ASSERT_TRUE(state.recordIdleBoundarySample(sample, 1040U));
        state.recordIdleIrqArm(true, 1040U);
        assert_mode(Mode::IdleIrq, state.mode());
    }
}

void test_idle_irq_arm_failure_latches_polling_only() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Released, 40U);
    assert_action(Action::RequestIdleIrq, action_at(state, 1040U));

    state.recordIdleIrqArm(false, 1040U);

    assert_mode(Mode::PollingOnly, state.mode());
    TEST_ASSERT_FALSE(state.releaseEligible());
    assert_action(Action::ReadPollingOnly, action_at(state, 1040U));
}

void test_idle_fallback_read_is_due_every_200_ms() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    assert_action(Action::None, action_at(state, 1239U));
    assert_action(Action::ReadIdleFallback, action_at(state, 1240U));
    record(state, Action::ReadIdleFallback, Sample::NoData, 1240U);
    assert_action(Action::None, action_at(state, 1439U));
    assert_action(Action::ReadIdleFallback, action_at(state, 1440U));
}

void test_idle_irq_requests_immediate_read_before_fallback_deadline() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    assert_action(Action::ReadIdleIrq, action_at(state, 1080U, true));
    const auto effect =
        state.recordRead(Action::ReadIdleIrq, Sample::Pressed, 1080U);

    TEST_ASSERT_TRUE(effect.disarm_idle_irq);
    TEST_ASSERT_FALSE(effect.missed_idle_irq);
    assert_mode(Mode::FastInteraction, state.mode());
    assert_action(Action::None, action_at(state, 1119U));
    assert_action(Action::ReadFast, action_at(state, 1120U));
}

void test_any_irq_read_disarms_and_requalifies_from_scratch() {
    const Sample samples[] = {
        Sample::Pressed,
        Sample::NoData,
        Sample::Released,
        Sample::Error,
    };

    for (Sample sample : samples) {
        State state;
        state.reset(0U, true);
        enter_idle(state, 40U);

        const auto effect =
            state.recordRead(Action::ReadIdleIrq, sample, 1080U);

        TEST_ASSERT_TRUE(effect.disarm_idle_irq);
        TEST_ASSERT_FALSE(effect.missed_idle_irq);
        TEST_ASSERT_FALSE(state.releaseEligible());
        assert_mode(Mode::FastInteraction, state.mode());
        assert_action(Action::None, action_at(state, 1119U));
        assert_action(Action::ReadFast, action_at(state, 1120U));
    }
}

void test_idle_fallback_no_data_or_release_keeps_irq_mode() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);
    const uint32_t release_ms = state.explicitReleaseMs();

    record(state, Action::ReadIdleFallback, Sample::NoData, 1240U);

    assert_mode(Mode::IdleIrq, state.mode());
    TEST_ASSERT_EQUAL_UINT32(release_ms, state.explicitReleaseMs());
    record(state, Action::ReadIdleFallback, Sample::Released, 1440U);

    assert_mode(Mode::IdleIrq, state.mode());
    TEST_ASSERT_EQUAL_UINT32(release_ms, state.explicitReleaseMs());
    assert_action(Action::None, action_at(state, 1639U));
    assert_action(Action::ReadIdleFallback, action_at(state, 1640U));
}

void test_no_data_after_irq_does_not_count_as_release() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    const auto effect =
        state.recordRead(Action::ReadIdleIrq, Sample::NoData, 1080U);

    TEST_ASSERT_TRUE(effect.disarm_idle_irq);
    TEST_ASSERT_FALSE(state.releaseEligible());
    assert_action(Action::ReadFast, action_at(state, 1120U));
    record(state, Action::ReadFast, Sample::Pressed, 1120U);
    TEST_ASSERT_FALSE(state.releaseEligible());

    record(state, Action::ReadFast, Sample::Released, 1160U);
    TEST_ASSERT_TRUE(state.releaseEligible());
    assert_action(Action::RequestIdleIrq, action_at(state, 2160U));
}

void test_fallback_press_reports_missed_irq_and_latches_polling_only() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    const auto effect =
        state.recordRead(Action::ReadIdleFallback, Sample::Pressed, 1240U);

    TEST_ASSERT_TRUE(effect.disarm_idle_irq);
    TEST_ASSERT_TRUE(effect.missed_idle_irq);
    TEST_ASSERT_EQUAL_UINT32(1U, state.missedIdleIrqCount());
    assert_mode(Mode::PollingOnly, state.mode());

    record(state, Action::ReadPollingOnly, Sample::Released, 1280U);
    assert_mode(Mode::PollingOnly, state.mode());
}

void test_fallback_press_with_post_read_irq_is_reconciled_as_irq_read() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    const Action source = ScreenOnTouchPolicy::reconcileIdleReadSource(
        Action::ReadIdleFallback, Sample::Pressed, true);
    assert_action(Action::ReadIdleIrq, source);

    const auto effect = state.recordRead(source, Sample::Pressed, 1240U);
    TEST_ASSERT_TRUE(effect.disarm_idle_irq);
    TEST_ASSERT_FALSE(effect.missed_idle_irq);
    TEST_ASSERT_EQUAL_UINT32(0U, state.missedIdleIrqCount());
    assert_mode(Mode::FastInteraction, state.mode());
}

void test_fallback_reconciliation_changes_only_pressed_with_late_irq() {
    assert_action(
        Action::ReadIdleFallback,
        ScreenOnTouchPolicy::reconcileIdleReadSource(
            Action::ReadIdleFallback, Sample::Pressed, false));
    assert_action(
        Action::ReadIdleFallback,
        ScreenOnTouchPolicy::reconcileIdleReadSource(
            Action::ReadIdleFallback, Sample::NoData, true));
    assert_action(
        Action::ReadFast,
        ScreenOnTouchPolicy::reconcileIdleReadSource(
            Action::ReadFast, Sample::Pressed, true));
}

void test_error_resets_release_eligibility_and_returns_to_fast() {
    State state;
    state.reset(0U, true);
    record(state, Action::ReadFast, Sample::Released, 40U);
    assert_mode(Mode::ReleasedCalm, state.decide(390U, false).mode);

    record(state, Action::ReadCalm, Sample::Error, 440U);

    TEST_ASSERT_FALSE(state.releaseEligible());
    assert_mode(Mode::FastInteraction, state.mode());
    assert_action(Action::ReadFast, action_at(state, 1480U));
}

void test_idle_error_requests_irq_disarm_and_requalifies_from_scratch() {
    State state;
    state.reset(0U, true);
    enter_idle(state, 40U);

    const auto effect =
        state.recordRead(Action::ReadIdleFallback, Sample::Error, 1240U);

    TEST_ASSERT_TRUE(effect.disarm_idle_irq);
    TEST_ASSERT_FALSE(effect.missed_idle_irq);
    TEST_ASSERT_FALSE(state.releaseEligible());
    assert_mode(Mode::FastInteraction, state.mode());
}

void test_force_polling_only_is_sticky_until_reset() {
    State state;
    state.reset(0U, true);
    state.usePollingOnly();

    record(state, Action::ReadPollingOnly, Sample::Released, 40U);
    assert_mode(Mode::PollingOnly, state.mode());

    state.reset(100U, true);
    assert_mode(Mode::FastInteraction, state.mode());
    TEST_ASSERT_EQUAL_UINT32(0U, state.missedIdleIrqCount());
}

void test_release_and_fast_deadlines_are_wrap_safe() {
    State state;
    const uint32_t release_ms = UINT32_MAX - 200U;
    state.reset(release_ms, true);
    record(state, Action::ReadFast, Sample::Released, release_ms);

    assert_mode(Mode::FastInteraction,
                state.decide(release_ms + 349U, false).mode);
    assert_mode(Mode::ReleasedCalm,
                state.decide(release_ms + 350U, false).mode);
    assert_action(Action::RequestIdleIrq,
                  action_at(state, release_ms + 1000U));
}

void test_idle_fallback_deadline_is_wrap_safe() {
    State state;
    const uint32_t arm_ms = UINT32_MAX - 100U;
    state.reset(arm_ms - 1000U, true);
    record(state,
           Action::ReadFast,
           Sample::Released,
           arm_ms - ScreenOnTouchPolicy::IDLE_AFTER_RELEASE_MS);
    assert_action(Action::RequestIdleIrq, action_at(state, arm_ms));
    state.recordIdleIrqArm(true, arm_ms);

    assert_action(Action::None,
                  action_at(state,
                            arm_ms +
                                ScreenOnTouchPolicy::IDLE_FALLBACK_INTERVAL_MS -
                                1U));
    assert_action(Action::ReadIdleFallback,
                  action_at(state,
                            arm_ms +
                                ScreenOnTouchPolicy::IDLE_FALLBACK_INTERVAL_MS));
}

void test_fast_and_polling_read_deadlines_are_wrap_safe() {
    const uint32_t start_ms = UINT32_MAX - 20U;

    State fast;
    fast.reset(start_ms, true);
    record(fast, Action::ReadFast, Sample::NoData, start_ms);
    assert_action(Action::None, action_at(fast, start_ms + 39U));
    assert_action(Action::ReadFast, action_at(fast, start_ms + 40U));

    State polling;
    polling.reset(start_ms, false);
    record(polling, Action::ReadPollingOnly, Sample::NoData, start_ms);
    assert_action(Action::None, action_at(polling, start_ms + 39U));
    assert_action(Action::ReadPollingOnly,
                  action_at(polling, start_ms + 40U));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_agreed_intervals_are_exposed_for_integration);
    RUN_TEST(test_irq_unavailable_uses_immediate_then_40_ms_polling_only);
    RUN_TEST(test_fast_mode_reads_immediately_then_every_40_ms);
    RUN_TEST(test_pressed_and_no_data_never_establish_idle_eligibility);
    RUN_TEST(test_explicit_release_keeps_fast_reads_for_350_ms_then_uses_80_ms);
    RUN_TEST(test_repeated_explicit_release_does_not_postpone_idle);
    RUN_TEST(test_press_after_release_requires_a_new_release_in_fast_or_calm);
    RUN_TEST(test_no_data_in_calm_does_not_move_release_deadline);
    RUN_TEST(test_idle_is_requested_exactly_1000_ms_after_explicit_release);
    RUN_TEST(test_idle_boundary_sample_blocks_arm_for_press_or_error);
    RUN_TEST(test_idle_irq_arm_failure_latches_polling_only);
    RUN_TEST(test_idle_fallback_read_is_due_every_200_ms);
    RUN_TEST(test_idle_irq_requests_immediate_read_before_fallback_deadline);
    RUN_TEST(test_any_irq_read_disarms_and_requalifies_from_scratch);
    RUN_TEST(test_idle_fallback_no_data_or_release_keeps_irq_mode);
    RUN_TEST(test_no_data_after_irq_does_not_count_as_release);
    RUN_TEST(test_fallback_press_reports_missed_irq_and_latches_polling_only);
    RUN_TEST(test_fallback_press_with_post_read_irq_is_reconciled_as_irq_read);
    RUN_TEST(test_fallback_reconciliation_changes_only_pressed_with_late_irq);
    RUN_TEST(test_error_resets_release_eligibility_and_returns_to_fast);
    RUN_TEST(test_idle_error_requests_irq_disarm_and_requalifies_from_scratch);
    RUN_TEST(test_force_polling_only_is_sticky_until_reset);
    RUN_TEST(test_release_and_fast_deadlines_are_wrap_safe);
    RUN_TEST(test_idle_fallback_deadline_is_wrap_safe);
    RUN_TEST(test_fast_and_polling_read_deadlines_are_wrap_safe);
    return UNITY_END();
}
