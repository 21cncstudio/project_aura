#include <unity.h>

#include "Ch422gBoardPolicy.h"
#include "core/Ch422gReadyProbe.h"

namespace {

#if defined(AURA_HARDWARE_PROFILE_7) && AURA_HARDWARE_PROFILE_7
constexpr uint8_t kExpectedInitialIo = 0xD1U;
#else
constexpr uint8_t kExpectedInitialIo = 0xDBU;
#endif

struct WriteRecord {
    uint8_t address = 0;
    uint8_t value = 0;
    uint32_t timeout_ms = 0;
};

uint32_t now_ms = 0;
uint16_t start_calls = 0;
uint16_t write_calls = 0;
uint16_t stop_calls = 0;
uint16_t sample_calls = 0;
uint16_t recovery_calls = 0;
uint16_t delay_calls = 0;
uint32_t last_delay_ms = 0;
uint16_t fail_first_write_calls = 0;
uint16_t fail_on_write_call = 0;
bool advance_to_timeout_on_failure = false;
uint32_t failure_timeout_ms = 0;
esp_err_t write_failure_error = ESP_FAIL;
esp_err_t start_result = ESP_OK;
esp_err_t stop_result = ESP_OK;
Ch422gReadyProbe::LineState sampled_lines{false, true};
Ch422gReadyProbe::RecoveryResult recovered_bus{{true, true}, 9};
WriteRecord writes[64]{};

void resetFake() {
    now_ms = 0;
    start_calls = 0;
    write_calls = 0;
    stop_calls = 0;
    sample_calls = 0;
    recovery_calls = 0;
    delay_calls = 0;
    last_delay_ms = 0;
    fail_first_write_calls = 0;
    fail_on_write_call = 0;
    advance_to_timeout_on_failure = false;
    failure_timeout_ms = 0;
    write_failure_error = ESP_FAIL;
    start_result = ESP_OK;
    stop_result = ESP_OK;
    sampled_lines = {false, true};
    recovered_bus = {{true, true}, 9};
    for (auto &record : writes) {
        record = WriteRecord{};
    }
}

esp_err_t startHost(i2c_port_t, gpio_num_t, gpio_num_t, uint32_t) {
    ++start_calls;
    return start_result;
}

esp_err_t writeDevice(i2c_port_t,
                      uint8_t address,
                      uint8_t value,
                      uint32_t timeout_ms) {
    if (write_calls < (sizeof(writes) / sizeof(writes[0]))) {
        writes[write_calls] = WriteRecord{address, value, timeout_ms};
    }
    ++write_calls;

    const bool should_fail =
        write_calls <= fail_first_write_calls || write_calls == fail_on_write_call;
    if (should_fail && advance_to_timeout_on_failure) {
        now_ms = failure_timeout_ms;
    }
    return should_fail ? write_failure_error : ESP_OK;
}

esp_err_t stopHost(i2c_port_t) {
    ++stop_calls;
    return stop_result;
}

Ch422gReadyProbe::LineState sampleLines(gpio_num_t, gpio_num_t) {
    ++sample_calls;
    return sampled_lines;
}

Ch422gReadyProbe::RecoveryResult recoverBus(gpio_num_t, gpio_num_t) {
    ++recovery_calls;
    return recovered_bus;
}

uint32_t nowMs() {
    return now_ms;
}

void delayMs(uint32_t delay_ms) {
    ++delay_calls;
    last_delay_ms = delay_ms;
    now_ms += delay_ms;
}

const Ch422gReadyProbe::Ops ops{
    startHost,
    writeDevice,
    stopHost,
    sampleLines,
    recoverBus,
    nowMs,
    delayMs,
};

Ch422gReadyProbe::Result runProbe(uint32_t timeout_ms = 2000,
                                 uint32_t poll_ms = 250,
                                 uint32_t load_settle_ms = 250) {
    return Ch422gReadyProbe::detail::waitWithOps(
        I2C_NUM_0,
        static_cast<gpio_num_t>(8),
        static_cast<gpio_num_t>(9),
        100000,
        timeout_ms,
        poll_ms,
        load_settle_ms,
        25,
        ops);
}

void assertWrite(uint16_t index, uint8_t address, uint8_t value) {
    TEST_ASSERT_TRUE(index < write_calls);
    TEST_ASSERT_EQUAL_HEX8(address, writes[index].address);
    TEST_ASSERT_EQUAL_HEX8(value, writes[index].value);
}

void assertAllIoWritesUseProfileImage() {
    TEST_ASSERT_TRUE(write_calls <= (sizeof(writes) / sizeof(writes[0])));
    uint16_t io_write_calls = 0;
    for (uint16_t index = 0; index < write_calls; ++index) {
        if (writes[index].address == Ch422gReadyProbe::kWriteIoAddress) {
            ++io_write_calls;
            TEST_ASSERT_EQUAL_HEX8(kExpectedInitialIo, writes[index].value);
            TEST_ASSERT_EQUAL_HEX8(0U, writes[index].value & 0x20U);
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT16(0U, io_write_calls);
}

} // namespace

void setUp() {
    resetFake();
}

void tearDown() {}

void test_probe_uses_shared_native_usb_policy_and_exact_profile_image() {
    TEST_ASSERT_EQUAL_HEX8(0x20U, AURA_CH422G_USB_SEL_MASK);
    TEST_ASSERT_EQUAL_HEX8(kExpectedInitialIo, AURA_CH422G_INITIAL_IO_VALUE);
    TEST_ASSERT_EQUAL_HEX8(
        0U, AURA_CH422G_INITIAL_IO_VALUE & AURA_CH422G_BACKLIGHT_MASK);
    TEST_ASSERT_EQUAL_HEX8(
        AURA_CH422G_INITIAL_IO_VALUE, Ch422gReadyProbe::kWriteIoSafeValue);
    const auto result = runProbe();
    TEST_ASSERT_TRUE(result.ready());
    assertAllIoWritesUseProfileImage();
}

void test_probe_primes_used_io_then_validates_vendor_order() {
    const auto result = runProbe();
    TEST_ASSERT_TRUE(result.ready());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Phase::Complete),
                          static_cast<int>(result.phase));
    TEST_ASSERT_EQUAL_UINT16(1, result.attempts);
    TEST_ASSERT_EQUAL_UINT32(250, result.waited_ms);
    TEST_ASSERT_EQUAL_UINT16(5, write_calls);
    TEST_ASSERT_EQUAL_UINT16(1, start_calls);
    TEST_ASSERT_EQUAL_UINT16(1, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(0, recovery_calls);

    assertWrite(0, Ch422gReadyProbe::kWriteIoAddress, Ch422gReadyProbe::kWriteIoSafeValue);
    assertWrite(1, Ch422gReadyProbe::kWriteSetAddress, Ch422gReadyProbe::kWriteSetOutputValue);
    assertWrite(2, Ch422gReadyProbe::kWriteSetAddress, Ch422gReadyProbe::kWriteSetOutputValue);
    assertWrite(3, Ch422gReadyProbe::kWriteOcAddress, Ch422gReadyProbe::kWriteOcSafeValue);
    assertWrite(4, Ch422gReadyProbe::kWriteIoAddress, Ch422gReadyProbe::kWriteIoSafeValue);
}

void test_probe_retries_never_write_a_legacy_usb_high_image() {
    const esp_err_t failures[] = {ESP_FAIL, ESP_ERR_TIMEOUT};
    for (const auto failure : failures) {
        for (uint16_t failed_stage = 1; failed_stage <= 5; ++failed_stage) {
            resetFake();
            fail_on_write_call = failed_stage;
            write_failure_error = failure;
            const auto result = runProbe();

            TEST_ASSERT_TRUE(result.ready());
            TEST_ASSERT_EQUAL_UINT16(2, result.attempts);
            TEST_ASSERT_EQUAL_UINT16(failed_stage + 5, write_calls);
            assertAllIoWritesUseProfileImage();
        }
    }
}

void test_timeout_recreates_host_recovers_bus_and_then_succeeds() {
    fail_on_write_call = 1;
    write_failure_error = ESP_ERR_TIMEOUT;
    const auto result = runProbe();

    TEST_ASSERT_TRUE(result.ready());
    TEST_ASSERT_EQUAL_UINT16(2, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(2, start_calls);
    TEST_ASSERT_EQUAL_UINT16(2, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(1, sample_calls);
    TEST_ASSERT_EQUAL_UINT16(1, recovery_calls);
    TEST_ASSERT_EQUAL_UINT16(1, result.bus_recoveries);
    TEST_ASSERT_TRUE(result.failure_lines_valid);
    TEST_ASSERT_FALSE(result.failure_sda_high);
    TEST_ASSERT_TRUE(result.failure_scl_high);
    TEST_ASSERT_TRUE(result.recovery_sda_high);
    TEST_ASSERT_TRUE(result.recovery_scl_high);
    TEST_ASSERT_EQUAL_UINT8(9, result.recovery_pulses);
    TEST_ASSERT_EQUAL_UINT32(500, result.waited_ms);
    assertWrite(1, Ch422gReadyProbe::kWriteIoAddress, Ch422gReadyProbe::kWriteIoSafeValue);
}

void test_nack_recreates_host_without_gpio_recovery() {
    fail_on_write_call = 1;
    write_failure_error = ESP_FAIL;
    const auto result = runProbe();

    TEST_ASSERT_TRUE(result.ready());
    TEST_ASSERT_EQUAL_UINT16(2, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(2, start_calls);
    TEST_ASSERT_EQUAL_UINT16(2, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(1, sample_calls);
    TEST_ASSERT_EQUAL_UINT16(0, recovery_calls);
    TEST_ASSERT_EQUAL_UINT32(500, result.waited_ms);
}

void test_validation_failure_restarts_from_safe_io_on_fresh_host() {
    fail_on_write_call = 4;
    const auto result = runProbe();

    TEST_ASSERT_TRUE(result.ready());
    TEST_ASSERT_EQUAL_UINT16(2, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(2, start_calls);
    TEST_ASSERT_EQUAL_UINT16(2, stop_calls);
    TEST_ASSERT_EQUAL_UINT32(750, result.waited_ms);
    TEST_ASSERT_EQUAL_UINT16(9, write_calls);
    assertWrite(4, Ch422gReadyProbe::kWriteIoAddress, Ch422gReadyProbe::kWriteIoSafeValue);
}

void test_each_write_timeout_reports_phase_lines_and_recovers() {
    struct FailureCase {
        Ch422gReadyProbe::Phase phase;
        uint8_t address;
        uint8_t value;
    };
    const FailureCase cases[] = {
        {Ch422gReadyProbe::Phase::PrimeIo,
         Ch422gReadyProbe::kWriteIoAddress,
         Ch422gReadyProbe::kWriteIoSafeValue},
        {Ch422gReadyProbe::Phase::EnableOutputs,
         Ch422gReadyProbe::kWriteSetAddress,
         Ch422gReadyProbe::kWriteSetOutputValue},
        {Ch422gReadyProbe::Phase::ValidateSet,
         Ch422gReadyProbe::kWriteSetAddress,
         Ch422gReadyProbe::kWriteSetOutputValue},
        {Ch422gReadyProbe::Phase::ValidateOc,
         Ch422gReadyProbe::kWriteOcAddress,
         Ch422gReadyProbe::kWriteOcSafeValue},
        {Ch422gReadyProbe::Phase::ValidateIo,
         Ch422gReadyProbe::kWriteIoAddress,
         Ch422gReadyProbe::kWriteIoSafeValue},
    };

    for (uint16_t index = 0; index < (sizeof(cases) / sizeof(cases[0])); ++index) {
        resetFake();
        fail_on_write_call = index + 1;
        write_failure_error = ESP_ERR_TIMEOUT;
        advance_to_timeout_on_failure = true;
        failure_timeout_ms = 1000;
        const auto result = runProbe(1000);

        TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::Timeout),
                              static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(cases[index].phase),
                              static_cast<int>(result.phase));
        TEST_ASSERT_EQUAL_HEX8(cases[index].address, result.failed_address);
        TEST_ASSERT_EQUAL_HEX8(cases[index].value, result.failed_value);
        TEST_ASSERT_EQUAL_UINT16(index + 1, write_calls);
        TEST_ASSERT_EQUAL_UINT16(1, start_calls);
        TEST_ASSERT_EQUAL_UINT16(1, stop_calls);
        TEST_ASSERT_EQUAL_UINT16(1, recovery_calls);
        TEST_ASSERT_TRUE(result.failure_lines_valid);
    }
}

void test_probe_timeout_is_bounded_and_uses_fresh_hosts() {
    fail_first_write_calls = 100;
    write_failure_error = ESP_ERR_TIMEOUT;
    const auto result = runProbe(600, 250);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::Timeout),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(600, result.waited_ms);
    TEST_ASSERT_EQUAL_UINT16(3, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(3, start_calls);
    TEST_ASSERT_EQUAL_UINT16(3, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(3, recovery_calls);
    TEST_ASSERT_EQUAL_UINT16(3, result.bus_recoveries);
    TEST_ASSERT_EQUAL_HEX8(Ch422gReadyProbe::kWriteIoAddress, result.failed_address);
}

void test_production_timing_allows_one_passive_retry_before_timeout() {
    fail_first_write_calls = 100;
    write_failure_error = ESP_ERR_TIMEOUT;
    const auto result = runProbe(20000, 15000);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::Timeout),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(20000, result.waited_ms);
    TEST_ASSERT_EQUAL_UINT16(2, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(2, start_calls);
    TEST_ASSERT_EQUAL_UINT16(2, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(2, recovery_calls);
    TEST_ASSERT_EQUAL_UINT16(2, result.bus_recoveries);
}

void test_settle_does_not_overshoot_remaining_timeout_or_leak_host() {
    const auto result = runProbe(200, 50, 250);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::Timeout),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Phase::LoadSettle),
                          static_cast<int>(result.phase));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_TIMEOUT, result.last_error);
    TEST_ASSERT_EQUAL_UINT32(200, result.waited_ms);
    TEST_ASSERT_EQUAL_UINT16(2, write_calls);
    TEST_ASSERT_EQUAL_UINT16(1, start_calls);
    TEST_ASSERT_EQUAL_UINT16(1, stop_calls);
    TEST_ASSERT_EQUAL_UINT16(0, recovery_calls);
}

void test_host_start_failure_does_not_stop_unowned_host() {
    start_result = ESP_FAIL;
    const auto result = runProbe();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::HostStartFailed),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT16(1, result.attempts);
    TEST_ASSERT_EQUAL_UINT16(1, start_calls);
    TEST_ASSERT_EQUAL_UINT16(0, stop_calls);
}

void test_host_stop_failure_overrides_probe_success() {
    stop_result = ESP_FAIL;
    const auto result = runProbe();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::HostStopFailed),
                          static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.ready());
    TEST_ASSERT_EQUAL_UINT16(1, stop_calls);
}

