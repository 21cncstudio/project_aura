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

void test_brownout_marker_survives_unacknowledged_warm_restart() {
    beginPreQuietWake(
        Event::AlarmWake, 150, 1787000000, true, false, {true, true, true});
    markPreQuietWaitExceeded(500, 2);

    initializeAtBoot(true, true);

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::PreQuietWaitExceeded),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_TRUE(bootSnapshot().trace.pre_quiet_wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(
        2, bootSnapshot().trace.pre_quiet_wait_exceeded_active_operations);

    // This is an ordinary warm restart, with no external recovery flag.
    initializeAtBoot(false);
    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
}

void test_acknowledge_clears_brownout_marker() {
    beginWake(Event::AlarmWake, 150, 1787000000, true, false,
              {true, true, true});
    markDriverCallBegin();

    initializeAtBoot(true, true);
    initializeAtBoot(false);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);

    acknowledgeBootSnapshot();
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().retention_uncertain);
}

void test_true_cold_reset_clears_brownout_marker() {
    beginWake(Event::AlarmWake, 150, 1787000000, true, false,
              {true, true, true});
    markDriverCallBegin();

    initializeAtBoot(true, true);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);

    initializeAtBoot(true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::PowerLost),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
    TEST_ASSERT_FALSE(bootSnapshot().retention_uncertain);

    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().retention_uncertain);
}

void test_brownout_style_boot_reports_corrupt_retained_storage() {
    test::corruptRetained();

    initializeAtBoot(true, true);

    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    initializeAtBoot(false);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    acknowledgeBootSnapshot();
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
}

void test_brownout_torn_first_write_is_reported_as_corrupt() {
    test::seedValidEmptyWithCorruptSibling();

    initializeAtBoot(true, true);

    TEST_ASSERT_FALSE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));

    initializeAtBoot(false);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
}

void test_brownout_terminal_fallback_with_corrupt_sibling_keeps_details() {
    test::seedTerminalWithCorruptSibling();

    initializeAtBoot(true, true);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::TouchWake),
                          static_cast<int>(bootSnapshot().trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Succeeded),
                          static_cast<int>(bootSnapshot().trace.command_result));

    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_torn_latest_marker_preserves_uncertainty_conservatively() {
    beginWake(Event::TouchWake, 180, 1787000200, true, false,
              {true, true, true});
    markDriverCallBegin();

    initializeAtBoot(true, true);
    initializeAtBoot(true, true);
    test::corruptLatestEvidenceMarker();
    initializeAtBoot(false);

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::DriverCallBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
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
    beginPreQuietWake(Event::ScheduleWake,
                      300,
                      1786617600,
                      true,
                      false,
                      {true, true, true});
    markPreQuietReady(500, {true, true, true});
    markDriverCallBegin();
    markDriverCallReturned(true, false, 912, {true, false, true});
    markWakeProbeUpdateBegin();
    markWakeProbeUpdateReturned({true, true, true});
    markLvglActivityBegin();
    markLvglActivityReturned();
    markCommandReturned(CommandResult::Succeeded);
    markGuardSettleBegin();
    markGuardSettleReturned();
    markCompleted();

    initializeAtBoot(false);

    const BootSnapshot &snapshot = bootSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(snapshot.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(snapshot.trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DriverResult::Succeeded),
                          static_cast<int>(snapshot.trace.driver_result));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Succeeded),
                          static_cast<int>(snapshot.trace.command_result));
    TEST_ASSERT_EQUAL_UINT32(912, snapshot.trace.driver_duration_us);
    TEST_ASSERT_EQUAL_UINT32(500, snapshot.trace.pre_quiet_elapsed_ms);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.trace.pre_quiet_active_operations);
    TEST_ASSERT_FALSE(snapshot.trace.pre_quiet_wait_exceeded);
    TEST_ASSERT_FALSE(snapshot.trace.pre_quiet_forced_by_timeout);
    TEST_ASSERT_TRUE(snapshot.trace.after_driver.valid);
    TEST_ASSERT_FALSE(snapshot.trace.after_driver.sda_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_driver.scl_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_wake_probe.sda_high);
    TEST_ASSERT_TRUE(snapshot.trace.after_wake_probe.scl_high);
}

