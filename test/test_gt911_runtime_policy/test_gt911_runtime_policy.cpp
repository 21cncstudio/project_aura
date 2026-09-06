#include <unity.h>

#include "core/Gt911RuntimePolicy.h"

using Gt911RuntimePolicy::FrameKind;
using Gt911RuntimePolicy::InterruptMode;
using Gt911RuntimePolicy::ReconciledFrameKind;
using Gt911RuntimePolicy::VendorReadAction;

void setUp() {}
void tearDown() {}

void test_gt911_runtime_interrupt_modes_decode_all_register_values() {
    const auto rising = Gt911RuntimePolicy::decodeInterruptConfig(0x00);
    TEST_ASSERT_EQUAL(InterruptMode::RisingEdge, rising.mode);
    TEST_ASSERT_TRUE(rising.active_high);
    TEST_ASSERT_TRUE(rising.positive_edge);
    TEST_ASSERT_TRUE(rising.direct_irq_supported);

    const auto falling = Gt911RuntimePolicy::decodeInterruptConfig(0x01);
    TEST_ASSERT_EQUAL(InterruptMode::FallingEdge, falling.mode);
    TEST_ASSERT_FALSE(falling.active_high);
    TEST_ASSERT_FALSE(falling.positive_edge);
    TEST_ASSERT_TRUE(falling.direct_irq_supported);

    const auto low = Gt911RuntimePolicy::decodeInterruptConfig(0x02);
    TEST_ASSERT_EQUAL(InterruptMode::LowLevel, low.mode);
    TEST_ASSERT_FALSE(low.active_high);
    TEST_ASSERT_FALSE(low.positive_edge);
    TEST_ASSERT_FALSE(low.direct_irq_supported);

    const auto high = Gt911RuntimePolicy::decodeInterruptConfig(0x03);
    TEST_ASSERT_EQUAL(InterruptMode::HighLevel, high.mode);
    TEST_ASSERT_TRUE(high.active_high);
    TEST_ASSERT_TRUE(high.positive_edge);
    TEST_ASSERT_FALSE(high.direct_irq_supported);

    const auto masked = Gt911RuntimePolicy::decodeInterruptConfig(0xFD);
    TEST_ASSERT_EQUAL(InterruptMode::FallingEdge, masked.mode);
    TEST_ASSERT_FALSE(masked.active_high);
}

void test_gt911_runtime_direct_irq_availability_requires_complete_edge_path() {
    TEST_ASSERT_TRUE(Gt911RuntimePolicy::directIrqAvailable(
        true, 0, false, true, true));
    TEST_ASSERT_TRUE(Gt911RuntimePolicy::directIrqAvailable(
        true, 1, false, true, true));

    for (int8_t mode = -1; mode <= 3; ++mode) {
        if (mode == 0 || mode == 1) {
            continue;
        }
        TEST_ASSERT_FALSE(Gt911RuntimePolicy::directIrqAvailable(
            true, mode, false, true, true));
    }
    TEST_ASSERT_FALSE(Gt911RuntimePolicy::directIrqAvailable(
        false, 1, false, true, true));
    TEST_ASSERT_FALSE(Gt911RuntimePolicy::directIrqAvailable(
        true, 1, true, true, true));
    TEST_ASSERT_FALSE(Gt911RuntimePolicy::directIrqAvailable(
        true, 1, false, false, true));
    TEST_ASSERT_FALSE(Gt911RuntimePolicy::directIrqAvailable(
        true, 1, false, true, false));
}

void test_gt911_runtime_frame_status_requires_ready_bit() {
    for (uint8_t count = 0; count <= Gt911RuntimePolicy::POINT_COUNT_MASK;
         ++count) {
        const auto no_frame = Gt911RuntimePolicy::decodeFrameStatus(count);
        TEST_ASSERT_EQUAL(FrameKind::NoFrame, no_frame.kind);
        TEST_ASSERT_EQUAL_UINT8(0, no_frame.point_count);
        TEST_ASSERT_EQUAL(
            VendorReadAction::Skip,
            Gt911RuntimePolicy::vendorReadAction(count));
    }
}

void test_gt911_runtime_frame_read_plan_cleans_every_ready_frame() {
    TEST_ASSERT_EQUAL(
        VendorReadAction::ReadFrame,
        Gt911RuntimePolicy::vendorReadAction(0x80));

    for (uint8_t count = 1; count <= Gt911RuntimePolicy::MAX_POINT_COUNT;
         ++count) {
        TEST_ASSERT_EQUAL(
            VendorReadAction::ReadFrame,
            Gt911RuntimePolicy::vendorReadAction(0x80U | count));
    }

    for (uint8_t count = Gt911RuntimePolicy::MAX_POINT_COUNT + 1;
         count <= Gt911RuntimePolicy::POINT_COUNT_MASK;
         ++count) {
        TEST_ASSERT_EQUAL(
            VendorReadAction::ReadForCleanup,
            Gt911RuntimePolicy::vendorReadAction(0x80U | count));
    }
}

void test_gt911_runtime_reconcile_no_frame_skips_vendor_read() {
    for (uint8_t count = 0; count <= Gt911RuntimePolicy::POINT_COUNT_MASK;
         ++count) {
        const auto sample = Gt911RuntimePolicy::reconcileFrame(count, -1);
        TEST_ASSERT_EQUAL(ReconciledFrameKind::NoData, sample.kind);
        TEST_ASSERT_EQUAL(VendorReadAction::Skip, sample.vendor_read_action);
        TEST_ASSERT_FALSE(sample.preserve_cache_on_error);
    }
}

