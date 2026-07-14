#include <unity.h>
#include <cstring>

#include "ArduinoMock.h"
#include "TimeMock.h"
#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "modules/PressureHistory.h"
#include "modules/SensorManager.h"
#include "modules/StorageManager.h"
#include "drivers/Bmp3xx.h"
#include "drivers/Bmp580.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "drivers/Dps310.h"
#include "drivers/Sen0466.h"
#include "drivers/Sen66.h"
#include "drivers/Sfa30.h"
#include "drivers/Sfa40.h"

static void resetDriverStates() {
    Bmp3xx::state() = Bmp3xxTestState();
    Bmp3xx::variant_state() = Bmp3xx::Variant::BMP390;
    Bmp580::state() = Bmp580TestState();
    Bmp580::variant_state() = Bmp580::Variant::BMP580_581;
    Sen66::state() = Sen66TestState();
    Dps310::state() = Dps310TestState();
    Sfa30::state() = Sfa30TestState();
    Sfa40::state() = Sfa40TestState();
    Sen0466::state() = Sen0466TestState();
    DfrOptionalGasSensor::state() = DfrOptionalGasSensorTestState();
}

void setUp() {
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    PressureHistory::setNowEpochFn(&mockNow);
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
    Logger::resetRecentForTest();
    boot_reset_reason = ESP_RST_POWERON;
    resetDriverStates();
}

void tearDown() {
    Logger::resetRecentForTest();
    PressureHistory::setNowEpochFn(nullptr);
}

static bool recentContainsMessagePrefix(const char *prefix) {
    Logger::RecentEntry recent[16];
    const size_t count = Logger::copyRecent(recent, sizeof(recent) / sizeof(recent[0]));
    for (size_t i = 0; i < count; ++i) {
        if (strncmp(recent[i].message, prefix, strlen(prefix)) == 0) {
            return true;
        }
    }
    return false;
}

void test_sensor_manager_poll_updates_data() {
    setMillis(Config::PRESSURE_HISTORY_STEP_MS);

    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &bmp = Bmp580::state();
    bmp.start_ok = false;
    auto &bmp3 = Bmp3xx::state();
    bmp3.start_ok = false;
    manager.begin(storage, 0.0f, 0.0f);

    auto &sen = Sen66::state();
    sen.provide_data = true;
    sen.poll_changed = true;
    sen.update_last_data_on_poll = true;
    sen.poll_data.temp_valid = true;
    sen.poll_data.temperature = 21.5f;
    sen.poll_data.hum_valid = true;
    sen.poll_data.humidity = 40.0f;

    auto &sfa = Sfa40::state();
    sfa.has_new_data = true;
    sfa.hcho_ppb = 12.3f;

    auto &dps = Dps310::state();
    dps.has_new_data = true;
    dps.pressure = 1012.5f;
    dps.temperature = 23.1f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.hcho_sensor_present);
    TEST_ASSERT_FALSE(data.hcho_warmup);
    TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.3f, data.hcho);
    TEST_ASSERT_TRUE(data.pressure_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1012.5f, data.pressure);
    TEST_ASSERT_TRUE(Sen66::state().update_pressure_called);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1012.5f, Sen66::state().last_pressure);
}

void test_sensor_manager_warmup_change() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;
    manager.begin(storage, 0.0f, 0.0f);

    auto &sen = Sen66::state();
    sen.warmup = false;
    SensorManager::PollResult first =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_FALSE(first.warmup_changed);

    sen.warmup = true;
    SensorManager::PollResult second =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(second.warmup_changed);
}

