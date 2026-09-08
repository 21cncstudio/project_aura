#include <unity.h>

#include "core/NetworkIdentity.h"

void setUp() {}
void tearDown() {}

void test_mac_suffix_uses_device_specific_last_three_bytes() {
    const uint8_t mac[6] = {0x58, 0xE6, 0xC5, 0x12, 0x34, 0x56};

    TEST_ASSERT_EQUAL_HEX32(0x123456, NetworkIdentity::macSuffix24(mac));
}

void test_same_vendor_prefix_produces_distinct_device_suffixes() {
    const uint8_t first[6] = {0x58, 0xE6, 0xC5, 0x12, 0x34, 0x56};
    const uint8_t second[6] = {0x58, 0xE6, 0xC5, 0xAB, 0xCD, 0xEF};

    TEST_ASSERT_NOT_EQUAL(NetworkIdentity::macSuffix24(first),
                          NetworkIdentity::macSuffix24(second));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, NetworkIdentity::macSuffix24(second));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_mac_suffix_uses_device_specific_last_three_bytes);
    RUN_TEST(test_same_vendor_prefix_produces_distinct_device_suffixes);
    return UNITY_END();
}