void test_gt911_runtime_reconcile_coherent_release_and_press() {
    const auto release = Gt911RuntimePolicy::reconcileFrame(0x80, 0);
    TEST_ASSERT_EQUAL(ReconciledFrameKind::Released, release.kind);
    TEST_ASSERT_EQUAL(VendorReadAction::ReadFrame,
                      release.vendor_read_action);
    TEST_ASSERT_FALSE(release.preserve_cache_on_error);

    for (uint8_t count = 1; count <= Gt911RuntimePolicy::MAX_POINT_COUNT;
         ++count) {
        const auto press =
            Gt911RuntimePolicy::reconcileFrame(0x80U | count, 1);
        TEST_ASSERT_EQUAL(ReconciledFrameKind::Pressed, press.kind);
        TEST_ASSERT_EQUAL(VendorReadAction::ReadFrame,
                          press.vendor_read_action);
        TEST_ASSERT_FALSE(press.preserve_cache_on_error);
    }
}

void test_gt911_runtime_reconcile_frame_races_conservatively() {
    const auto press_disappeared =
        Gt911RuntimePolicy::reconcileFrame(0x81, 0);
    TEST_ASSERT_EQUAL(ReconciledFrameKind::Released, press_disappeared.kind);
    TEST_ASSERT_EQUAL(VendorReadAction::ReadFrame,
                      press_disappeared.vendor_read_action);
    TEST_ASSERT_FALSE(press_disappeared.preserve_cache_on_error);

    const auto new_press_after_release =
        Gt911RuntimePolicy::reconcileFrame(0x80, 1);
    TEST_ASSERT_EQUAL(ReconciledFrameKind::Pressed,
                      new_press_after_release.kind);
    TEST_ASSERT_EQUAL(VendorReadAction::ReadFrame,
                      new_press_after_release.vendor_read_action);
    TEST_ASSERT_FALSE(new_press_after_release.preserve_cache_on_error);
}

void test_gt911_runtime_reconcile_vendor_errors() {
    const uint8_t ready_statuses[] = {0x80, 0x81, 0x85};
    for (const uint8_t status : ready_statuses) {
        const auto sample = Gt911RuntimePolicy::reconcileFrame(status, -1);
        TEST_ASSERT_EQUAL(ReconciledFrameKind::Error, sample.kind);
        TEST_ASSERT_EQUAL(VendorReadAction::ReadFrame,
                          sample.vendor_read_action);
        TEST_ASSERT_FALSE(sample.preserve_cache_on_error);
    }
}

void test_gt911_runtime_reconcile_malformed_frame_is_cleanup_only() {
    const int vendor_results[] = {-1, 0, 1};
    for (uint8_t count = Gt911RuntimePolicy::MAX_POINT_COUNT + 1;
         count <= Gt911RuntimePolicy::POINT_COUNT_MASK;
         ++count) {
        const uint8_t status = 0x80U | count;
        for (const int vendor_result : vendor_results) {
            const auto sample =
                Gt911RuntimePolicy::reconcileFrame(status, vendor_result);
            TEST_ASSERT_EQUAL(ReconciledFrameKind::Error, sample.kind);
            TEST_ASSERT_EQUAL(VendorReadAction::ReadForCleanup,
                              sample.vendor_read_action);
            TEST_ASSERT_FALSE(sample.preserve_cache_on_error);
        }
    }
}

void test_gt911_runtime_frame_status_distinguishes_release_and_press() {
    const auto released = Gt911RuntimePolicy::decodeFrameStatus(0x80);
    TEST_ASSERT_EQUAL(FrameKind::Released, released.kind);
    TEST_ASSERT_EQUAL_UINT8(0, released.point_count);

    for (uint8_t count = 1; count <= Gt911RuntimePolicy::MAX_POINT_COUNT; ++count) {
        const auto pressed = Gt911RuntimePolicy::decodeFrameStatus(0x80U | count);
        TEST_ASSERT_EQUAL(FrameKind::Pressed, pressed.kind);
        TEST_ASSERT_EQUAL_UINT8(count, pressed.point_count);
    }
}

void test_gt911_runtime_frame_status_rejects_impossible_point_counts() {
    for (uint8_t count = Gt911RuntimePolicy::MAX_POINT_COUNT + 1;
         count <= Gt911RuntimePolicy::POINT_COUNT_MASK;
         ++count) {
        const auto malformed =
            Gt911RuntimePolicy::decodeFrameStatus(0x80U | count);
        TEST_ASSERT_EQUAL(FrameKind::Malformed, malformed.kind);
        TEST_ASSERT_EQUAL_UINT8(count, malformed.point_count);
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_gt911_runtime_interrupt_modes_decode_all_register_values);
    RUN_TEST(test_gt911_runtime_direct_irq_availability_requires_complete_edge_path);
    RUN_TEST(test_gt911_runtime_frame_status_requires_ready_bit);
    RUN_TEST(test_gt911_runtime_frame_status_distinguishes_release_and_press);
    RUN_TEST(test_gt911_runtime_frame_status_rejects_impossible_point_counts);
    RUN_TEST(test_gt911_runtime_frame_read_plan_cleans_every_ready_frame);
    RUN_TEST(test_gt911_runtime_reconcile_no_frame_skips_vendor_read);
    RUN_TEST(test_gt911_runtime_reconcile_coherent_release_and_press);
    RUN_TEST(test_gt911_runtime_reconcile_frame_races_conservatively);
    RUN_TEST(test_gt911_runtime_reconcile_vendor_errors);
    RUN_TEST(test_gt911_runtime_reconcile_malformed_frame_is_cleanup_only);
    return UNITY_END();
}
