#include <unity.h>
#include <cstring>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/I2CHelper.h"
#include "core/Logger.h"
#include "drivers/Sfa30.h"
#include "esp_system.h"

namespace {

void encodeWordWithCrc(uint16_t word, uint8_t *dst) {
    dst[0] = static_cast<uint8_t>(word >> 8);
    dst[1] = static_cast<uint8_t>(word & 0xFF);
    dst[2] = I2C::crc8(dst, 2);
}

void encodeAsciiPair(char a, char b, uint8_t *dst) {
    encodeWordWithCrc((static_cast<uint16_t>(static_cast<uint8_t>(a)) << 8) |
                          static_cast<uint8_t>(b),
                      dst);
}

void setValidDeviceMarkingResponse() {
    static uint8_t marking_data[48];
    memset(marking_data, 0, sizeof(marking_data));
    const char kMarking[] = "SFA30-TEST";
    for (size_t i = 0; i < sizeof(marking_data) / 3; ++i) {
        const size_t char_index = i * 2U;
        const char a = (char_index < sizeof(kMarking) - 1U) ? kMarking[char_index] : '\0';
        const char b = ((char_index + 1U) < sizeof(kMarking) - 1U) ? kMarking[char_index + 1U] : '\0';
        encodeAsciiPair(a, b, &marking_data[i * 3U]);
    }
    I2cMock::setCommandRead(Config::SFA3X_ADDR,
                            Config::SFA30_CMD_GET_DEVICE_MARKING,
                            marking_data,
                            sizeof(marking_data));
}

bool recentContainsMessagePrefix(const char *prefix) {
    Logger::RecentEntry recent[16];
    const size_t count = Logger::copyRecent(recent, sizeof(recent) / sizeof(recent[0]));
    for (size_t i = 0; i < count; ++i) {
        if (strncmp(recent[i].message, prefix, strlen(prefix)) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

static_assert(Config::SFA3X_CMD_START == 0x0006, "SFA30 start opcode drifted from datasheet");
static_assert(Config::SFA3X_CMD_STOP == 0x0104, "SFA30 stop opcode drifted from datasheet");
static_assert(Config::SFA3X_CMD_READ_VALUES == 0x0327, "SFA30 read opcode drifted from datasheet");
static_assert(Config::SFA30_CMD_GET_DEVICE_MARKING == 0xD060,
              "SFA30 device-marking opcode drifted from datasheet");

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
    boot_reset_reason = ESP_RST_POWERON;
    boot_board_cold_start = true;
    boot_peripherals_cold_start = true;
    boot_peripherals_may_have_lost_power = true;
}

void tearDown() {}

void test_real_sfa30_start_keeps_absent_when_device_does_not_ack() {
    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    Logger::resetRecentForTest();
    sfa.start();

    TEST_ASSERT_FALSE(sfa.isPresent());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_FALSE(sfa.hasFault());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Absent),
                      static_cast<int>(sfa.status()));
    TEST_ASSERT_FALSE(recentContainsMessagePrefix("detect failed ("));
}

void test_real_sfa30_start_marks_fault_when_present_but_start_fails() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandFailure(Config::SFA3X_ADDR, Config::SFA3X_CMD_START, true);

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();

    TEST_ASSERT_TRUE(sfa.isPresent());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_TRUE(sfa.hasFault());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Fault),
                      static_cast<int>(sfa.status()));
}

void test_real_sfa30_warm_restart_stop_failure_marks_fault_when_device_acks() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandFailure(Config::SFA3X_ADDR, Config::SFA3X_CMD_STOP, true);
    boot_reset_reason = ESP_RST_SW;
    boot_board_cold_start = false;
    boot_peripherals_cold_start = false;
    boot_peripherals_may_have_lost_power = false;

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();

    TEST_ASSERT_TRUE(sfa.isPresent());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_TRUE(sfa.hasFault());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Fault),
                      static_cast<int>(sfa.status()));
}

void test_real_sfa30_reads_hcho_from_valid_frame() {
    uint8_t read_data[9];

    encodeWordWithCrc(50, &read_data[0]);
    encodeWordWithCrc(0x8000, &read_data[3]);
    encodeWordWithCrc(0x6666, &read_data[6]);

    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandRead(Config::SFA3X_ADDR,
                            Config::SFA3X_CMD_READ_VALUES,
                            read_data,
                            sizeof(read_data));

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();
    TEST_ASSERT_TRUE(sfa.isOk());

    setMillis(Config::SFA3X_POLL_MS);
    sfa.poll();

    float hcho_ppb = 0.0f;
    TEST_ASSERT_TRUE(sfa.takeNewData(hcho_ppb));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, hcho_ppb);
}

