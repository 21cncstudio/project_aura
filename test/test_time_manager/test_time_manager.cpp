#include <unity.h>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "SntpMock.h"
#include "TimeMock.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "modules/StorageManager.h"
#include "modules/TimeManager.h"

namespace {

uint8_t toBcd(uint8_t value) {
    return static_cast<uint8_t>(value + 6 * (value / 10));
}

void seedPcf8523WithOldValidTime() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    I2cMock::setReadWrap(Config::PCF8523_ADDR, Config::PCF8523_REG_TMR_B_REG);

    const uint8_t signature[] = {0x00, 0x00, 0x07, 0x00, 0x07};
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_OFFSET,
                          signature, sizeof(signature));

    const uint8_t time_regs[] = {
        toBcd(56), toBcd(34), toBcd(12), toBcd(15), 0x02, toBcd(4), toBcd(19)
    };
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS,
                          time_regs, sizeof(time_regs));
    I2cMock::setRegister(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_3, 0x00);
}

void seedPcf8523WithUnsetTime() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    I2cMock::setReadWrap(Config::PCF8523_ADDR, Config::PCF8523_REG_TMR_B_REG);

    const uint8_t signature[] = {0x00, 0x00, 0x07, 0x00, 0x07};
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_OFFSET,
                          signature, sizeof(signature));

    const uint8_t time_regs[] = {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS,
                          time_regs, sizeof(time_regs));
    I2cMock::setRegister(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_3, 0x00);
}

void seedDs3231WithOldValidTime() {
    I2cMock::setDevicePresent(Config::DS3231_ADDR, true);
    I2cMock::setReadWrap(Config::DS3231_ADDR, Config::DS3231_REG_TEMP_LSB);

    const uint8_t meta_regs[] = {0x00, 0x00, 0x19, 0x40};
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_STATUS,
                          meta_regs, sizeof(meta_regs));

    const uint8_t time_regs[] = {
        toBcd(56), toBcd(34), toBcd(12), 0x02, toBcd(15), toBcd(4), toBcd(19)
    };
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_SECONDS,
                          time_regs, sizeof(time_regs));
}

void seedDs3231WithFreshValidTime() {
    I2cMock::setDevicePresent(Config::DS3231_ADDR, true);
    I2cMock::setReadWrap(Config::DS3231_ADDR, Config::DS3231_REG_TEMP_LSB);

    const uint8_t meta_regs[] = {0x00, 0x00, 0x19, 0x40};
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_STATUS,
                          meta_regs, sizeof(meta_regs));

    const uint8_t time_regs[] = {
        toBcd(56), toBcd(34), toBcd(12), 0x02, toBcd(15), toBcd(4), toBcd(26)
    };
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_SECONDS,
                          time_regs, sizeof(time_regs));
}

void seedDs3231WithDirtyCalendar() {
    I2cMock::setDevicePresent(Config::DS3231_ADDR, true);
    I2cMock::setReadWrap(Config::DS3231_ADDR, Config::DS3231_REG_TEMP_LSB);

    const uint8_t meta_regs[] = {0x00, 0x00, 0x19, 0x40};
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_STATUS,
                          meta_regs, sizeof(meta_regs));

    const uint8_t time_regs[] = {
        toBcd(56), toBcd(34), toBcd(12), 0x00, 0x00, 0x00, toBcd(19)
    };
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_SECONDS,
                          time_regs, sizeof(time_regs));
}

void seedDs3231ThatLooksLikePcf8523Fallback() {
    I2cMock::setDevicePresent(Config::DS3231_ADDR, true);
    I2cMock::setReadWrap(Config::DS3231_ADDR, Config::DS3231_REG_TEMP_LSB);

    const uint8_t meta_regs[] = {0x00, 0x00, 0x19, 0x40};
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_STATUS,
                          meta_regs, sizeof(meta_regs));

    const uint8_t time_regs[] = {
        toBcd(8), toBcd(34), toBcd(9), 4, toBcd(12), toBcd(3), toBcd(19)
    };
    I2cMock::setRegisters(Config::DS3231_ADDR, Config::DS3231_REG_SECONDS,
                          time_regs, sizeof(time_regs));

    const uint8_t alarm1_regs[] = {3, 4, 5};
    I2cMock::setRegisters(Config::DS3231_ADDR, 0x07, alarm1_regs, sizeof(alarm1_regs));
}

void seedPcf8523WithFreshValidTime() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    I2cMock::setReadWrap(Config::PCF8523_ADDR, Config::PCF8523_REG_TMR_B_REG);

    const uint8_t signature[] = {0x00, 0x00, 0x07, 0x00, 0x07};
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_OFFSET,
                          signature, sizeof(signature));

    const uint8_t time_regs[] = {
        toBcd(56), toBcd(34), toBcd(12), toBcd(15), 0x02, toBcd(4), toBcd(26)
    };
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS,
                          time_regs, sizeof(time_regs));
    I2cMock::setRegister(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_3, 0x00);
}

