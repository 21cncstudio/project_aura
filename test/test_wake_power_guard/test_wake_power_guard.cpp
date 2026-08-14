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
    TEST_ASSERT_TRUE(readyToSwitch(126U, 20U, 75U));
}

void test_max_wait_bounds_wake_latency_when_activity_remains_busy() {
    Activity active = tryAcquireActivity(1000U);
    TEST_ASSERT_TRUE(request(1001U));
    TEST_ASSERT_FALSE(readyToSwitch(1075U, 20U, 75U));
    TEST_ASSERT_TRUE(readyToSwitch(1076U, 20U, 75U));
    TEST_ASSERT_EQUAL_UINT32(1, activeOperations());
}

void test_settle_window_pauses_then_resumes_background_work() {
    TEST_ASSERT_TRUE(request(200U));
    TEST_ASSERT_TRUE(readyToSwitch(220U, 20U, 75U));
    beginSettle(220U, 200U);

    TEST_ASSERT_EQUAL(Phase::Settle, phase(220U));
    TEST_ASSERT_TRUE(backgroundPaused(419U));
    TEST_ASSERT_FALSE(static_cast<bool>(tryAcquireActivity(419U)));
    TEST_ASSERT_FALSE(backgroundPaused(420U));
    TEST_ASSERT_EQUAL(Phase::Idle, phase(420U));
    TEST_ASSERT_TRUE(static_cast<bool>(tryAcquireActivity(420U)));
}

void test_settle_deadline_handles_millisecond_wraparound() {
    const uint32_t started = UINT32_MAX - 10U;
    TEST_ASSERT_TRUE(request(started));
    beginSettle(started, 20U);

    TEST_ASSERT_TRUE(backgroundPaused(8U));
    TEST_ASSERT_FALSE(backgroundPaused(9U));
}

void test_cancel_and_prequiet_failsafe_restore_admission() {
    TEST_ASSERT_TRUE(request(100U));
    cancel();
    TEST_ASSERT_FALSE(backgroundPaused(101U));

    TEST_ASSERT_TRUE(request(200U));
    TEST_ASSERT_TRUE(backgroundPaused(2199U));
    TEST_ASSERT_FALSE(backgroundPaused(2200U));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_guard_admits_and_releases_background_activity);
    RUN_TEST(test_request_closes_admission_and_waits_for_existing_activity);
    RUN_TEST(test_max_wait_bounds_wake_latency_when_activity_remains_busy);
    RUN_TEST(test_settle_window_pauses_then_resumes_background_work);
    RUN_TEST(test_settle_deadline_handles_millisecond_wraparound);
    RUN_TEST(test_cancel_and_prequiet_failsafe_restore_admission);
    return UNITY_END();
}