void test_real_sfa30_crc_errors_mark_fault_after_three_polls() {
    uint8_t read_data[9];

    encodeWordWithCrc(50, &read_data[0]);
    encodeWordWithCrc(0x8000, &read_data[3]);
    encodeWordWithCrc(0x6666, &read_data[6]);
    read_data[2] ^= 0xFF;

    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandRead(Config::SFA3X_ADDR,
                            Config::SFA3X_CMD_READ_VALUES,
                            read_data,
                            sizeof(read_data));

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();
    TEST_ASSERT_TRUE(sfa.isOk());

    setMillis(Config::SFA3X_POLL_MS);
    sfa.poll();
    TEST_ASSERT_FALSE(sfa.hasFault());

    setMillis(Config::SFA3X_POLL_MS * 2U);
    sfa.poll();
    TEST_ASSERT_FALSE(sfa.hasFault());

    setMillis(Config::SFA3X_POLL_MS * 3U);
    sfa.poll();
    TEST_ASSERT_TRUE(sfa.hasFault());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Fault),
                      static_cast<int>(sfa.status()));
}

void test_real_sfa30_can_restart_after_runtime_stop_failure_once_bus_recovers() {
    uint8_t read_data[9];

    encodeWordWithCrc(75, &read_data[0]);
    encodeWordWithCrc(0x8000, &read_data[3]);
    encodeWordWithCrc(0x6666, &read_data[6]);

    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandRead(Config::SFA3X_ADDR,
                            Config::SFA3X_CMD_READ_VALUES,
                            read_data,
                            sizeof(read_data));

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();
    TEST_ASSERT_TRUE(sfa.isOk());

    I2cMock::setCommandFailure(Config::SFA3X_ADDR, Config::SFA3X_CMD_STOP, true);
    sfa.stop();

    I2cMock::setCommandFailure(Config::SFA3X_ADDR, Config::SFA3X_CMD_STOP, false);
    sfa.start();

    TEST_ASSERT_TRUE(sfa.isPresent());
    TEST_ASSERT_TRUE(sfa.isOk());
    TEST_ASSERT_FALSE(sfa.hasFault());

    setMillis(Config::SFA3X_POLL_MS);
    sfa.poll();

    float hcho_ppb = 0.0f;
    TEST_ASSERT_TRUE(sfa.takeNewData(hcho_ppb));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, hcho_ppb);
}

void test_real_sfa30_reports_warmup_only_during_first_powerup_10_seconds() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();

    setMillis(Config::SFA30_POWERUP_SUPPRESS_MS - 1U);
    TEST_ASSERT_TRUE(sfa.isWarmupActive());

    setMillis(Config::SFA30_POWERUP_SUPPRESS_MS);
    TEST_ASSERT_FALSE(sfa.isWarmupActive());
}

void test_real_sfa30_warm_restart_does_not_report_powerup_warmup() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    boot_reset_reason = ESP_RST_SW;
    boot_board_cold_start = false;
    boot_peripherals_cold_start = false;
    boot_peripherals_may_have_lost_power = false;

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();

    TEST_ASSERT_FALSE(sfa.isWarmupActive());
}

void test_real_sfa30_brownout_preserves_unknown_state_and_applies_warmup() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    boot_reset_reason = ESP_RST_BROWNOUT;
    boot_board_cold_start = true;
    boot_peripherals_cold_start = false;
    boot_peripherals_may_have_lost_power = true;

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();
    TEST_ASSERT_TRUE(sfa.isOk());

    setMillis(Config::SFA30_POWERUP_SUPPRESS_MS - 1U);
    TEST_ASSERT_TRUE(sfa.isWarmupActive());

    setMillis(Config::SFA30_POWERUP_SUPPRESS_MS);
    TEST_ASSERT_FALSE(sfa.isWarmupActive());
}

void test_real_sfa30_start_marks_fault_when_device_marking_read_fails() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    I2cMock::setCommandFailure(Config::SFA3X_ADDR, Config::SFA30_CMD_GET_DEVICE_MARKING, true);

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    Logger::resetRecentForTest();
    sfa.start();

    TEST_ASSERT_TRUE(sfa.isPresent());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_TRUE(sfa.hasFault());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Fault),
                      static_cast<int>(sfa.status()));
    TEST_ASSERT_TRUE(recentContainsMessagePrefix("detect failed ("));
}

