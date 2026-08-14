#include <unity.h>

#include "core/BacklightWakeBreadcrumbs.h"

using namespace BacklightWakeBreadcrumbs;

void setUp() {
    test::resetRetained();
}

void tearDown() {}

void test_empty_warm_boot_has_no_trace() {
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
}

void test_cold_boot_rejects_retained_trace() {
    beginWake(Event::ScheduleWake, 100, 123456, true, false, {true, true, true});
    markDriverCallBegin();

    initializeAtBoot(true);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::PowerLost),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
}

void test_warm_boot_preserves_last_incomplete_stage_and_context() {
    beginWake(Event::ScheduleWake, 200, 1786617600, true, false, {true, true, false});
    markDriverCallBegin();

    initializeAtBoot(false);

    const BootSnapshot &snapshot = bootSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(snapshot.status));
    TEST_ASSERT_TRUE(snapshot.has_trace);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::ScheduleWake),
                          static_cast<int>(snapshot.trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::DriverCallBegin),
                          static_cast<int>(snapshot.trace.stage));
    TEST_ASSERT_EQUAL_UINT32(200, snapshot.trace.uptime_ms);
    TEST_ASSERT_EQUAL_UINT32(1786617600, snapshot.trace.epoch_s);
    TEST_ASSERT_TRUE(snapshot.trace.target_on);
    TEST_ASSERT_FALSE(snapshot.trace.previous_on);
    TEST_ASSERT_TRUE(snapshot.trace.before.valid);
    TEST_ASSERT_TRUE(snapshot.trace.before.sda_high);
    TEST_ASSERT_FALSE(snapshot.trace.before.scl_high);
}

void test_warm_boot_isolates_pre_driver_touch_irq_mask() {
    beginWake(Event::AlarmWake, 250, 1786617600, true, false,
              {true, true, true});
    markTouchIrqMaskBegin();
    markTouchIrqMaskReturned();

    initializeAtBoot(false);

    const BootSnapshot &snapshot = bootSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(snapshot.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::TouchIrqMaskReturned),
                          static_cast<int>(snapshot.trace.stage));
}

void test_completed_trace_retains_result_duration_and_lines() {
    beginWake(Event::ScheduleWake, 300, 1786617600, true, false, {true, true, true});
    markDriverCallBegin();
    markDriverCallReturned(true, false, 912, {true, false, true});
    markWakeProbeUpdateBegin();
    markWakeProbeUpdateReturned({true, true, true});
    markLvglActivityBegin();
    markLvglActivityReturned();
    markCommandReturned();
    markCompleted();

    initializeAtBoot(false);

    const BootSnapshot &snapshot = bootSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(snapshot.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(snapshot.trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DriverResult::Succeeded),
                          static_cast<int>(snapshot.trace.driver_result));
    TEST_ASSERT_EQUAL_UINT32(912, snapshot.trace.driver_duration_us);
    TEST_ASSERT_TRUE(snapshot.trace.after_driver.valid);
    TEST_ASSERT_FALSE(snapshot.trace.after_driver.sda_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_driver.scl_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_wake_probe.sda_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_wake_probe.scl_high);
}

void test_acknowledge_hides_retained_trace_but_keeps_boot_copy() {
    beginWake(Event::ScheduleWake, 400, 1786617600, true, false, {true, true, true});
    markDriverCallBegin();
    initializeAtBoot(false);
    const uint32_t captured_sequence = bootSnapshot().trace.sequence;

    acknowledgeBootSnapshot();

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_EQUAL_UINT32(captured_sequence, bootSnapshot().trace.sequence);
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
}

void test_corrupt_nonempty_storage_is_rejected() {
    test::corruptRetained();
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
}

void test_corrupt_latest_slot_falls_back_to_previous_valid_stage() {
    beginWake(Event::ScheduleWake, 500, 1786617600, true, false, {true, true, true});
    markDriverCallBegin();
    test::corruptLatestRecord();

    initializeAtBoot(false);

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Request),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_acknowledge_does_not_clear_a_newer_runtime_trace() {
    beginWake(Event::ScheduleWake, 600, 1786617600, true, false, {true, true, true});
    markDriverCallBegin();
    initializeAtBoot(false);
    const uint32_t captured_sequence = bootSnapshot().trace.sequence;

    beginWake(Event::ScheduleWake, 700, 1786617700, true, false, {true, true, true});
    acknowledgeBootSnapshot();
    initializeAtBoot(false);

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_EQUAL_UINT32(captured_sequence + 1u,
                             bootSnapshot().trace.sequence);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Request),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_new_operation_increments_sequence() {
    beginWake(Event::ScheduleWake, 1, 2, true, false, {true, true, true});
    initializeAtBoot(false);
    const uint32_t first_sequence = bootSnapshot().trace.sequence;
    acknowledgeBootSnapshot();
    beginWake(Event::ScheduleWake, 3, 4, true, false, {true, true, true});
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_UINT32(first_sequence + 1u, bootSnapshot().trace.sequence);
}

