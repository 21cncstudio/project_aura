#include <unity.h>

#include "web/OtaPhysicalConfirm.h"

using OtaPhysicalConfirm::ConsumeStatus;
using OtaPhysicalConfirm::PrepareStatus;
using OtaPhysicalConfirm::State;

void setUp() {}
void tearDown() {}

void test_prepare_requires_confirmation_then_allows_single_upload() {
    OtaPhysicalConfirm::StateMachine state;
    const auto required = state.prepare(4096, false, 0, 1000);

    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Required), static_cast<int>(required.status));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, required.confirm_id);
    TEST_ASSERT_EQUAL_UINT32(OtaPhysicalConfirm::kRetryAfterMs, required.retry_after_ms);
    TEST_ASSERT_EQUAL_UINT32(OtaPhysicalConfirm::kPendingTimeoutMs, required.confirm_timeout_ms);

    TEST_ASSERT_TRUE(state.allowCurrent(required.confirm_id, 2000));

    const auto ready = state.prepare(4096, true, required.confirm_id, 2100);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Ready), static_cast<int>(ready.status));
    TEST_ASSERT_EQUAL_UINT32(required.confirm_id, ready.confirm_id);

    const auto consumed = state.consumeForUpload(4096, true, required.confirm_id, 2200);
    TEST_ASSERT_EQUAL(static_cast<int>(ConsumeStatus::Consumed), static_cast<int>(consumed.status));
    TEST_ASSERT_EQUAL(static_cast<int>(State::Consumed), static_cast<int>(state.snapshot().state));

    const auto second_upload = state.consumeForUpload(4096, true, required.confirm_id, 2300);
    TEST_ASSERT_EQUAL(static_cast<int>(ConsumeStatus::Expired), static_cast<int>(second_upload.status));
}

void test_second_prepare_without_id_is_busy_while_pending() {
    OtaPhysicalConfirm::StateMachine state;
    const auto first = state.prepare(1000, false, 0, 10);
    const auto second = state.prepare(2000, false, 0, 20);

    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Required), static_cast<int>(first.status));
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Busy), static_cast<int>(second.status));
    TEST_ASSERT_EQUAL_UINT32(first.confirm_id, second.confirm_id);
}

void test_deny_is_reported_to_same_confirm_id_and_new_request_can_start() {
    OtaPhysicalConfirm::StateMachine state;
    const auto pending = state.prepare(3000, false, 0, 100);

    TEST_ASSERT_TRUE(state.denyCurrent(pending.confirm_id, 200));

    const auto blocked_before_observed = state.prepare(3000, false, 0, 250);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Busy),
                      static_cast<int>(blocked_before_observed.status));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id, blocked_before_observed.confirm_id);

    const auto denied = state.prepare(3000, true, pending.confirm_id, 300);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Denied), static_cast<int>(denied.status));

    const auto next = state.prepare(3000, false, 0, 400);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Required), static_cast<int>(next.status));
    TEST_ASSERT_NOT_EQUAL_UINT32(pending.confirm_id, next.confirm_id);
}

void test_terminal_retention_releases_if_browser_disappears() {
    OtaPhysicalConfirm::StateMachine state;
    const auto pending = state.prepare(3000, false, 0, 100);

    TEST_ASSERT_TRUE(state.denyCurrent(pending.confirm_id, 200));

    const auto blocked = state.prepare(
        3000,
        false,
        0,
        200 + OtaPhysicalConfirm::kTerminalRetentionMs - 1);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Busy), static_cast<int>(blocked.status));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id, blocked.confirm_id);

    const auto next = state.prepare(
        3000,
        false,
        0,
        200 + OtaPhysicalConfirm::kTerminalRetentionMs);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Required), static_cast<int>(next.status));
    TEST_ASSERT_NOT_EQUAL_UINT32(pending.confirm_id, next.confirm_id);

    const auto late_denied = state.prepare(
        3000,
        true,
        pending.confirm_id,
        200 + OtaPhysicalConfirm::kTerminalRetentionMs + 1);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Denied), static_cast<int>(late_denied.status));
}

void test_pending_and_allowed_windows_expire() {
    OtaPhysicalConfirm::StateMachine state;
    constexpr uint32_t start_ms = 100;
    const auto pending = state.prepare(3000, false, 0, start_ms);
    const uint32_t pending_expired_at = start_ms + OtaPhysicalConfirm::kPendingTimeoutMs;

    TEST_ASSERT_EQUAL_UINT32(0, state.pollExpired(pending_expired_at - 1));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id, state.pollExpired(pending_expired_at));
    TEST_ASSERT_EQUAL_UINT32(0, state.pollExpired(pending_expired_at + 1));

    const auto blocked_before_observed = state.prepare(3000, false, 0, pending_expired_at + 1);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Busy),
                      static_cast<int>(blocked_before_observed.status));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id, blocked_before_observed.confirm_id);

    const auto expired_pending =
        state.prepare(3000, true, pending.confirm_id, pending_expired_at + 2);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Expired), static_cast<int>(expired_pending.status));

    const uint32_t next_start_ms = pending_expired_at + 3;
    const auto next = state.prepare(3000, false, 0, next_start_ms);
    TEST_ASSERT_TRUE(state.allowCurrent(next.confirm_id, next_start_ms + 100));
    const auto expired_upload = state.consumeForUpload(
        3000,
        true,
        next.confirm_id,
        next_start_ms + 100 + OtaPhysicalConfirm::kAllowedTimeoutMs);
    TEST_ASSERT_EQUAL(static_cast<int>(ConsumeStatus::Expired), static_cast<int>(expired_upload.status));
}