void test_real_sfa30_start_marks_fault_when_device_marking_is_empty() {
    static uint8_t empty_marking[48];
    memset(empty_marking, 0, sizeof(empty_marking));
    for (size_t i = 0; i < sizeof(empty_marking) / 3; ++i) {
        encodeWordWithCrc(0x0000, &empty_marking[i * 3U]);
    }

    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    I2cMock::setCommandRead(Config::SFA3X_ADDR,
                            Config::SFA30_CMD_GET_DEVICE_MARKING,
                            empty_marking,
                            sizeof(empty_marking));

    Sfa30 sfa;

    TEST_ASSERT_TRUE(sfa.begin());
    sfa.start();

    TEST_ASSERT_TRUE(sfa.isPresent());
    TEST_ASSERT_FALSE(sfa.isOk());
    TEST_ASSERT_TRUE(sfa.hasFault());
    TEST_ASSERT_EQUAL(static_cast<int>(Sfa30::Status::Fault),
                      static_cast<int>(sfa.status()));
}

void test_real_sfa30_cooperative_late_start_honors_command_delays() {
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();

    Sfa30 sfa;
    TEST_ASSERT_TRUE(sfa.begin());
    sfa.beginLateStart();

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Ping.
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Marking command.
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Wait.
    TEST_ASSERT_FALSE(sfa.isOk());

    setMillis(Config::SFA3X_READ_DELAY_MS);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Arm read.
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Read marking.
    TEST_ASSERT_TRUE(sfa.lateStartIdentified());
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis()))); // Start command.

    setMillis(Config::SFA3X_READ_DELAY_MS + Config::SFA30_START_SETTLE_MS - 1U);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sfa.pollLateStart(getMillis())));
    setMillis(Config::SFA3X_READ_DELAY_MS + Config::SFA30_START_SETTLE_MS);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(sfa.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sfa.isOk());
}

void test_real_sfa30_cooperative_warm_stop_failure_preserves_identification() {
    boot_reset_reason = ESP_RST_SW;
    boot_board_cold_start = false;
    boot_peripherals_cold_start = false;
    boot_peripherals_may_have_lost_power = false;
    I2cMock::setDevicePresent(Config::SFA3X_ADDR, true);
    setValidDeviceMarkingResponse();
    I2cMock::setCommandFailure(Config::SFA3X_ADDR,
                               Config::SFA3X_CMD_STOP,
                               true);

    Sfa30 sfa;
    TEST_ASSERT_TRUE(sfa.begin());
    sfa.beginLateStart();
    sfa.pollLateStart(getMillis()); // Ping.
    sfa.pollLateStart(getMillis()); // Marking command.
    setMillis(Config::SFA3X_READ_DELAY_MS);
    sfa.pollLateStart(getMillis()); // Arm read.
    sfa.pollLateStart(getMillis()); // Read marking.
    TEST_ASSERT_TRUE(sfa.lateStartIdentified());
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Failed),
                      static_cast<int>(sfa.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sfa.hasFault());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_real_sfa30_start_keeps_absent_when_device_does_not_ack);
    RUN_TEST(test_real_sfa30_start_marks_fault_when_present_but_start_fails);
    RUN_TEST(test_real_sfa30_warm_restart_stop_failure_marks_fault_when_device_acks);
    RUN_TEST(test_real_sfa30_reads_hcho_from_valid_frame);
    RUN_TEST(test_real_sfa30_crc_errors_mark_fault_after_three_polls);
    RUN_TEST(test_real_sfa30_can_restart_after_runtime_stop_failure_once_bus_recovers);
    RUN_TEST(test_real_sfa30_reports_warmup_only_during_first_powerup_10_seconds);
    RUN_TEST(test_real_sfa30_warm_restart_does_not_report_powerup_warmup);
    RUN_TEST(test_real_sfa30_brownout_preserves_unknown_state_and_applies_warmup);
    RUN_TEST(test_real_sfa30_start_marks_fault_when_device_marking_read_fails);
    RUN_TEST(test_real_sfa30_start_marks_fault_when_device_marking_is_empty);
    RUN_TEST(test_real_sfa30_cooperative_late_start_honors_command_delays);
    RUN_TEST(test_real_sfa30_cooperative_warm_stop_failure_preserves_identification);
    return UNITY_END();
}