void test_enum_text_is_stable() {
    TEST_ASSERT_EQUAL_STRING("power_lost", statusText(CaptureStatus::PowerLost));
    TEST_ASSERT_EQUAL_STRING("active", statusText(CaptureStatus::Active));
    TEST_ASSERT_EQUAL_STRING("completed", statusText(CaptureStatus::Completed));
    TEST_ASSERT_EQUAL_STRING("schedule_wake", eventText(Event::ScheduleWake));
    TEST_ASSERT_EQUAL_STRING("touch_wake", eventText(Event::TouchWake));
    TEST_ASSERT_EQUAL_STRING("alarm_wake", eventText(Event::AlarmWake));
    TEST_ASSERT_EQUAL_STRING("driver_call_begin", stageText(Stage::DriverCallBegin));
    TEST_ASSERT_EQUAL_STRING("touch_irq_mask_begin",
                             stageText(Stage::TouchIrqMaskBegin));
    TEST_ASSERT_EQUAL_STRING("touch_irq_mask_returned",
                             stageText(Stage::TouchIrqMaskReturned));
    TEST_ASSERT_EQUAL_STRING("wake_probe_update_returned",
                             stageText(Stage::WakeProbeUpdateReturned));
    TEST_ASSERT_EQUAL_STRING("succeeded", driverResultText(DriverResult::Succeeded));
}

void assert_event_advances_through_all_stages(Event event) {
    beginWake(event, 10, 20, true, false, {true, true, true});
    markTouchIrqMaskBegin();
    markTouchIrqMaskReturned();
    markDriverCallBegin();
    markDriverCallReturned(true, false, 30, {true, true, true});
    markWakeProbeUpdateBegin();
    markWakeProbeUpdateReturned({true, true, true});
    markLvglActivityBegin();
    markLvglActivityReturned();
    markCommandReturned();
    markCompleted();
    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(event),
                          static_cast<int>(bootSnapshot().trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_touch_and_alarm_wakes_advance_through_all_stages() {
    assert_event_advances_through_all_stages(Event::TouchWake);
    test::resetRetained();
    assert_event_advances_through_all_stages(Event::AlarmWake);
}

void test_dark_wake_source_prefers_alarm_over_touch() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::None),
                          static_cast<int>(selectDarkWakeEvent(false, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::TouchWake),
                          static_cast<int>(selectDarkWakeEvent(true, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::AlarmWake),
                          static_cast<int>(selectDarkWakeEvent(false, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::AlarmWake),
                          static_cast<int>(selectDarkWakeEvent(true, true)));
}

void test_ui_context_checkpoints_survive_warm_boot() {
    beginWake(Event::TouchWake, 800, 1786617800, true, false, {true, true, true});
    markCompleted();
    markUiPostBacklightContext(0x3fca1000, 0x3fca1000, 0x3fcc2000);
    markUiPreRenderContext(0x3fca1000, 0x00000000, 0x3fcc2000);

    initializeAtBoot(false);

    const Trace &trace = bootSnapshot().trace;
    TEST_ASSERT_EQUAL_UINT32(0x3fca1000, trace.expected_network_manager_addr);
    TEST_ASSERT_EQUAL_UINT32(0x3fca1000, trace.post_backlight_network_manager_addr);
    TEST_ASSERT_EQUAL_UINT32(0x00000000, trace.pre_render_network_manager_addr);
    TEST_ASSERT_EQUAL_UINT32(0x3fcc2000, trace.post_backlight_task_handle);
    TEST_ASSERT_EQUAL_UINT32(0x3fcc2000, trace.pre_render_task_handle);
}

void test_retained_record_layout_remains_60_bytes() {
    TEST_ASSERT_EQUAL_UINT32(60, test::retainedRecordSize());
}

void test_schedule_wake_trace_policy_excludes_manual_and_sleep_transitions() {
    TEST_ASSERT_TRUE(shouldTraceScheduleWake(true, true, false));
    TEST_ASSERT_FALSE(shouldTraceScheduleWake(false, true, false));
    TEST_ASSERT_FALSE(shouldTraceScheduleWake(true, false, true));
    TEST_ASSERT_FALSE(shouldTraceScheduleWake(true, false, false));
    TEST_ASSERT_FALSE(shouldTraceScheduleWake(true, true, true));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_warm_boot_has_no_trace);
    RUN_TEST(test_cold_boot_rejects_retained_trace);
    RUN_TEST(test_warm_boot_preserves_last_incomplete_stage_and_context);
    RUN_TEST(test_warm_boot_isolates_pre_driver_touch_irq_mask);
    RUN_TEST(test_completed_trace_retains_result_duration_and_lines);
    RUN_TEST(test_acknowledge_hides_retained_trace_but_keeps_boot_copy);
    RUN_TEST(test_corrupt_nonempty_storage_is_rejected);
    RUN_TEST(test_corrupt_latest_slot_falls_back_to_previous_valid_stage);
    RUN_TEST(test_acknowledge_does_not_clear_a_newer_runtime_trace);
    RUN_TEST(test_new_operation_increments_sequence);
    RUN_TEST(test_enum_text_is_stable);
    RUN_TEST(test_touch_and_alarm_wakes_advance_through_all_stages);
    RUN_TEST(test_dark_wake_source_prefers_alarm_over_touch);
    RUN_TEST(test_ui_context_checkpoints_survive_warm_boot);
    RUN_TEST(test_retained_record_layout_remains_60_bytes);
    RUN_TEST(test_schedule_wake_trace_policy_excludes_manual_and_sleep_transitions);
    return UNITY_END();
}