void seedPcf8523WithLeapDayValidTime() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    I2cMock::setReadWrap(Config::PCF8523_ADDR, Config::PCF8523_REG_TMR_B_REG);

    const uint8_t signature[] = {0x00, 0x00, 0x07, 0x00, 0x07};
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_OFFSET,
                          signature, sizeof(signature));

    const uint8_t time_regs[] = {
        toBcd(0), toBcd(0), toBcd(0), toBcd(29), 0x04, toBcd(2), toBcd(24)
    };
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS,
                          time_regs, sizeof(time_regs));
    I2cMock::setRegister(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_3, 0x00);
}

void seedPcf8523ThatLooksLikeWeakDs3231() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    I2cMock::setReadWrap(Config::PCF8523_ADDR, Config::PCF8523_REG_TMR_B_REG);

    const uint8_t control[] = {0x00, 0x00, 0x00};
    const uint8_t time_regs[] = {
        toBcd(8), toBcd(34), toBcd(9), toBcd(26), 0x02, toBcd(4), toBcd(26)
    };
    const uint8_t timer_regs[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_1,
                          control, sizeof(control));
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS,
                          time_regs, sizeof(time_regs));
    I2cMock::setRegisters(Config::PCF8523_ADDR, Config::PCF8523_REG_OFFSET,
                          timer_regs, sizeof(timer_regs));
}

} // namespace

void setUp() {
    setMillis(0);
    setNowEpoch(0);
    boot_reset_reason = ESP_RST_POWERON;
    I2cMock::reset();
    SntpMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
    Logger::resetRecentForTest();
}

void tearDown() {
    Logger::resetRecentForTest();
}

void test_time_manager_init_rtc_handles_absent_rtc() {
    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("RTC", manager.rtcLabel());
}

void test_time_manager_init_rtc_selects_pcf8523() {
    seedPcf8523WithOldValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_init_rtc_marks_unset_pcf8523_as_time_unset_not_fault() {
    seedPcf8523WithUnsetTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_TRUE(manager.isRtcLostPower());
    TEST_ASSERT_TRUE(manager.isRtcTimeUnset());
    TEST_ASSERT_FALSE(manager.isRtcReadFault());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_does_not_trust_plausible_rtc_time_after_power_loss() {
    seedPcf8523WithFreshValidTime();
    const uint8_t seconds = I2cMock::getRegister(
        Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS);
    I2cMock::setRegister(
        Config::PCF8523_ADDR,
        Config::PCF8523_REG_SECONDS,
        static_cast<uint8_t>(seconds | 0x80));

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_TRUE(manager.isRtcLostPower());
    TEST_ASSERT_TRUE(manager.isRtcTimeUnset());
    TEST_ASSERT_EQUAL_INT64(0, static_cast<long long>(mockNow()));
    TEST_ASSERT_BITS_HIGH(
        0x80,
        I2cMock::getRegister(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS));
}

void test_time_manager_set_local_time_initializes_unset_rtc() {
    seedPcf8523WithUnsetTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcTimeUnset());

    TEST_ASSERT_TRUE(manager.setLocalTime(2026, 4, 15, 12, 34));
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_FALSE(manager.isRtcTimeUnset());
}

void test_time_manager_init_rtc_selects_ds3231() {
    seedDs3231WithOldValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_FALSE(manager.isRtcTimeUnset());
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());
}

void test_time_manager_init_rtc_keeps_dirty_ds3231_visible() {
    seedDs3231WithDirtyCalendar();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());
}

void test_time_manager_init_rtc_keeps_dirty_ds3231_visible_when_osf_set() {
    seedDs3231WithDirtyCalendar();
    I2cMock::setRegister(Config::DS3231_ADDR, Config::DS3231_REG_STATUS, Config::DS3231_STATUS_OSF);

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_TRUE(manager.isRtcLostPower());
    TEST_ASSERT_TRUE(manager.isRtcTimeUnset());
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());
}

void test_time_manager_init_rtc_prefers_ds3231_before_pcf8523_fallback() {
    seedDs3231ThatLooksLikePcf8523Fallback();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());
}

void test_time_manager_init_rtc_retries_weak_ds3231_candidate_as_pcf8523() {
    seedPcf8523ThatLooksLikeWeakDs3231();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_init_rtc_respects_manual_pcf8523_mode() {
    seedPcf8523WithFreshValidTime();

    StorageManager storage;
    storage.begin();
    storage.config().rtc_mode = Config::RtcMode::Pcf8523;

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Pcf8523),
                          static_cast<int>(manager.configuredRtcMode()));
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_init_rtc_respects_manual_ds3231_mode() {
    seedDs3231WithFreshValidTime();

    StorageManager storage;
    storage.begin();
    storage.config().rtc_mode = Config::RtcMode::Ds3231;

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Ds3231),
                          static_cast<int>(manager.configuredRtcMode()));
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());
}