void test_prequiet_trace_covers_wait_threshold_and_event_promotion() {
    beginPreQuietWake(
        Event::ScheduleWake, 310, 1787000100, true, false, {true, false, true});

    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::PreQuietBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_UINT32(310, bootSnapshot().trace.uptime_ms);

    markPreQuietWaitExceeded(500, 3);
    markPreQuietWaitExceeded(700, 1);
    updateWakeEvent(Event::AlarmWake);
    markPreQuietReady(825, {true, true, true});
    initializeAtBoot(false);

    const Trace &trace = bootSnapshot().trace;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::PreQuietReady),
                          static_cast<int>(trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::AlarmWake),
                          static_cast<int>(trace.event));
    TEST_ASSERT_EQUAL_UINT32(825, trace.pre_quiet_elapsed_ms);
    TEST_ASSERT_TRUE(trace.pre_quiet_wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(3, trace.pre_quiet_active_operations);
    TEST_ASSERT_EQUAL_UINT32(
        3, trace.pre_quiet_wait_exceeded_active_operations);
    TEST_ASSERT_FALSE(trace.pre_quiet_forced_by_timeout);
    TEST_ASSERT_TRUE(trace.before.valid);
    TEST_ASSERT_TRUE(trace.before.sda_high);
    TEST_ASSERT_TRUE(trace.before.scl_high);
}

void test_failed_command_is_terminal_and_cannot_be_marked_completed() {
    beginWake(Event::TouchWake, 320, 1787000200, true, false, {true, true, true});
    markDriverCallBegin();
    markDriverCallReturned(true, false, 100, {true, true, true});
    markCommandReturned(CommandResult::Failed);
    markGuardSettleBegin();
    markCompleted();
    markDriverCallBegin();
    markDriverCallReturned(false, false, 999, {true, false, false});
    markWakeProbeUpdateReturned({true, false, false});

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Failed),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Failed),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Failed),
                          static_cast<int>(bootSnapshot().trace.command_result));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DriverResult::Succeeded),
                          static_cast<int>(bootSnapshot().trace.driver_result));
    TEST_ASSERT_EQUAL_UINT32(100, bootSnapshot().trace.driver_duration_us);
}

void test_aborted_command_is_a_distinct_terminal_result() {
    beginPreQuietWake(
        Event::AlarmWake, 330, 1787000300, true, false, {true, true, true});
    markAborted();
    markCompleted();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Aborted),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Aborted),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Aborted),
                          static_cast<int>(bootSnapshot().trace.command_result));
}

void test_completed_requires_explicit_command_success() {
    beginWake(Event::ScheduleWake, 340, 1787000400, true, false, {true, true, true});
    markCompleted();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Request),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Unknown),
                          static_cast<int>(bootSnapshot().trace.command_result));
}

void test_completed_requires_returned_guard_settle_or_legacy_command_stage() {
    beginWake(Event::TouchWake, 345, 1787000450, true, false,
              {true, true, true});
    markCommandReturnedPendingSettle(CommandResult::Succeeded);
    markGuardSettleBegin();
    markCompleted();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::GuardSettleBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_guard_settle_stage_survives_reset_before_completion() {
    beginWake(Event::TouchWake, 350, 1787000500, true, false, {true, true, true});
    markCommandReturned(CommandResult::Succeeded);
    markGuardSettleBegin();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::GuardSettleBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Succeeded),
                          static_cast<int>(bootSnapshot().trace.command_result));
}

