#include <unity.h>

#include "web/OtaUiLineagePolicy.h"

void setUp() {}
void tearDown() {}

void test_new_pending_supersedes_old_terminal_and_hide_commands() {
    OtaUiLineagePolicy::ScreenLineage lineage;
    constexpr uint32_t confirm_a = 41;
    constexpr uint32_t confirm_b = 42;

    TEST_ASSERT_TRUE(lineage.accept(confirm_a));  // A Pending
    TEST_ASSERT_TRUE(lineage.accept(confirm_a));  // A Allowed
    TEST_ASSERT_TRUE(lineage.accept(confirm_b));  // B Pending
    TEST_ASSERT_FALSE(lineage.accept(confirm_a)); // stale A terminal
    TEST_ASSERT_FALSE(lineage.accept(confirm_a)); // stale A Hidden
    TEST_ASSERT_EQUAL_UINT32(confirm_b, lineage.latestConfirmId());
    TEST_ASSERT_TRUE(lineage.accept(confirm_b));  // B Installing
}

void test_zero_id_is_rejected_and_wraparound_sequence_is_ordered() {
    OtaUiLineagePolicy::ScreenLineage lineage;

    TEST_ASSERT_FALSE(lineage.accept(0));
    TEST_ASSERT_TRUE(lineage.accept(UINT32_MAX));
    TEST_ASSERT_TRUE(lineage.accept(1));
    TEST_ASSERT_FALSE(lineage.accept(UINT32_MAX));
    TEST_ASSERT_EQUAL_UINT32(1, lineage.latestConfirmId());
}

void test_terminal_command_can_establish_or_advance_lineage_without_pending_delivery() {
    OtaUiLineagePolicy::ScreenLineage lineage;

    TEST_ASSERT_TRUE(lineage.accept(501));  // A terminal overwrote A Pending
    TEST_ASSERT_TRUE(lineage.accept(502));  // B terminal overwrote B Pending
    TEST_ASSERT_FALSE(lineage.accept(501));
    TEST_ASSERT_EQUAL_UINT32(502, lineage.latestConfirmId());
}

void test_upload_confirm_id_is_exposed_only_after_successful_consumption() {
    constexpr uint32_t attacker_supplied_id = 0x40000000U;
    constexpr uint32_t validated_id = 19U;

    TEST_ASSERT_EQUAL_UINT32(
        0,
        OtaUiLineagePolicy::validatedUploadConfirmId(false, attacker_supplied_id));
    TEST_ASSERT_EQUAL_UINT32(
        0,
        OtaUiLineagePolicy::validatedUploadConfirmId(true, 0));
    TEST_ASSERT_EQUAL_UINT32(
        validated_id,
        OtaUiLineagePolicy::validatedUploadConfirmId(true, validated_id));
}

void test_preflight_expiry_returns_its_confirm_id_once() {
    OtaUiLineagePolicy::PreflightLease lease;
    constexpr uint32_t confirm_id = 77;

    TEST_ASSERT_TRUE(lease.arm(confirm_id, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, lease.takeIfDue(999));
    TEST_ASSERT_EQUAL_UINT32(confirm_id, lease.takeIfDue(1000));
    TEST_ASSERT_EQUAL_UINT32(0, lease.takeIfDue(1001));
}

void test_preflight_rearm_and_cancel_are_lineage_specific() {
    OtaUiLineagePolicy::PreflightLease lease;
    constexpr uint32_t confirm_a = 90;
    constexpr uint32_t confirm_b = 91;

    TEST_ASSERT_TRUE(lease.arm(confirm_a, 100));
    TEST_ASSERT_TRUE(lease.arm(confirm_b, 200));
    TEST_ASSERT_FALSE(lease.cancel(confirm_a));
    TEST_ASSERT_EQUAL_UINT32(0, lease.takeIfDue(100));
    TEST_ASSERT_EQUAL_UINT32(confirm_b, lease.takeIfDue(200));

    TEST_ASSERT_FALSE(lease.arm(confirm_a, 250));
    TEST_ASSERT_TRUE(lease.arm(confirm_b, 300));
    TEST_ASSERT_TRUE(lease.cancel(confirm_b));
    TEST_ASSERT_EQUAL_UINT32(0, lease.takeIfDue(300));
}

void test_preflight_deadline_handles_millis_wraparound() {
    OtaUiLineagePolicy::PreflightLease lease;
    constexpr uint32_t confirm_id = 123;

    TEST_ASSERT_TRUE(lease.arm(confirm_id, 5));
    TEST_ASSERT_EQUAL_UINT32(0, lease.takeIfDue(UINT32_MAX - 1));
    TEST_ASSERT_EQUAL_UINT32(confirm_id, lease.takeIfDue(5));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_new_pending_supersedes_old_terminal_and_hide_commands);
    RUN_TEST(test_zero_id_is_rejected_and_wraparound_sequence_is_ordered);
    RUN_TEST(test_terminal_command_can_establish_or_advance_lineage_without_pending_delivery);
    RUN_TEST(test_upload_confirm_id_is_exposed_only_after_successful_consumption);
    RUN_TEST(test_preflight_expiry_returns_its_confirm_id_once);
    RUN_TEST(test_preflight_rearm_and_cancel_are_lineage_specific);
    RUN_TEST(test_preflight_deadline_handles_millis_wraparound);
    return UNITY_END();
}
