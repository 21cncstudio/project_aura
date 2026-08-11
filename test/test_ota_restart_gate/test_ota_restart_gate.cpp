// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "web/OtaRestartGate.h"

void setUp() {}
void tearDown() {}

void test_upload_excludes_restart_until_upload_ends() {
    OtaRestartGate gate;

    TEST_ASSERT_TRUE(gate.tryBeginUpload());
    TEST_ASSERT_TRUE(gate.uploadActive());
    TEST_ASSERT_TRUE(gate.busy());
    TEST_ASSERT_FALSE(gate.tryBeginRestart());

    gate.endUpload();
    TEST_ASSERT_FALSE(gate.busy());
    TEST_ASSERT_TRUE(gate.tryBeginRestart());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OtaRestartGate::State::Restart),
                            static_cast<uint8_t>(gate.state()));
}

void test_restart_excludes_upload() {
    OtaRestartGate gate;

    TEST_ASSERT_TRUE(gate.tryBeginRestart());
    TEST_ASSERT_FALSE(gate.tryBeginUpload());
    TEST_ASSERT_FALSE(gate.uploadActive());
    TEST_ASSERT_TRUE(gate.busy());
}

void test_gate_rejects_duplicate_upload_and_reset_restores_idle() {
    OtaRestartGate gate;

    TEST_ASSERT_TRUE(gate.tryBeginUpload());
    TEST_ASSERT_FALSE(gate.tryBeginUpload());
    gate.reset();

    TEST_ASSERT_FALSE(gate.busy());
    TEST_ASSERT_TRUE(gate.tryBeginUpload());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_upload_excludes_restart_until_upload_ends);
    RUN_TEST(test_restart_excludes_upload);
    RUN_TEST(test_gate_rejects_duplicate_upload_and_reset_restores_idle);
    return UNITY_END();
}
