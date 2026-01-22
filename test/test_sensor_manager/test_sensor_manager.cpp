#include <unity.h>

#include "ArduinoMock.h"
#include "TimeMock.h"
#include "config/AppConfig.h"
#include "modules/PressureHistory.h"
#include "modules/SensorManager.h"
#include "modules/StorageManager.h"
#include "drivers/Bmp580.h"
#include "drivers/Dps310.h"
#include "drivers/Sen66.h"
#include "drivers/Sfa3x.h"

static void resetDriverStates() {
    Bmp580::state() = Bmp580TestState();
    Sen66::state() = Sen66TestState();
    Dps310::state() = Dps310TestState();
    Sfa3x::state() = Sfa3xTestState();
}

void setUp() {
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    PressureHistory::setNowEpochFn(&mockNow);
    resetDriverStates();
}

void tearDown() {
    PressureHistory::setNowEpochFn(nullptr);
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
    manager.begin(storage, 0.0f, 0.0f);

    auto &sen = Sen66::state();
    sen.provide_data = true;
    sen.poll_changed = true;
    sen.update_last_data_on_poll = true;
    sen.poll_data.temp_valid = true;
    sen.poll_data.temperature = 21.5f;
    sen.poll_data.hum_valid = true;
    sen.poll_data.humidity = 40.0f;

    auto &sfa = Sfa3x::state();
    sfa.has_new_data = true;
    sfa.hcho_ppb = 12.3f;

    auto &dps = Dps310::state();
    dps.has_new_data = true;
    dps.pressure = 1012.5f;
    dps.temperature = 23.1f;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
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

void test_sensor_manager_stale_resets_data() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorManager manager;
    SensorData data;

    data.temp_valid = true;
    data.hum_valid = true;
    data.co2_valid = true;
    data.co2 = 500;
    data.pressure_valid = true;
    data.pressure = 1000.0f;

    setMillis(10000);
    auto &sen = Sen66::state();
    sen.last_data_ms = getMillis() - (Config::SEN66_STALE_MS + 1);
    sen.update_last_data_on_poll = false;

    SensorManager::PollResult result =
        manager.poll(data, storage, history, true);

    TEST_ASSERT_TRUE(result.data_changed);
    TEST_ASSERT_FALSE(data.temp_valid);
    TEST_ASSERT_FALSE(data.hum_valid);
    TEST_ASSERT_FALSE(data.co2_valid);
    TEST_ASSERT_FALSE(data.pressure_valid);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_manager_poll_updates_data);
    RUN_TEST(test_sensor_manager_warmup_change);
    RUN_TEST(test_sensor_manager_stale_resets_data);
    return UNITY_END();
}