void test_sensor_manager_stale_preserves_other_sensor_data() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;
    Bmp580::state().start_ok = false;
    Bmp3xx::state().start_ok = false;
    Dps310::state().start_ok = false;
    manager.begin(storage, 0.0f, 0.0f);

    data.temp_valid = true;
    data.hum_valid = true;
    data.co2_valid = true;
    data.co2 = 500;
    data.voc_valid = true;
    data.voc_index = 123;
    data.nox_valid = true;
    data.nox_index = 87;
    data.pm1_valid = true;
    data.pm1 = 4.2f;
    data.pressure_valid = true;
    data.pressure = 1000.0f;
    data.hcho_valid = true;
    data.hcho = 8.5f;
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_warmup = false;
    data.co_ppm = 2.3f;

    setMillis(10000);
    auto &sen = Sen66::state();
    sen.last_data_ms = getMillis() - (Config::SEN66_STALE_MS + 1);
    sen.update_last_data_on_poll = false;
    auto &co = Sen0466::state();
    co.present = true;
    co.data_valid = true;
    co.warmup = false;
    co.co_ppm = 2.3f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_FALSE(data.temp_valid);
    TEST_ASSERT_FALSE(data.hum_valid);
    TEST_ASSERT_FALSE(data.co2_valid);
    TEST_ASSERT_FALSE(data.voc_valid);
    TEST_ASSERT_FALSE(data.nox_valid);
    TEST_ASSERT_FALSE(data.pm1_valid);
    TEST_ASSERT_FALSE(data.pm_valid);
    TEST_ASSERT_TRUE(data.pressure_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, data.pressure);
    TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.5f, data.hcho);
    TEST_ASSERT_TRUE(data.co_sensor_present);
    TEST_ASSERT_TRUE(data.co_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.3f, data.co_ppm);
}

void test_sensor_manager_before_begin_suppresses_poll_and_public_operations() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data{};
    uint16_t correction = 123;

    const SensorManager::PollResult result = manager.poll(data, storage, history, true);

    TEST_ASSERT_FALSE(result.data_changed);
    TEST_ASSERT_FALSE(result.warmup_changed);
    TEST_ASSERT_FALSE(Sen66::state().save_voc_called);
    TEST_ASSERT_FALSE(Sen66::state().update_pressure_called);
    TEST_ASSERT_FALSE(manager.deviceReset());
    TEST_ASSERT_FALSE(manager.start(true));
    TEST_ASSERT_FALSE(manager.setAscEnabled(true));
    TEST_ASSERT_FALSE(manager.calibrateFrc(400, false, 0.0f, correction));
    manager.scheduleRetry(1000);
    TEST_ASSERT_EQUAL_UINT32(0, Sen66::state().retry_at_ms);
    TEST_ASSERT_FALSE(Sen66::state().device_reset_called);
    TEST_ASSERT_FALSE(Sen66::state().start_called);
}

void test_sensor_manager_pm05_clamps_to_sensor_limit() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    manager.begin(storage, 0.0f, 0.0f);

    data.pm05_valid = true;
    data.pm05 = Config::SEN66_PM_NUM_MAX_PPCM3 + 500.0f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.pm05_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SEN66_PM_NUM_MAX_PPCM3, data.pm05);
}

void test_sensor_manager_pm1_invalid_resets_stale_value() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    manager.begin(storage, 0.0f, 0.0f);

    data.pm1_valid = false;
    data.pm1 = 17.5f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_FALSE(data.pm1_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.pm1);
}

void test_sensor_manager_without_co_sensor_keeps_pm1_and_clears_co() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &co = Sen0466::state();
    co.start_ok = false;

    manager.begin(storage, 0.0f, 0.0f);

    data.pm1_valid = true;
    data.pm1 = 6.0f;
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_warmup = true;
    data.co_ppm = 3.5f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_FALSE(data.co_sensor_present);
    TEST_ASSERT_FALSE(data.co_valid);
    TEST_ASSERT_FALSE(data.co_warmup);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.co_ppm);
    TEST_ASSERT_TRUE(data.pm1_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, data.pm1);
}

void test_sensor_manager_optional_gas_nh3_updates_generic_and_legacy_fields() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &optional_gas = DfrOptionalGasSensor::state();
    optional_gas.start_ok = true;
    optional_gas.present = true;
    optional_gas.data_valid = true;
    optional_gas.warmup = false;
    optional_gas.ppm = 12.5f;
    optional_gas.gas_type = DfrOptionalGasSensor::OptionalGasType::NH3;

    manager.begin(storage, 0.0f, 0.0f);

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.optional_gas_sensor_present);
    TEST_ASSERT_TRUE(data.optional_gas_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, data.optional_gas_ppm);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      data.optional_gas_type);
    TEST_ASSERT_TRUE(data.nh3_sensor_present);
    TEST_ASSERT_TRUE(data.nh3_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, data.nh3_ppm);
}

