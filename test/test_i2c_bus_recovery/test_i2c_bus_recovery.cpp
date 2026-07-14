#include <unity.h>

#include <vector>

#include "core/I2cBusRecovery.h"

namespace {

constexpr gpio_num_t kSda = 8;
constexpr gpio_num_t kScl = 9;

enum class OpType : uint8_t { Level, Direction, Pull, Delay };

struct Op {
    OpType type;
    gpio_num_t pin;
    int value;
};

struct FakeGpio {
    bool sda_high = true;
    bool scl_high = true;
    bool sda_permanently_low = false;
    bool scl_permanently_low = false;
    bool scl_fails_after_first_low = false;
    bool scl_was_driven_low = false;
    uint8_t release_sda_at_pulse = 0;
    uint8_t pulses = 0;
    uint8_t sda_low_writes = 0;
    gpio_mode_t sda_mode = GPIO_MODE_INPUT;
    gpio_mode_t scl_mode = GPIO_MODE_INPUT;
    gpio_pull_mode_t sda_pull = GPIO_FLOATING;
    gpio_pull_mode_t scl_pull = GPIO_FLOATING;
    std::vector<Op> ops;
};

FakeGpio fake;

esp_err_t setLevel(gpio_num_t pin, uint32_t level) {
    fake.ops.push_back({OpType::Level, pin, static_cast<int>(level)});
    if (pin == kSda) {
        if (level == 0) {
            ++fake.sda_low_writes;
        }
        fake.sda_high = !fake.sda_permanently_low && level != 0;
        return ESP_OK;
    }

    if (level == 0) {
        fake.scl_was_driven_low = true;
        fake.scl_high = false;
    } else if (fake.scl_permanently_low ||
               (fake.scl_fails_after_first_low && fake.scl_was_driven_low)) {
        fake.scl_high = false;
    } else {
        if (fake.scl_was_driven_low) {
            ++fake.pulses;
            if (fake.release_sda_at_pulse != 0 && fake.pulses >= fake.release_sda_at_pulse) {
                fake.sda_permanently_low = false;
                fake.sda_high = true;
            }
        }
        fake.scl_high = true;
    }
    return ESP_OK;
}

esp_err_t setDirection(gpio_num_t pin, gpio_mode_t mode) {
    fake.ops.push_back({OpType::Direction, pin, static_cast<int>(mode)});
    if (pin == kSda) fake.sda_mode = mode;
    if (pin == kScl) fake.scl_mode = mode;
    return ESP_OK;
}

esp_err_t setPull(gpio_num_t pin, gpio_pull_mode_t mode) {
    fake.ops.push_back({OpType::Pull, pin, static_cast<int>(mode)});
    if (pin == kSda) fake.sda_pull = mode;
    if (pin == kScl) fake.scl_pull = mode;
    return ESP_OK;
}

int getLevel(gpio_num_t pin) {
    return pin == kSda ? (fake.sda_high ? 1 : 0) : (fake.scl_high ? 1 : 0);
}

void delayUs(uint32_t delay_us) {
    fake.ops.push_back({OpType::Delay, -1, static_cast<int>(delay_us)});
}

I2cBusRecovery::GpioOps ops{
    setLevel,
    setDirection,
    setPull,
    getLevel,
    delayUs,
};

I2cBusRecovery::Result recover() {
    return I2cBusRecovery::detail::recoverWithOps(kSda, kScl, ops);
}

void assertPassive() {
    TEST_ASSERT_EQUAL_INT(GPIO_MODE_INPUT, fake.sda_mode);
    TEST_ASSERT_EQUAL_INT(GPIO_MODE_INPUT, fake.scl_mode);
    TEST_ASSERT_EQUAL_INT(GPIO_FLOATING, fake.sda_pull);
    TEST_ASSERT_EQUAL_INT(GPIO_FLOATING, fake.scl_pull);
}

} // namespace

void setUp() {
    fake = FakeGpio{};
}

void tearDown() {}

void test_idle_bus_uses_no_pulses_and_leaves_pins_passive() {
    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::Idle),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.busReady());
    TEST_ASSERT_EQUAL_UINT8(0, result.pulses);
    TEST_ASSERT_EQUAL_UINT8(0, fake.pulses);
    assertPassive();
}

void test_high_latches_are_written_before_open_drain_output() {
    (void)recover();

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(4, fake.ops.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OpType::Level), static_cast<int>(fake.ops[0].type));
    TEST_ASSERT_EQUAL_INT(kSda, fake.ops[0].pin);
    TEST_ASSERT_EQUAL_INT(1, fake.ops[0].value);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OpType::Level), static_cast<int>(fake.ops[1].type));
    TEST_ASSERT_EQUAL_INT(kScl, fake.ops[1].pin);
    TEST_ASSERT_EQUAL_INT(1, fake.ops[1].value);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OpType::Direction), static_cast<int>(fake.ops[2].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OpType::Direction), static_cast<int>(fake.ops[3].type));
}

void test_sda_release_on_selected_pulse_is_recovered() {
    fake.sda_high = false;
    fake.sda_permanently_low = true;
    fake.release_sda_at_pulse = 4;

    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::Recovered),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT8(4, result.pulses);
    TEST_ASSERT_TRUE(result.after.idle());
    assertPassive();
}

void test_sda_stays_low_after_nine_pulses() {
    fake.sda_high = false;
    fake.sda_permanently_low = true;

    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::SdaStuckLow),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT8(9, result.pulses);
    TEST_ASSERT_FALSE(result.busReady());
    assertPassive();
}

void test_scl_initially_low_generates_no_clocks() {
    fake.scl_high = false;
    fake.scl_permanently_low = true;

    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::SclStuckLow),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT8(0, result.pulses);
    TEST_ASSERT_EQUAL_UINT8(0, fake.pulses);
    assertPassive();
}

void test_both_lines_low_are_classified_without_clocks() {
    fake.sda_high = false;
    fake.scl_high = false;
    fake.sda_permanently_low = true;
    fake.scl_permanently_low = true;

    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::BothStuckLow),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT8(0, result.pulses);
    assertPassive();
}

void test_scl_that_does_not_release_aborts_without_stop() {
    fake.sda_high = false;
    fake.sda_permanently_low = true;
    fake.scl_fails_after_first_low = true;

    const auto result = recover();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(I2cBusRecovery::Status::BothStuckLow),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT8(1, result.pulses);
    TEST_ASSERT_EQUAL_UINT8(0, fake.sda_low_writes);
    assertPassive();
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_bus_uses_no_pulses_and_leaves_pins_passive);
    RUN_TEST(test_high_latches_are_written_before_open_drain_output);
    RUN_TEST(test_sda_release_on_selected_pulse_is_recovered);
    RUN_TEST(test_sda_stays_low_after_nine_pulses);
    RUN_TEST(test_scl_initially_low_generates_no_clocks);
    RUN_TEST(test_both_lines_low_are_classified_without_clocks);
    RUN_TEST(test_scl_that_does_not_release_aborts_without_stop);
    return UNITY_END();
}