void test_failed_driver_attempt_stays_incomplete_until_guard_settle_returns() {
    beginWake(Event::MqttWake, 360, 1787000600, true, false, {true, true, true});
    markDriverCallBegin();
    markDriverCallReturned(false, false, 140, {true, true, true});
    markCommandReturnedPendingSettle(CommandResult::Failed);
    markGuardSettleBegin();
    markFailed();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::GuardSettleBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Failed),
                          static_cast<int>(bootSnapshot().trace.command_result));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DriverResult::Failed),
                          static_cast<int>(bootSnapshot().trace.driver_result));

    markGuardSettleReturned();
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::GuardSettleReturned),
                          static_cast<int>(bootSnapshot().trace.stage));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Failed),
                          static_cast<int>(bootSnapshot().trace.command_result));

    markCompleted();
    markFailed();
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Failed),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Failed),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_legacy_v2_completed_record_remains_decodable() {
    test::seedLegacyV2CompletedTrace();

    initializeAtBoot(false);

    const BootSnapshot &snapshot = bootSnapshot();
    TEST_ASSERT_TRUE(snapshot.has_trace);
    TEST_ASSERT_FALSE(snapshot.retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(snapshot.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandResult::Unknown),
                          static_cast<int>(snapshot.trace.command_result));
    TEST_ASSERT_TRUE(snapshot.trace.pre_quiet_forced_by_timeout);
    TEST_ASSERT_FALSE(snapshot.trace.pre_quiet_wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(3, snapshot.trace.pre_quiet_active_operations);
    TEST_ASSERT_EQUAL_UINT32(60, test::retainedRecordSize());
}

void test_legacy_v2_ignores_uninitialized_marker_storage() {
    test::seedLegacyV2CompletedTrace();
    test::corruptEvidenceStorage();

    initializeAtBoot(false);

    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_FALSE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(bootSnapshot().trace.stage));
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
    TEST_ASSERT_TRUE(bootSnapshot().retention_uncertain);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                          static_cast<int>(bootSnapshot().status));
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
    TEST_ASSERT_EQUAL_STRING("failed", statusText(CaptureStatus::Failed));
    TEST_ASSERT_EQUAL_STRING("aborted", statusText(CaptureStatus::Aborted));
    TEST_ASSERT_EQUAL_STRING("schedule_wake", eventText(Event::ScheduleWake));
    TEST_ASSERT_EQUAL_STRING("touch_wake", eventText(Event::TouchWake));
    TEST_ASSERT_EQUAL_STRING("alarm_wake", eventText(Event::AlarmWake));
    TEST_ASSERT_EQUAL_STRING("web_wake", eventText(Event::WebWake));
    TEST_ASSERT_EQUAL_STRING("mqtt_wake", eventText(Event::MqttWake));
    TEST_ASSERT_EQUAL_STRING("startup_wake", eventText(Event::StartupWake));
    TEST_ASSERT_EQUAL_STRING("driver_call_begin", stageText(Stage::DriverCallBegin));
    TEST_ASSERT_EQUAL_STRING("touch_irq_mask_begin",
                             stageText(Stage::TouchIrqMaskBegin));
    TEST_ASSERT_EQUAL_STRING("touch_irq_mask_returned",
                             stageText(Stage::TouchIrqMaskReturned));
    TEST_ASSERT_EQUAL_STRING("power_settle_begin",
                             stageText(Stage::PowerSettleBegin));
    TEST_ASSERT_EQUAL_STRING("power_settle_returned",
                             stageText(Stage::PowerSettleReturned));
    TEST_ASSERT_EQUAL_STRING("pre_quiet_begin",
                             stageText(Stage::PreQuietBegin));
    TEST_ASSERT_EQUAL_STRING("pre_quiet_wait_exceeded",
                             stageText(Stage::PreQuietWaitExceeded));
    TEST_ASSERT_EQUAL_STRING("pre_quiet_ready",
                             stageText(Stage::PreQuietReady));
    TEST_ASSERT_EQUAL_STRING("guard_settle_begin",
                             stageText(Stage::GuardSettleBegin));
    TEST_ASSERT_EQUAL_STRING("guard_settle_returned",
                             stageText(Stage::GuardSettleReturned));
    TEST_ASSERT_EQUAL_STRING("wake_probe_update_returned",
                             stageText(Stage::WakeProbeUpdateReturned));
    TEST_ASSERT_EQUAL_STRING("succeeded", driverResultText(DriverResult::Succeeded));
    TEST_ASSERT_EQUAL_STRING("failed", commandResultText(CommandResult::Failed));
    TEST_ASSERT_EQUAL_STRING("aborted", commandResultText(CommandResult::Aborted));
}

