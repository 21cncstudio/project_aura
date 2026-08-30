#include <unity.h>

#include <cstring>

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

TimeManager::PollResult pollDeferredChecked(TimeManager &manager,
                                            uint32_t advance_ms = 1000U,
                                            bool rtc_i2c_available = true) {
    advanceMillis(advance_ms);
    const uint32_t before = I2cMock::transactionCount();
    const TimeManager::PollResult result =
        manager.poll(getMillis(), rtc_i2c_available);
    const uint32_t transactions = I2cMock::transactionCount() - before;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1U, transactions);
    return result;
}

template <typename Predicate>
TimeManager::PollResult pollDeferredUntil(TimeManager &manager,
                                          Predicate done,
                                          uint16_t max_steps = 80U,
                                          uint32_t advance_ms = 1000U) {
    TimeManager::PollResult combined;
    for (uint16_t step = 0; step < max_steps && !done(); ++step) {
        const TimeManager::PollResult current =
            pollDeferredChecked(manager, advance_ms);
        combined.state_changed = combined.state_changed || current.state_changed;
        combined.time_updated = combined.time_updated || current.time_updated;
    }
    TEST_ASSERT_TRUE_MESSAGE(done(), "deferred RTC operation did not settle");
    return combined;
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
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcLostPower());
    TEST_ASSERT_EQUAL_STRING("RTC", manager.rtcLabel());
}

void test_time_manager_plausible_process_epoch_is_not_boot_trusted() {
    StorageManager storage;
    storage.begin();
    constexpr time_t plausible_epoch = 1776256496;
    setNowEpoch(plausible_epoch);

    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.isSystemTimeValid());
    TEST_ASSERT_FALSE(manager.isSystemTimeTrusted());

    TEST_ASSERT_TRUE(manager.setLocalTime(2026, 4, 15, 12, 34));
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
    manager.begin(storage);
    TEST_ASSERT_TRUE(manager.isSystemTimeValid());
    TEST_ASSERT_FALSE(manager.isSystemTimeTrusted());
}

void test_time_manager_validates_manual_calendar_before_trusting() {
    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);
    const uint32_t transactions_before = I2cMock::transactionCount();

    TEST_ASSERT_FALSE(manager.setLocalTime(2019, 12, 31, 12, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2100, 1, 1, 12, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2026, 13, 1, 12, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2026, 4, 31, 12, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2023, 2, 29, 12, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2026, 4, 15, 24, 0));
    TEST_ASSERT_FALSE(manager.setLocalTime(2026, 4, 15, 12, 60));
    TEST_ASSERT_FALSE(manager.isSystemTimeValid());
    TEST_ASSERT_FALSE(manager.isSystemTimeTrusted());
    TEST_ASSERT_EQUAL_INT64(0, static_cast<long long>(mockNow()));
    TEST_ASSERT_EQUAL_UINT32(transactions_before, I2cMock::transactionCount());

    TEST_ASSERT_TRUE(manager.setLocalTime(2024, 2, 29, 12, 34));
    TEST_ASSERT_TRUE(manager.isSystemTimeValid());
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
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
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
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
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
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
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
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
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
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
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
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

void test_time_manager_poll_detects_rtc_that_appears_during_startup_window() {
    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    seedPcf8523WithUnsetTime();
    setMillis(999);
    auto before_due = manager.poll(getMillis());
    TEST_ASSERT_FALSE(before_due.state_changed);
    TEST_ASSERT_FALSE(manager.isRtcPresent());

    const auto initialized = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(initialized.state_changed);
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcTimeUnset());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());

    I2cMock::setDevicePresent(Config::PCF8523_ADDR, false);
    advanceMillis(Config::RTC_STATUS_POLL_MS);
    manager.poll(getMillis());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
}

void test_time_manager_retries_begin_failure_then_initializes_without_blocking_poll() {
    seedPcf8523WithFreshValidTime();
    I2cMock::setWriteFailure(Config::PCF8523_ADDR,
                             Config::PCF8523_REG_CONTROL_3,
                             true);

    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    I2cMock::setWriteFailure(Config::PCF8523_ADDR,
                             Config::PCF8523_REG_CONTROL_3,
                             false);
    const auto completed = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(completed.state_changed);
    TEST_ASSERT_TRUE(completed.time_updated);
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
}