void test_sensor_manager_optional_gas_so2_does_not_populate_nh3_compat_fields() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &optional_gas = DfrOptionalGasSensor::state();
    optional_gas.start_ok = true;
    optional_gas.present = true;
    optional_gas.data_valid = true;
    optional_gas.warmup = false;
    optional_gas.ppm = 7.5f;
    optional_gas.gas_type = DfrOptionalGasSensor::OptionalGasType::SO2;

    manager.begin(storage, 0.0f, 0.0f);

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.optional_gas_sensor_present);
    TEST_ASSERT_TRUE(data.optional_gas_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, data.optional_gas_ppm);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::SO2),
                      data.optional_gas_type);
    TEST_ASSERT_FALSE(data.nh3_sensor_present);
    TEST_ASSERT_FALSE(data.nh3_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.nh3_ppm);
}

void test_sensor_manager_optional_gas_h2s_updates_generic_without_nh3_compat_fields() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &optional_gas = DfrOptionalGasSensor::state();
    optional_gas.start_ok = true;
    optional_gas.present = true;
    optional_gas.data_valid = true;
    optional_gas.warmup = false;
    optional_gas.ppm = 18.0f;
    optional_gas.gas_type = DfrOptionalGasSensor::OptionalGasType::H2S;

    manager.begin(storage, 0.0f, 0.0f);

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.optional_gas_sensor_present);
    TEST_ASSERT_TRUE(data.optional_gas_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.0f, data.optional_gas_ppm);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::H2S),
                      data.optional_gas_type);
    TEST_ASSERT_FALSE(data.nh3_sensor_present);
    TEST_ASSERT_FALSE(data.nh3_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.nh3_ppm);
}

void test_sensor_manager_optional_gas_o3_updates_generic_without_nh3_compat_fields() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &optional_gas = DfrOptionalGasSensor::state();
    optional_gas.start_ok = true;
    optional_gas.present = true;
    optional_gas.data_valid = true;
    optional_gas.warmup = false;
    optional_gas.ppm = 0.23f;
    optional_gas.ppm_decimals = 2;
    optional_gas.gas_type = DfrOptionalGasSensor::OptionalGasType::O3;

    manager.begin(storage, 0.0f, 0.0f);

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.optional_gas_sensor_present);
    TEST_ASSERT_TRUE(data.optional_gas_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.23f, data.optional_gas_ppm);
    TEST_ASSERT_EQUAL_UINT8(2, data.optional_gas_ppm_decimals);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O3),
                      data.optional_gas_type);
    TEST_ASSERT_FALSE(data.nh3_sensor_present);
    TEST_ASSERT_FALSE(data.nh3_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data.nh3_ppm);
}

void test_sensor_manager_sfa_absent_is_not_fault() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Absent;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_FALSE(manager.isSfaPresent());
    TEST_ASSERT_FALSE(manager.isSfaOk());
    TEST_ASSERT_FALSE(manager.hasSfaFault());

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_FALSE(result.data_changed);
}

void test_sensor_manager_falls_back_to_sfa30_when_sfa40_probe_is_rejected() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa40 = Sfa40::state();
    sfa40.status = Sfa40::Status::Fault;
    sfa40.fallback_to_sfa30 = true;

    auto &sfa30 = Sfa30::state();
    sfa30.status = Sfa30::Status::Ok;
    sfa30.has_new_data = true;
    sfa30.hcho_ppb = 14.2f;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_TRUE(manager.isSfaPresent());
    TEST_ASSERT_TRUE(manager.isSfaOk());
    TEST_ASSERT_FALSE(manager.hasSfaFault());
    TEST_ASSERT_FALSE(manager.isSfaWarmupActive());

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f, data.hcho);
}