void test_confirm_id_and_size_must_match() {
    OtaPhysicalConfirm::StateMachine state;
    const auto pending = state.prepare(4096, false, 0, 100);

    TEST_ASSERT_TRUE(state.allowCurrent(pending.confirm_id, 200));

    const auto wrong_id = state.prepare(4096, true, pending.confirm_id + 1, 300);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Mismatch), static_cast<int>(wrong_id.status));

    const auto wrong_size = state.consumeForUpload(8192, true, pending.confirm_id, 400);
    TEST_ASSERT_EQUAL(static_cast<int>(ConsumeStatus::Mismatch), static_cast<int>(wrong_size.status));

    const auto missing = state.consumeForUpload(4096, false, 0, 500);
    TEST_ASSERT_EQUAL(static_cast<int>(ConsumeStatus::Missing), static_cast<int>(missing.status));
}

void test_allowed_expiry_reports_exact_confirm_id_once() {
    OtaPhysicalConfirm::StateMachine state;
    constexpr uint32_t start_ms = 500;
    const auto pending = state.prepare(4096, false, 0, start_ms);
    const uint32_t allowed_at = start_ms + 100;
    TEST_ASSERT_TRUE(state.allowCurrent(pending.confirm_id, allowed_at));

    const uint32_t expired_at = allowed_at + OtaPhysicalConfirm::kAllowedTimeoutMs;
    TEST_ASSERT_EQUAL_UINT32(0, state.pollExpired(expired_at - 1));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id, state.pollExpired(expired_at));
    TEST_ASSERT_EQUAL_UINT32(0, state.pollExpired(expired_at + 1));
}

void test_expiry_event_survives_detection_by_another_state_machine_call() {
    OtaPhysicalConfirm::StateMachine state;
    constexpr uint32_t start_ms = 700;
    const auto pending = state.prepare(4096, false, 0, start_ms);
    const uint32_t expired_at = start_ms + OtaPhysicalConfirm::kPendingTimeoutMs;

    const auto expired =
        state.prepare(4096, true, pending.confirm_id, expired_at);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Expired),
                      static_cast<int>(expired.status));
    TEST_ASSERT_EQUAL_UINT32(pending.confirm_id,
                             state.pollExpired(expired_at + 1));
    TEST_ASSERT_EQUAL_UINT32(0, state.pollExpired(expired_at + 2));
}

void test_stale_allow_cannot_authorize_new_request() {
    OtaPhysicalConfirm::StateMachine state;
    const auto first = state.prepare(4096, false, 0, 100);
    TEST_ASSERT_TRUE(state.denyCurrent(first.confirm_id, 200));

    const auto observed = state.prepare(4096, true, first.confirm_id, 300);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Denied),
                      static_cast<int>(observed.status));

    const auto second = state.prepare(8192, false, 0, 400);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Required),
                      static_cast<int>(second.status));
    TEST_ASSERT_NOT_EQUAL_UINT32(first.confirm_id, second.confirm_id);

    TEST_ASSERT_FALSE(state.allowCurrent(first.confirm_id, 500));
    TEST_ASSERT_EQUAL(static_cast<int>(State::Pending),
                      static_cast<int>(state.snapshot().state));
    TEST_ASSERT_EQUAL_UINT32(second.confirm_id, state.snapshot().confirm_id);
    TEST_ASSERT_TRUE(state.allowCurrent(second.confirm_id, 600));
}

void test_stale_or_repeated_deny_cannot_reject_new_request() {
    OtaPhysicalConfirm::StateMachine state;
    const auto first = state.prepare(4096, false, 0, 100);
    TEST_ASSERT_TRUE(state.denyCurrent(first.confirm_id, 200));
    TEST_ASSERT_FALSE(state.denyCurrent(first.confirm_id, 201));

    const auto observed = state.prepare(4096, true, first.confirm_id, 300);
    TEST_ASSERT_EQUAL(static_cast<int>(PrepareStatus::Denied),
                      static_cast<int>(observed.status));
    const auto second = state.prepare(8192, false, 0, 400);

    TEST_ASSERT_FALSE(state.denyCurrent(0, 450));
    TEST_ASSERT_FALSE(state.denyCurrent(first.confirm_id, 500));
    TEST_ASSERT_EQUAL(static_cast<int>(State::Pending),
                      static_cast<int>(state.snapshot().state));
    TEST_ASSERT_EQUAL_UINT32(second.confirm_id, state.snapshot().confirm_id);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_prepare_requires_confirmation_then_allows_single_upload);
    RUN_TEST(test_second_prepare_without_id_is_busy_while_pending);
    RUN_TEST(test_deny_is_reported_to_same_confirm_id_and_new_request_can_start);
    RUN_TEST(test_terminal_retention_releases_if_browser_disappears);
    RUN_TEST(test_pending_and_allowed_windows_expire);
    RUN_TEST(test_allowed_expiry_reports_exact_confirm_id_once);
    RUN_TEST(test_expiry_event_survives_detection_by_another_state_machine_call);
    RUN_TEST(test_confirm_id_and_size_must_match);
    RUN_TEST(test_stale_allow_cannot_authorize_new_request);
    RUN_TEST(test_stale_or_repeated_deny_cannot_reject_new_request);
    return UNITY_END();
}