void test_time_manager_sync_unresolved_weak_ds_keeps_bounded_probe_active() {
    seedPcf8523ThatLooksLikeWeakDs3231();
    I2cMock::setReadFailureOnCall(Config::PCF8523_ADDR,
                                  Config::PCF8523_REG_CONTROL_1,
                                  6U);

    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    setMillis(1000);
    manager.poll(getMillis());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
}

void test_time_manager_deferred_weak_ds_switches_to_pcf_before_success() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523ThatLooksLikeWeakDs3231();
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "DS3231") == 0; });
    TEST_ASSERT_EQUAL_STRING("DS3231", manager.rtcLabel());

    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "PCF8523") == 0; });
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    const auto completed = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(completed.state_changed);
    TEST_ASSERT_TRUE(completed.time_updated);
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
}

void test_time_manager_deferred_unresolved_weak_ds_records_failure_and_retries() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523ThatLooksLikeWeakDs3231();
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "DS3231") == 0; });
    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_CONTROL_1,
                            true);

    TimeManager::PollResult unresolved;
    for (uint8_t step = 0; step < 30U && !unresolved.state_changed; ++step) {
        unresolved = pollDeferredChecked(manager);
    }

    TEST_ASSERT_TRUE(unresolved.state_changed);
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_CONTROL_1,
                            false);
    pollDeferredChecked(manager);
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
}

void test_time_manager_deferred_all_read_failures_recover_in_runtime_poll() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523WithFreshValidTime();
    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_SECONDS,
                            true);
    const auto initialized_without_read = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });

    TEST_ASSERT_TRUE(initialized_without_read.state_changed);
    TEST_ASSERT_TRUE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());

    // A plausible process epoch has no boot-local provenance and must be
    // replaced by the first valid RTC status read.
    constexpr time_t plausible_untrusted_epoch = 1893456000;
    constexpr time_t rtc_epoch = 1776256496;
    setNowEpoch(plausible_untrusted_epoch);
    TEST_ASSERT_TRUE(manager.isSystemTimeValid());
    TEST_ASSERT_FALSE(manager.isSystemTimeTrusted());

    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_SECONDS,
                            false);
    advanceMillis(Config::RTC_STATUS_POLL_MS);
    const auto recovered = manager.poll(getMillis());
    TEST_ASSERT_TRUE(recovered.state_changed);
    TEST_ASSERT_TRUE(recovered.time_updated);
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
    TEST_ASSERT_EQUAL_INT64(static_cast<long long>(rtc_epoch),
                            static_cast<long long>(mockNow()));
}

void test_time_manager_deferred_rtc_deadline_handles_millis_wraparound() {
    constexpr uint32_t start_ms = UINT32_MAX - 1200U;
    setMillis(start_ms);

    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523WithFreshValidTime();
    const auto completed = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(completed.state_changed);
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
}

void test_time_manager_plausible_process_epoch_does_not_seed_deferred_rtc() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523WithOldValidTime();
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "PCF8523") == 0; });
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    constexpr time_t plausible_epoch = 1776256496;
    setNowEpoch(plausible_epoch);
    const auto completed = pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });

    TEST_ASSERT_TRUE(completed.state_changed);
    TEST_ASSERT_EQUAL_INT64(static_cast<long long>(plausible_epoch),
                            static_cast<long long>(mockNow()));
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcValid());
    TEST_ASSERT_FALSE(manager.isSystemTimeTrusted());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
    TEST_ASSERT_EQUAL_UINT8(
        toBcd(19),
        I2cMock::getRegister(
            Config::PCF8523_ADDR,
            static_cast<uint8_t>(Config::PCF8523_REG_SECONDS + 6U)));
}

void test_time_manager_stops_missing_rtc_detection_after_five_attempts() {
    StorageManager storage;
    storage.begin();

    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    pollDeferredUntil(
        manager,
        [&manager]() { return !manager.isRtcDetecting(); },
        40U);

    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());

    seedPcf8523WithFreshValidTime();
    setMillis(60000);
    const auto after_exhaustion = manager.poll(getMillis());
    TEST_ASSERT_FALSE(after_exhaustion.state_changed);
    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
}