void test_sensor_manager_warm_restart_prefers_confirmed_sfa30_before_sfa40() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    boot_reset_reason = ESP_RST_SW;

    auto &sfa30 = Sfa30::state();
    sfa30.probe_ok = true;
    sfa30.status = Sfa30::Status::Ok;

    auto &sfa40 = Sfa40::state();
    sfa40.status = Sfa40::Status::Ok;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_TRUE(sfa30.probe_called);
    TEST_ASSERT_TRUE(sfa30.start_called);
    TEST_ASSERT_FALSE(sfa40.start_called);
    TEST_ASSERT_TRUE(manager.isSfaPresent());
    TEST_ASSERT_TRUE(manager.isSfaOk());
}

void test_sensor_manager_warm_restart_tries_sfa40_when_sfa30_probe_does_not_confirm() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    boot_reset_reason = ESP_RST_SW;

    auto &sfa30 = Sfa30::state();
    sfa30.probe_ok = false;
    sfa30.status = Sfa30::Status::Fault;

    auto &sfa40 = Sfa40::state();
    sfa40.status = Sfa40::Status::Ok;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_TRUE(sfa30.probe_called);
    TEST_ASSERT_FALSE(sfa30.start_called);
    TEST_ASSERT_TRUE(sfa40.start_called);
    TEST_ASSERT_TRUE(manager.isSfaPresent());
    TEST_ASSERT_TRUE(manager.isSfaOk());
}

void test_sensor_manager_hcho_sensor_label_reports_sfa30_when_active() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    auto &sfa40 = Sfa40::state();
    sfa40.status = Sfa40::Status::Fault;
    sfa40.fallback_to_sfa30 = true;

    auto &sfa30 = Sfa30::state();
    sfa30.status = Sfa30::Status::Ok;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL_STRING("SFA30", manager.hchoSensorLabel());
}

void test_sensor_manager_hcho_sensor_label_reports_sfa40_when_active() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    auto &sfa40 = Sfa40::state();
    sfa40.status = Sfa40::Status::Ok;

    auto &sfa30 = Sfa30::state();
    sfa30.status = Sfa30::Status::Absent;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL_STRING("SFA40", manager.hchoSensorLabel());
}

void test_sensor_manager_sfa30_warmup_keeps_new_hcho_data_invalid() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    boot_reset_reason = ESP_RST_SW;

    auto &sfa30 = Sfa30::state();
    sfa30.probe_ok = true;
    sfa30.status = Sfa30::Status::Ok;
    sfa30.warmup_active = true;

    manager.begin(storage, 0.0f, 0.0f);

    sfa30.has_new_data = true;
    sfa30.hcho_ppb = 0.0f;
    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(manager.isSfaWarmupActive());
    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.hcho_sensor_present);
    TEST_ASSERT_TRUE(data.hcho_warmup);
    TEST_ASSERT_FALSE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, data.hcho);
}

void test_sensor_manager_sfa_fault_is_reported() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Fault;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_TRUE(manager.isSfaPresent());
    TEST_ASSERT_FALSE(manager.isSfaOk());
    TEST_ASSERT_TRUE(manager.hasSfaFault());
}

void test_sensor_manager_sfa_state_change_marks_data_changed() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Ok;
    sfa.warmup_active = false;

    manager.begin(storage, 0.0f, 0.0f);

    SensorManager::PollResult first =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(first.data_changed);
    TEST_ASSERT_TRUE(data.hcho_sensor_present);
    TEST_ASSERT_FALSE(data.hcho_warmup);

    sfa.warmup_active = true;
    SensorManager::PollResult second =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(second.data_changed);
    TEST_ASSERT_TRUE(data.hcho_sensor_present);
    TEST_ASSERT_TRUE(data.hcho_warmup);

    sfa.warmup_active = false;
    sfa.status = Sfa40::Status::Fault;
    SensorManager::PollResult third =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(third.data_changed);
    TEST_ASSERT_TRUE(data.hcho_sensor_present);
    TEST_ASSERT_FALSE(data.hcho_warmup);
}

void test_sensor_manager_sfa_fault_invalidates_previous_hcho_value() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Ok;
    sfa.warmup_active = false;

    manager.begin(storage, 0.0f, 0.0f);

    sfa.has_new_data = true;
    sfa.hcho_ppb = 18.4f;
    SensorManager::PollResult first =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(first.data_changed);
    TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.4f, data.hcho);

    sfa.status = Sfa40::Status::Fault;
    sfa.has_new_data = false;
    SensorManager::PollResult second =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(second.data_changed);
    TEST_ASSERT_FALSE(data.hcho_valid);
    TEST_ASSERT_TRUE(sfa.invalidate_called);
}