void test_stop_failure_after_timeout_prevents_unsafe_recovery() {
    fail_on_write_call = 1;
    write_failure_error = ESP_ERR_TIMEOUT;
    stop_result = ESP_FAIL;
    const auto result = runProbe();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::HostStopFailed),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT16(1, sample_calls);
    TEST_ASSERT_EQUAL_UINT16(0, recovery_calls);
}

void test_invalid_timing_is_rejected_without_starting_host() {
    auto result = runProbe(1000, 0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::InvalidOps),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, result.last_error);
    TEST_ASSERT_EQUAL_UINT16(0, start_calls);

    result = runProbe(0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Ch422gReadyProbe::Status::InvalidOps),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT16(0, start_calls);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_probe_uses_shared_native_usb_policy_and_exact_profile_image);
    RUN_TEST(test_probe_primes_used_io_then_validates_vendor_order);
    RUN_TEST(test_probe_retries_never_write_a_legacy_usb_high_image);
    RUN_TEST(test_timeout_recreates_host_recovers_bus_and_then_succeeds);
    RUN_TEST(test_nack_recreates_host_without_gpio_recovery);
    RUN_TEST(test_validation_failure_restarts_from_safe_io_on_fresh_host);
    RUN_TEST(test_each_write_timeout_reports_phase_lines_and_recovers);
    RUN_TEST(test_probe_timeout_is_bounded_and_uses_fresh_hosts);
    RUN_TEST(test_production_timing_allows_one_passive_retry_before_timeout);
    RUN_TEST(test_settle_does_not_overshoot_remaining_timeout_or_leak_host);
    RUN_TEST(test_host_start_failure_does_not_stop_unowned_host);
    RUN_TEST(test_host_stop_failure_overrides_probe_success);
    RUN_TEST(test_stop_failure_after_timeout_prevents_unsafe_recovery);
    RUN_TEST(test_invalid_timing_is_rejected_without_starting_host);
    return UNITY_END();
}