void test_time_manager_deferred_poll_performs_at_most_one_i2c_transaction() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523ThatLooksLikeWeakDs3231();
    pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });

    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_ambiguous_pcf_fallback_requires_successful_read() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());

    seedPcf8523ThatLooksLikeWeakDs3231();
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "DS3231") == 0; });
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "PCF8523") == 0; });

    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_SECONDS,
                            true);
    TimeManager::PollResult failed_attempt;
    for (uint8_t step = 0; step < 12U && !failed_attempt.state_changed; ++step) {
        failed_attempt = pollDeferredChecked(manager);
    }

    TEST_ASSERT_TRUE(failed_attempt.state_changed);
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
    TEST_ASSERT_EQUAL_STRING("PCF8523", manager.rtcLabel());
}

void test_time_manager_pauses_all_rtc_i2c_when_bus_is_unavailable() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    seedPcf8523WithFreshValidTime();

    setMillis(1000U);
    const uint32_t before = I2cMock::transactionCount();
    for (uint8_t step = 0; step < 5U; ++step) {
        advanceMillis(1000U);
        manager.poll(getMillis(), false);
    }
    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(manager.isRtcValid());
}

void test_time_manager_keeps_ntp_polling_when_rtc_i2c_is_unavailable() {
    seedPcf8523WithOldValidTime();
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcInitialized());

    TEST_ASSERT_TRUE(manager.updateWifiState(true, true));
    constexpr time_t trusted_epoch = 1776256496;
    setNowEpoch(trusted_epoch);
    SntpMock::setSyncStatus(SNTP_SYNC_STATUS_COMPLETED);

    const uint32_t before = I2cMock::transactionCount();
    const auto result = manager.poll(getMillis(), false);
    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
    TEST_ASSERT_TRUE(result.state_changed);
    TEST_ASSERT_TRUE(result.time_updated);
    TEST_ASSERT_FALSE(manager.isNtpSyncing());
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
    TEST_ASSERT_EQUAL_INT64(static_cast<long long>(trusted_epoch),
                            static_cast<long long>(mockNow()));

    const uint32_t before_retry = I2cMock::transactionCount();
    manager.poll(getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(before_retry + 1U, I2cMock::transactionCount());
    TEST_ASSERT_EQUAL_UINT8(
        toBcd(26),
        I2cMock::getRegister(
            Config::PCF8523_ADDR,
            static_cast<uint8_t>(Config::PCF8523_REG_SECONDS + 6U)));
}

void test_time_manager_permanent_runtime_gate_blocks_every_public_rtc_path() {
    seedPcf8523WithFreshValidTime();
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_TRUE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcInitialized());

    manager.disableSharedI2cRuntime();
    TEST_ASSERT_FALSE(manager.isSharedI2cRuntimeAvailable());
    TEST_ASSERT_FALSE(manager.isRtcDetecting());
    TEST_ASSERT_TRUE(manager.finalizeSharedI2cRuntimeDisable(0U));

    const uint32_t before = I2cMock::transactionCount();
    TEST_ASSERT_FALSE(manager.initRtc());

    setNowEpoch(0);
    setMillis(Config::RTC_RESTORE_INTERVAL_MS + 1U);
    tm local_tm = {};
    TEST_ASSERT_FALSE(manager.getLocalTime(local_tm));

    TEST_ASSERT_TRUE(manager.setLocalTime(2026, 8, 12, 10, 30));
    TEST_ASSERT_TRUE(manager.isSystemTimeValid());

    (void)manager.isRtcPresent();
    (void)manager.isRtcInitialized();
    (void)manager.isRtcValid();
    (void)manager.isRtcLostPower();
    (void)manager.isRtcTimeUnset();
    (void)manager.isRtcReadFault();
    (void)manager.isRtcBatteryLow();
    (void)manager.rtcLabel();
    manager.poll(getMillis() + Config::RTC_STATUS_POLL_MS, true);

    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
}

