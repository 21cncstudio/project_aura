#include <unity.h>

#include "core/SharedI2cRuntimeGate.h"

void setUp() {}
void tearDown() {}

void test_gate_disable_is_one_way_and_idempotent() {
    SharedI2cRuntimeGate::Gate gate;

    TEST_ASSERT_TRUE(gate.available());
    TEST_ASSERT_TRUE(gate.disable());
    TEST_ASSERT_FALSE(gate.available());
    TEST_ASSERT_FALSE(gate.disable());
    TEST_ASSERT_FALSE(gate.available());
}

void test_gate_reopens_only_at_explicit_boot_boundary() {
    SharedI2cRuntimeGate::Gate gate;
    gate.disable();

    gate.resetForBoot();

    TEST_ASSERT_TRUE(gate.available());
}

void test_disable_closes_new_access_while_existing_lease_drains() {
    SharedI2cRuntimeGate::Gate gate;
    auto admitted = gate.acquire();
    TEST_ASSERT_TRUE(static_cast<bool>(admitted));
    TEST_ASSERT_EQUAL_UINT32(1U, gate.activeCount());
    TEST_ASSERT_FALSE(gate.idle());

    TEST_ASSERT_TRUE(gate.disable());
    TEST_ASSERT_FALSE(gate.available());
    auto rejected = gate.acquire();
    TEST_ASSERT_FALSE(static_cast<bool>(rejected));
    TEST_ASSERT_EQUAL_UINT32(1U, gate.activeCount());
    TEST_ASSERT_FALSE(gate.idle());

    admitted = SharedI2cRuntimeGate::Gate::Access{};
    TEST_ASSERT_TRUE(gate.idle());
    TEST_ASSERT_EQUAL_UINT32(0U, gate.activeCount());
}

void test_access_move_releases_exactly_once() {
    SharedI2cRuntimeGate::Gate gate;
    auto first = gate.acquire();
    TEST_ASSERT_EQUAL_UINT32(1U, gate.activeCount());

    auto second = std::move(first);
    TEST_ASSERT_FALSE(static_cast<bool>(first));
    TEST_ASSERT_TRUE(static_cast<bool>(second));
    TEST_ASSERT_EQUAL_UINT32(1U, gate.activeCount());

    second = SharedI2cRuntimeGate::Gate::Access{};
    TEST_ASSERT_TRUE(gate.idle());
}

void test_reset_is_rejected_while_access_is_active() {
    SharedI2cRuntimeGate::Gate gate;
    auto access = gate.acquire();
    gate.disable();

    TEST_ASSERT_FALSE(gate.resetForBoot());
    TEST_ASSERT_FALSE(gate.available());

    access = SharedI2cRuntimeGate::Gate::Access{};
    TEST_ASSERT_TRUE(gate.resetForBoot());
    TEST_ASSERT_TRUE(gate.available());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_gate_disable_is_one_way_and_idempotent);
    RUN_TEST(test_gate_reopens_only_at_explicit_boot_boundary);
    RUN_TEST(test_disable_closes_new_access_while_existing_lease_drains);
    RUN_TEST(test_access_move_releases_exactly_once);
    RUN_TEST(test_reset_is_rejected_while_access_is_active);
    return UNITY_END();
}