void test_sensor_manager_sfa_warmup_keeps_new_hcho_data_invalid() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Ok;
    sfa.warmup_active = true;

    manager.begin(storage, 0.0f, 0.0f);

    sfa.has_new_data = true;
    sfa.hcho_ppb = 22.7f;
    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_FALSE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.7f, data.hcho);
}

void test_sensor_manager_sfa_warmup_invalidates_previous_hcho_value() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    auto &sfa = Sfa40::state();
    sfa.status = Sfa40::Status::Ok;
    sfa.warmup_active = false;

    manager.begin(storage, 0.0f, 0.0f);

    sfa.has_new_data = true;
    sfa.hcho_ppb = 19.1f;
    SensorManager::PollResult first =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(first.data_changed);
    TEST_ASSERT_TRUE(data.hcho_valid);

    sfa.warmup_active = true;
    sfa.has_new_data = false;
    SensorManager::PollResult second =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(second.data_changed);
    TEST_ASSERT_FALSE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 19.1f, data.hcho);
}

void test_sensor_manager_clamps_hcho_to_sfa40_max_range() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    manager.begin(storage, 0.0f, 0.0f);

    data.hcho_valid = true;
    data.hcho = Config::SFA40_HCHO_MAX_PPB + 500.0f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SFA40_HCHO_MAX_PPB, data.hcho);
}

void test_sensor_manager_bmp58x_label_reports_bmp580_581_family() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    Bmp580::variant_state() = Bmp580::Variant::BMP580_581;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL(SensorManager::PRESSURE_BMP58X, manager.pressureSensorType());
    TEST_ASSERT_EQUAL_STRING("BMP580/581:", manager.pressureSensorLabel());
}

void test_sensor_manager_bmp58x_label_reports_bmp585() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    Bmp580::variant_state() = Bmp580::Variant::BMP585;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL(SensorManager::PRESSURE_BMP58X, manager.pressureSensorType());
    TEST_ASSERT_EQUAL_STRING("BMP585:", manager.pressureSensorLabel());
}

void test_sensor_manager_bmp3xx_label_reports_bmp388() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    Bmp580::state().start_ok = false;
    Bmp3xx::variant_state() = Bmp3xx::Variant::BMP388;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL(SensorManager::PRESSURE_BMP3XX, manager.pressureSensorType());
    TEST_ASSERT_EQUAL_STRING("BMP388:", manager.pressureSensorLabel());
}

void test_sensor_manager_bmp3xx_label_reports_bmp390() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    Bmp580::state().start_ok = false;
    Bmp3xx::variant_state() = Bmp3xx::Variant::BMP390;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL(SensorManager::PRESSURE_BMP3XX, manager.pressureSensorType());
    TEST_ASSERT_EQUAL_STRING("BMP390:", manager.pressureSensorLabel());
}

void test_sensor_manager_falls_back_to_dps310_after_bmp_families_fail() {
    StorageManager storage;
    storage.begin();
    SensorManager manager;

    Bmp580::state().start_ok = false;
    Bmp3xx::state().start_ok = false;
    Dps310::state().start_ok = true;

    manager.begin(storage, 0.0f, 0.0f);

    TEST_ASSERT_EQUAL(SensorManager::PRESSURE_DPS310, manager.pressureSensorType());
    TEST_ASSERT_EQUAL_STRING("DPS310:", manager.pressureSensorLabel());
}