void test_time_manager_init_rtc_manual_ds3231_mode_does_not_fall_back_to_pcf8523() {
    seedPcf8523WithFreshValidTime();

    StorageManager storage;
    storage.begin();
    storage.config().rtc_mode = Config::RtcMode::Ds3231;

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Ds3231),
                          static_cast<int>(manager.configuredRtcMode()));
    TEST_ASSERT_EQUAL_STRING("RTC", manager.rtcLabel());
}

void test_time_manager_init_rtc_sets_system_epoch_from_utc_time() {
    seedPcf8523WithFreshValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_INT64(1776256496LL, static_cast<long long>(mockNow()));
}

void test_time_manager_init_rtc_sets_system_epoch_for_leap_day() {
    seedPcf8523WithLeapDayValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_INT64(1709164800LL, static_cast<long long>(mockNow()));
}

void test_time_manager_init_rtc_keeps_detected_pcf8523_when_begin_fails() {
    seedPcf8523WithOldValidTime();
    I2cMock::setWriteFailure(Config::PCF8523_ADDR, Config::PCF8523_REG_CONTROL_3, true);

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_init_rtc_keeps_detected_pcf8523_when_initial_read_fails() {
    seedPcf8523WithOldValidTime();
    I2cMock::setReadFailure(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS, true);

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_poll_marks_detected_rtc_invalid_after_repeated_read_failures() {
    seedPcf8523WithFreshValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());

    I2cMock::setReadFailure(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS, true);

    for (uint8_t i = 0; i < Config::RTC_STATUS_READ_FAIL_LIMIT - 1; ++i) {
        advanceMillis(Config::RTC_STATUS_POLL_MS);
        const auto result = manager.poll(getMillis());
        TEST_ASSERT_FALSE(result.state_changed);
        TEST_ASSERT_TRUE(manager.isRtcPresent());
        TEST_ASSERT_TRUE(manager.isRtcValid());
    }

    advanceMillis(Config::RTC_STATUS_POLL_MS);
    const auto result = manager.poll(getMillis());
    TEST_ASSERT_TRUE(result.state_changed);
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcValid());
}

void test_time_manager_poll_recovers_rtc_after_runtime_read_failures() {
    seedPcf8523WithFreshValidTime();

    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.initRtc());
    I2cMock::setReadFailure(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS, true);

    for (uint8_t i = 0; i < Config::RTC_STATUS_READ_FAIL_LIMIT; ++i) {
        advanceMillis(Config::RTC_STATUS_POLL_MS);
        manager.poll(getMillis());
    }

    TEST_ASSERT_FALSE(manager.isRtcValid());

    I2cMock::setReadFailure(Config::PCF8523_ADDR, Config::PCF8523_REG_SECONDS, false);
    advanceMillis(Config::RTC_STATUS_POLL_MS);
    const auto result = manager.poll(getMillis());
    TEST_ASSERT_TRUE(result.state_changed);
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcValid());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_time_manager_init_rtc_handles_absent_rtc);
    RUN_TEST(test_time_manager_init_rtc_selects_pcf8523);
    RUN_TEST(test_time_manager_init_rtc_marks_unset_pcf8523_as_time_unset_not_fault);
    RUN_TEST(test_time_manager_does_not_trust_plausible_rtc_time_after_power_loss);
    RUN_TEST(test_time_manager_set_local_time_initializes_unset_rtc);
    RUN_TEST(test_time_manager_init_rtc_selects_ds3231);
    RUN_TEST(test_time_manager_init_rtc_keeps_dirty_ds3231_visible);
    RUN_TEST(test_time_manager_init_rtc_keeps_dirty_ds3231_visible_when_osf_set);
    RUN_TEST(test_time_manager_init_rtc_prefers_ds3231_before_pcf8523_fallback);
    RUN_TEST(test_time_manager_init_rtc_retries_weak_ds3231_candidate_as_pcf8523);
    RUN_TEST(test_time_manager_init_rtc_respects_manual_pcf8523_mode);
    RUN_TEST(test_time_manager_init_rtc_respects_manual_ds3231_mode);
    RUN_TEST(test_time_manager_init_rtc_manual_ds3231_mode_does_not_fall_back_to_pcf8523);
    RUN_TEST(test_time_manager_init_rtc_sets_system_epoch_from_utc_time);
    RUN_TEST(test_time_manager_init_rtc_sets_system_epoch_for_leap_day);
    RUN_TEST(test_time_manager_init_rtc_keeps_detected_pcf8523_when_begin_fails);
    RUN_TEST(test_time_manager_init_rtc_keeps_detected_pcf8523_when_initial_read_fails);
    RUN_TEST(test_time_manager_poll_marks_detected_rtc_invalid_after_repeated_read_failures);
    RUN_TEST(test_time_manager_poll_recovers_rtc_after_runtime_read_failures);
    return UNITY_END();
}