void test_time_manager_permanent_runtime_gate_stops_deferred_detection() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    seedPcf8523WithFreshValidTime();
    manager.disableSharedI2cRuntime();
    TEST_ASSERT_TRUE(manager.finalizeSharedI2cRuntimeDisable(0U));
    const uint32_t before = I2cMock::transactionCount();
    for (uint8_t step = 0; step < 8U; ++step) {
        advanceMillis(1000U);
        manager.poll(getMillis(), true);
    }

    TEST_ASSERT_FALSE(manager.isRtcDetecting());
    TEST_ASSERT_FALSE(manager.isRtcPresent());
    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
}

void test_time_manager_permanent_runtime_gate_keeps_ntp_without_rtc_write() {
    seedPcf8523WithOldValidTime();
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    TEST_ASSERT_TRUE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.updateWifiState(true, true));

    manager.disableSharedI2cRuntime();
    constexpr time_t trusted_epoch = 1776256496;
    setNowEpoch(trusted_epoch);
    SntpMock::setSyncStatus(SNTP_SYNC_STATUS_COMPLETED);

    const uint32_t before = I2cMock::transactionCount();
    const TimeManager::PollResult result = manager.poll(getMillis(), true);

    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
    TEST_ASSERT_TRUE(result.state_changed);
    TEST_ASSERT_TRUE(result.time_updated);
    TEST_ASSERT_FALSE(manager.isNtpSyncing());
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
    TEST_ASSERT_EQUAL_INT64(static_cast<long long>(trusted_epoch),
                            static_cast<long long>(mockNow()));
}

void test_time_manager_begin_resets_runtime_gate_for_new_boot_lifecycle() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    manager.disableSharedI2cRuntime();
    TEST_ASSERT_FALSE(manager.isSharedI2cRuntimeAvailable());

    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.isSharedI2cRuntimeAvailable());
}

void test_time_manager_finalize_closes_gate_without_prior_disable() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);

    TEST_ASSERT_TRUE(manager.finalizeSharedI2cRuntimeDisable(0U));
    TEST_ASSERT_FALSE(manager.isSharedI2cRuntimeAvailable());

    seedPcf8523WithFreshValidTime();
    const uint32_t before = I2cMock::transactionCount();
    TEST_ASSERT_FALSE(manager.initRtc());
    manager.poll(getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(before, I2cMock::transactionCount());
}

void test_time_manager_ntp_completion_defers_rtc_write_during_detection() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    seedPcf8523WithOldValidTime();

    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "PCF8523") == 0; });
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());

    TEST_ASSERT_TRUE(manager.updateWifiState(true, true));
    constexpr time_t trusted_epoch = 1776256496;
    setNowEpoch(trusted_epoch);
    SntpMock::setSyncStatus(SNTP_SYNC_STATUS_COMPLETED);

    const uint32_t before = I2cMock::transactionCount();
    const auto ntp_completed = manager.poll(getMillis());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        1U,
        I2cMock::transactionCount() - before);
    TEST_ASSERT_TRUE(ntp_completed.time_updated);
    TEST_ASSERT_FALSE(manager.isNtpSyncing());
    TEST_ASSERT_TRUE(manager.isSystemTimeTrusted());
    TEST_ASSERT_FALSE(manager.isRtcInitialized());

    pollDeferredUntil(
        manager,
        [&manager]() { return manager.isRtcInitialized(); });
    TEST_ASSERT_TRUE(manager.isRtcValid());
    TEST_ASSERT_EQUAL_INT64(static_cast<long long>(trusted_epoch),
                            static_cast<long long>(mockNow()));
}

void test_time_manager_ds_status_failure_does_not_commit_partial_calendar() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    seedPcf8523ThatLooksLikeWeakDs3231();

    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "DS3231") == 0; });
    pollDeferredChecked(manager);  // DS3231 begin (no I2C).
    pollDeferredChecked(manager);  // First, invalid calendar read.
    pollDeferredChecked(manager);  // First status read succeeds.

    seedDs3231WithFreshValidTime();
    I2cMock::setReadFailure(Config::DS3231_ADDR,
                            Config::DS3231_REG_STATUS,
                            true);
    pollDeferredChecked(manager);  // Second calendar read succeeds.
    pollDeferredChecked(manager);  // Second status read fails.
    pollDeferredChecked(manager);  // Third calendar read succeeds.
    pollDeferredChecked(manager);  // Third status read fails.

    seedPcf8523ThatLooksLikeWeakDs3231();
    pollDeferredUntil(
        manager,
        [&manager]() { return strcmp(manager.rtcLabel(), "PCF8523") == 0; },
        20U);
    TEST_ASSERT_FALSE(manager.isRtcInitialized());
    TEST_ASSERT_TRUE(manager.isRtcDetecting());
}