void test_sensor_manager_stale_resets_temp_warning_state() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    manager.begin(storage, 0.0f, 0.0f);
    Logger::resetRecentForTest();

    auto &sen = Sen66::state();
    sen.provide_data = true;
    sen.poll_changed = true;
    sen.update_last_data_on_poll = true;
    sen.poll_data = SensorData{};
    sen.poll_data.temp_valid = true;
    sen.poll_data.temperature = 45.0f;

    setMillis(Config::SEN66_POLL_MS);
    SensorManager::PollResult first =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(first.data_changed);
    TEST_ASSERT_TRUE(recentContainsMessagePrefix("Temperature outside recommended range:"));

    Logger::resetRecentForTest();
    sen.provide_data = false;
    sen.poll_changed = false;
    sen.update_last_data_on_poll = false;
    setMillis(sen.last_data_ms + Config::SEN66_STALE_MS + 1);
    SensorManager::PollResult stale =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(stale.data_changed);
    TEST_ASSERT_FALSE(data.temp_valid);

    Logger::resetRecentForTest();
    sen.provide_data = true;
    sen.poll_changed = true;
    sen.update_last_data_on_poll = true;
    sen.poll_data = SensorData{};
    sen.poll_data.temp_valid = true;
    sen.poll_data.temperature = 45.0f;
    setMillis(getMillis() + Config::SEN66_POLL_MS);
    SensorManager::PollResult second =
        manager.poll(data, storage, history, true);
    TEST_ASSERT_TRUE(second.data_changed);
    TEST_ASSERT_TRUE(recentContainsMessagePrefix("Temperature outside recommended range:"));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_manager_poll_updates_data);
    RUN_TEST(test_sensor_manager_warmup_change);
    RUN_TEST(test_sensor_manager_stale_preserves_other_sensor_data);
    RUN_TEST(test_sensor_manager_before_begin_suppresses_poll_and_public_operations);
    RUN_TEST(test_sensor_manager_pm05_clamps_to_sensor_limit);
    RUN_TEST(test_sensor_manager_pm1_invalid_resets_stale_value);
    RUN_TEST(test_sensor_manager_without_co_sensor_keeps_pm1_and_clears_co);
    RUN_TEST(test_sensor_manager_optional_gas_nh3_updates_generic_and_legacy_fields);
    RUN_TEST(test_sensor_manager_optional_gas_so2_does_not_populate_nh3_compat_fields);
    RUN_TEST(test_sensor_manager_optional_gas_h2s_updates_generic_without_nh3_compat_fields);
    RUN_TEST(test_sensor_manager_optional_gas_o3_updates_generic_without_nh3_compat_fields);
    RUN_TEST(test_sensor_manager_sfa_absent_is_not_fault);
    RUN_TEST(test_sensor_manager_falls_back_to_sfa30_when_sfa40_probe_is_rejected);
    RUN_TEST(test_sensor_manager_warm_restart_prefers_confirmed_sfa30_before_sfa40);
    RUN_TEST(test_sensor_manager_warm_restart_tries_sfa40_when_sfa30_probe_does_not_confirm);
    RUN_TEST(test_sensor_manager_hcho_sensor_label_reports_sfa30_when_active);
    RUN_TEST(test_sensor_manager_hcho_sensor_label_reports_sfa40_when_active);
    RUN_TEST(test_sensor_manager_sfa30_warmup_keeps_new_hcho_data_invalid);
    RUN_TEST(test_sensor_manager_sfa_fault_is_reported);
    RUN_TEST(test_sensor_manager_sfa_state_change_marks_data_changed);
    RUN_TEST(test_sensor_manager_sfa_fault_invalidates_previous_hcho_value);
    RUN_TEST(test_sensor_manager_sfa_warmup_keeps_new_hcho_data_invalid);
    RUN_TEST(test_sensor_manager_sfa_warmup_invalidates_previous_hcho_value);
    RUN_TEST(test_sensor_manager_clamps_hcho_to_sfa40_max_range);
    RUN_TEST(test_sensor_manager_bmp58x_label_reports_bmp580_581_family);
    RUN_TEST(test_sensor_manager_bmp58x_label_reports_bmp585);
    RUN_TEST(test_sensor_manager_bmp3xx_label_reports_bmp388);
    RUN_TEST(test_sensor_manager_bmp3xx_label_reports_bmp390);
    RUN_TEST(test_sensor_manager_falls_back_to_dps310_after_bmp_families_fail);
    RUN_TEST(test_sensor_manager_stale_resets_temp_warning_state);
    return UNITY_END();
}