void assert_event_advances_through_all_stages(Event event) {
    beginWake(event, 10, 20, true, false, {true, true, true});
    markTouchIrqMaskBegin();
    markTouchIrqMaskReturned();
    markDriverCallBegin();
    markDriverCallReturned(true, false, 30, {true, true, true});
    markPowerSettleBegin();
    markPowerSettleReturned();
    markWakeProbeUpdateBegin();
    markWakeProbeUpdateReturned({true, true, true});
    markLvglActivityBegin();
    markLvglActivityReturned();
    markCommandReturned(CommandResult::Succeeded);
    markGuardSettleBegin();
    markGuardSettleReturned();
    markCompleted();
    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Completed),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(event),
                          static_cast<int>(bootSnapshot().trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::Completed),
                          static_cast<int>(bootSnapshot().trace.stage));
}

void test_all_wake_sources_advance_through_all_stages() {
    assert_event_advances_through_all_stages(Event::TouchWake);
    test::resetRetained();
    assert_event_advances_through_all_stages(Event::AlarmWake);
    test::resetRetained();
    assert_event_advances_through_all_stages(Event::WebWake);
    test::resetRetained();
    assert_event_advances_through_all_stages(Event::MqttWake);
    test::resetRetained();
    assert_event_advances_through_all_stages(Event::StartupWake);
}

void test_startup_event_appends_without_renumbering_retained_events() {
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(Event::None));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(Event::ScheduleWake));
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(Event::TouchWake));
    TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(Event::AlarmWake));
    TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(Event::WebWake));
    TEST_ASSERT_EQUAL_UINT8(5, static_cast<uint8_t>(Event::MqttWake));
    TEST_ASSERT_EQUAL_UINT8(6, static_cast<uint8_t>(Event::StartupWake));
}

void test_startup_prequiet_trace_survives_warm_restart() {
    beginPreQuietWake(
        Event::StartupWake, 1200, 1787000000, true, false, {true, false, true});
    markPreQuietWaitExceeded(500, 2);
    markPreQuietReady(750, {true, true, true});
    markTouchIrqMaskBegin();

    initializeAtBoot(false);

    const Trace &trace = bootSnapshot().trace;
    TEST_ASSERT_TRUE(bootSnapshot().has_trace);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::StartupWake),
                          static_cast<int>(trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::TouchIrqMaskBegin),
                          static_cast<int>(trace.stage));
    TEST_ASSERT_EQUAL_UINT32(1200, trace.uptime_ms);
    TEST_ASSERT_EQUAL_UINT32(750, trace.pre_quiet_elapsed_ms);
    TEST_ASSERT_TRUE(trace.pre_quiet_wait_exceeded);
    TEST_ASSERT_EQUAL_UINT32(2, trace.pre_quiet_wait_exceeded_active_operations);
    TEST_ASSERT_TRUE(trace.target_on);
    TEST_ASSERT_FALSE(trace.previous_on);
}

void test_startup_event_update_preserves_active_trace_context() {
    // Source-priority policy belongs to the caller; this checks record updates.
    beginPreQuietWake(
        Event::ScheduleWake, 1300, 1787000100, true, false, {true, true, true});
    markPreQuietWaitExceeded(500, 1);
    updateWakeEvent(Event::StartupWake);

    initializeAtBoot(false);

    const Trace &trace = bootSnapshot().trace;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::StartupWake),
                          static_cast<int>(trace.event));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::PreQuietWaitExceeded),
                          static_cast<int>(trace.stage));
    TEST_ASSERT_EQUAL_UINT32(1, trace.sequence);
    TEST_ASSERT_EQUAL_UINT32(1300, trace.uptime_ms);
    TEST_ASSERT_EQUAL_UINT32(1787000100, trace.epoch_s);
    TEST_ASSERT_EQUAL_UINT32(1, trace.pre_quiet_wait_exceeded_active_operations);
}

void test_legacy_v2_rejects_events_beyond_alarm_even_with_valid_crc() {
    const Event unsupported[] = {Event::WebWake, Event::MqttWake, Event::StartupWake};
    for (Event event : unsupported) {
        test::resetRetained();
        test::seedLegacyV2CompletedTrace(event);

        initializeAtBoot(false);

        TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Corrupt),
                              static_cast<int>(bootSnapshot().status));
        TEST_ASSERT_FALSE(bootSnapshot().has_trace);
    }
}