void test_time_manager_deferred_retry_deadline_uses_post_i2c_millis() {
    StorageManager storage;
    storage.begin();
    TimeManager manager;
    manager.begin(storage);
    TEST_ASSERT_FALSE(manager.initRtc());
    seedPcf8523WithFreshValidTime();

    setMillis(1000U);
    pollDeferredChecked(manager, 0U);  // Start the deferred attempt.
    pollDeferredChecked(manager, 1U);  // Explicit PCF8523 signature.
    pollDeferredChecked(manager, 1U);  // PCF8523 begin write.

    I2cMock::setReadFailure(Config::PCF8523_ADDR,
                            Config::PCF8523_REG_SECONDS,
                            true);
    I2cMock::setCommandAdvanceMs(100U);
    advanceMillis(500U);
    const uint32_t before_read = I2cMock::transactionCount();
    manager.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(before_read + 1U, I2cMock::transactionCount());

    const uint32_t after_read_ms = getMillis();
    setMillis(after_read_ms + Config::RTC_INIT_RETRY_MS - 1U);
    const uint32_t before_early_poll = I2cMock::transactionCount();
    manager.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(before_early_poll, I2cMock::transactionCount());

    advanceMillis(1U);
    manager.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(before_early_poll + 1U, I2cMock::transactionCount());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_time_manager_init_rtc_handles_absent_rtc);
    RUN_TEST(test_time_manager_plausible_process_epoch_is_not_boot_trusted);
    RUN_TEST(test_time_manager_validates_manual_calendar_before_trusting);
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
    RUN_TEST(test_time_manager_poll_detects_rtc_that_appears_during_startup_window);
    RUN_TEST(test_time_manager_retries_begin_failure_then_initializes_without_blocking_poll);
    RUN_TEST(test_time_manager_sync_unresolved_weak_ds_keeps_bounded_probe_active);
    RUN_TEST(test_time_manager_deferred_weak_ds_switches_to_pcf_before_success);
    RUN_TEST(test_time_manager_deferred_unresolved_weak_ds_records_failure_and_retries);
    RUN_TEST(test_time_manager_deferred_all_read_failures_recover_in_runtime_poll);
    RUN_TEST(test_time_manager_deferred_rtc_deadline_handles_millis_wraparound);
    RUN_TEST(test_time_manager_plausible_process_epoch_does_not_seed_deferred_rtc);
    RUN_TEST(test_time_manager_stops_missing_rtc_detection_after_five_attempts);
    RUN_TEST(test_time_manager_deferred_poll_performs_at_most_one_i2c_transaction);
    RUN_TEST(test_time_manager_ambiguous_pcf_fallback_requires_successful_read);
    RUN_TEST(test_time_manager_pauses_all_rtc_i2c_when_bus_is_unavailable);
    RUN_TEST(test_time_manager_keeps_ntp_polling_when_rtc_i2c_is_unavailable);
    RUN_TEST(test_time_manager_permanent_runtime_gate_blocks_every_public_rtc_path);
    RUN_TEST(test_time_manager_permanent_runtime_gate_stops_deferred_detection);
    RUN_TEST(test_time_manager_permanent_runtime_gate_keeps_ntp_without_rtc_write);
    RUN_TEST(test_time_manager_begin_resets_runtime_gate_for_new_boot_lifecycle);
    RUN_TEST(test_time_manager_finalize_closes_gate_without_prior_disable);
    RUN_TEST(test_time_manager_ntp_completion_defers_rtc_write_during_detection);
    RUN_TEST(test_time_manager_ds_status_failure_does_not_commit_partial_calendar);
    RUN_TEST(test_time_manager_deferred_retry_deadline_uses_post_i2c_millis);
    return UNITY_END();
}