void test_unknown_event_after_startup_is_not_started_or_applied() {
    const Event unsupported = static_cast<Event>(7);
    beginWake(unsupported, 1, 2, true, false, {true, true, true});
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Empty),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_FALSE(bootSnapshot().has_trace);

    beginWake(Event::StartupWake, 3, 4, true, false, {true, true, true});
    updateWakeEvent(unsupported);
    initializeAtBoot(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Event::StartupWake),
                          static_cast<int>(bootSnapshot().trace.event));
    TEST_ASSERT_EQUAL_UINT32(3, bootSnapshot().trace.uptime_ms);
}

void test_warm_boot_preserves_incomplete_power_settle() {
    beginWake(Event::AlarmWake, 900, 1786746847, true, false, {true, true, true});
    markDriverCallBegin();
    markDriverCallReturned(true, false, 40, {true, true, true});
    markPowerSettleBegin();

    initializeAtBoot(false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CaptureStatus::Active),
                          static_cast<int>(bootSnapshot().status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Stage::PowerSettleBegin),
                          static_cast<int>(bootSnapshot().trace.stage));
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
    markCommandReturned(CommandResult::Succeeded);
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
    RUN_TEST(test_brownout_marker_survives_unacknowledged_warm_restart);
    RUN_TEST(test_acknowledge_clears_brownout_marker);
    RUN_TEST(test_true_cold_reset_clears_brownout_marker);
    RUN_TEST(test_brownout_style_boot_reports_corrupt_retained_storage);
    RUN_TEST(test_brownout_torn_first_write_is_reported_as_corrupt);
    RUN_TEST(test_brownout_terminal_fallback_with_corrupt_sibling_keeps_details);
    RUN_TEST(test_torn_latest_marker_preserves_uncertainty_conservatively);
    RUN_TEST(test_warm_boot_preserves_last_incomplete_stage_and_context);
    RUN_TEST(test_warm_boot_isolates_pre_driver_touch_irq_mask);
    RUN_TEST(test_completed_trace_retains_result_duration_and_lines);
    RUN_TEST(test_prequiet_trace_covers_wait_threshold_and_event_promotion);
    RUN_TEST(test_failed_command_is_terminal_and_cannot_be_marked_completed);
    RUN_TEST(test_aborted_command_is_a_distinct_terminal_result);
    RUN_TEST(test_completed_requires_explicit_command_success);
    RUN_TEST(test_completed_requires_returned_guard_settle_or_legacy_command_stage);
    RUN_TEST(test_guard_settle_stage_survives_reset_before_completion);
    RUN_TEST(test_failed_driver_attempt_stays_incomplete_until_guard_settle_returns);
    RUN_TEST(test_legacy_v2_completed_record_remains_decodable);
    RUN_TEST(test_legacy_v2_ignores_uninitialized_marker_storage);
    RUN_TEST(test_acknowledge_hides_retained_trace_but_keeps_boot_copy);
    RUN_TEST(test_corrupt_nonempty_storage_is_rejected);
    RUN_TEST(test_corrupt_latest_slot_falls_back_to_previous_valid_stage);
    RUN_TEST(test_acknowledge_does_not_clear_a_newer_runtime_trace);
    RUN_TEST(test_new_operation_increments_sequence);
    RUN_TEST(test_enum_text_is_stable);
    RUN_TEST(test_all_wake_sources_advance_through_all_stages);
    RUN_TEST(test_startup_event_appends_without_renumbering_retained_events);
    RUN_TEST(test_startup_prequiet_trace_survives_warm_restart);
    RUN_TEST(test_startup_event_update_preserves_active_trace_context);
    RUN_TEST(test_legacy_v2_rejects_events_beyond_alarm_even_with_valid_crc);
    RUN_TEST(test_unknown_event_after_startup_is_not_started_or_applied);
    RUN_TEST(test_warm_boot_preserves_incomplete_power_settle);
    RUN_TEST(test_dark_wake_source_prefers_alarm_over_touch);
    RUN_TEST(test_ui_context_checkpoints_survive_warm_boot);
    RUN_TEST(test_retained_record_layout_remains_60_bytes);
    RUN_TEST(test_schedule_wake_trace_policy_excludes_manual_and_sleep_transitions);
    return UNITY_END();
}
