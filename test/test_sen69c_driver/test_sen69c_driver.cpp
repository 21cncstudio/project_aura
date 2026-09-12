#include <unity.h>
#include <cstring>
#include "ArduinoMock.h"
#include "I2cMock.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "modules/StorageManager.h"

#define private public
#define Sen6x RealSen6x
#include "../../src/drivers/Sen6x.h"
#undef private
#include "../../src/core/I2CHelper.cpp"
#include "../../src/drivers/Sen69c.cpp"
#include "../../src/drivers/Sen6x.cpp"
#undef Sen6x

namespace {
void response(uint16_t command, const uint16_t *words, size_t count, bool corrupt = false) {
    uint8_t bytes[48]{};
    for (size_t i=0; i<count; ++i) {
        bytes[3*i] = words[i]>>8; bytes[3*i+1] = words[i];
        bytes[3*i+2] = I2C::crc8(bytes+3*i,2);
    }
    if (corrupt) bytes[count*3-1] ^= 1;
    I2cMock::setCommandRead(0x6B,command,bytes,count*3);
}
void word(uint16_t cmd, uint16_t value) { response(cmd,&value,1); }
void identity(const char *model = "SEN69C", bool corrupt = false) {
    uint16_t words[16]{};
    char text[32]{}; strncpy(text,model,31);
    for (size_t i=0;i<16;++i) words[i]=(uint16_t(uint8_t(text[2*i]))<<8)|uint8_t(text[2*i+1]);
    response(0xD014,words,16,corrupt);
    memset(words,0,sizeof(words)); words[0]=0x4142; words[1]=0x4344;
    response(0xD033,words,16);
}
void start(Sen69c &sensor) {
    sensor.begin();
    word(0x6711,1);
    sensor.beginLateStart(true);
    for (unsigned i=0;i<300 && sensor.isBusy();++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_TRUE(sensor.isOk());
}
void frame(uint16_t hcho=432,uint16_t co2=987,bool corrupt=false) {
    const uint16_t values[] = {12,34,56,78,4567,5000,1230,40,hcho,co2};
    const uint16_t status[] = {0,0};
    const uint16_t number[] = {123,234,345,456,567};
    response(0x04B5,values,10,corrupt); response(0xD206,status,2);
    response(0xD210,status,2);
    response(0x0316,number,5); word(0x0202,1);
}
void pollFrame(Sen69c &sensor, SensorData &data) {
    for (unsigned i=0;i<10;++i) {
        bool changed; sensor.poll(data,changed); advanceMillis(20);
        if (sensor.poll_phase_ == Sen69c::PollPhase::Idle) break;
    }
}
}

void setUp() {
    setMillis(100);
    I2cMock::reset(); I2cMock::setDevicePresent(0x6B,true);
    Sen66::state() = Sen66TestState();
    boot_peripherals_cold_start = true;
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    StorageManager::resetTestPersistence();
}
void tearDown() {}

void test_model_probe_waits_and_selects_only_sen69c() {
    identity(); word(0x6711,1);
    RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    sensor.pollLateStart(millis());
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0xD304));
    TEST_ASSERT_EQUAL(0,Sen66::state().late_start_begin_count);
    advanceMillis(19); sensor.pollLateStart(millis());
    TEST_ASSERT_EQUAL(int(RealSen6x::Model::None),int(sensor.model()));
    advanceMillis(1); sensor.pollLateStart(millis());
    TEST_ASSERT_TRUE(sensor.isSen69c());
    for(unsigned i=0;i<300 && sensor.isBusy();++i) { sensor.pollLateStart(millis()); advanceMillis(20); }
    TEST_ASSERT_TRUE(sensor.isOk());
    TEST_ASSERT_EQUAL_STRING("ABCD",sensor.serial());
    TEST_ASSERT_EQUAL(0,Sen66::state().late_start_begin_count);
}
void test_model_probe_routes_sen66_to_existing_driver() {
    identity("SEN66"); RealSen6x sensor; sensor.begin(); sensor.beginLateStart(false);
    for(unsigned i=0;i<20 && sensor.isBusy();++i) { sensor.pollLateStart(millis()); advanceMillis(20); }
    TEST_ASSERT_TRUE(sensor.isOk()); TEST_ASSERT_FALSE(sensor.isSen69c());
    TEST_ASSERT_EQUAL(1,Sen66::state().late_start_begin_count);
    TEST_ASSERT_FALSE(Sen66::state().asc_enabled);
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0xD304));
}
void test_unknown_model_is_not_started() {
    identity("SEN68"); RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    sensor.pollLateStart(millis()); advanceMillis(20);
    TEST_ASSERT_EQUAL(int(CooperativeStart::Result::Failed),int(sensor.pollLateStart(millis())));
    TEST_ASSERT_EQUAL(int(RealSen6x::Model::Unsupported),int(sensor.model()));
    TEST_ASSERT_FALSE(sensor.isOk()); TEST_ASSERT_FALSE(sensor.isBusy());
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0x0021));
}
void test_identity_bad_crc_never_configures_sensor() {
    identity("SEN69C",true); RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    sensor.pollLateStart(millis()); advanceMillis(20);
    TEST_ASSERT_EQUAL(int(CooperativeStart::Result::Failed),int(sensor.pollLateStart(millis())));
    TEST_ASSERT_FALSE(sensor.isSen69c());
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0xD304));
}
void test_unterminated_identity_is_rejected() {
    uint16_t words[16]; for(auto &w:words) w=0x4141;
    response(0xD014,words,16);
    RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    sensor.pollLateStart(millis()); advanceMillis(20);
    TEST_ASSERT_EQUAL(int(CooperativeStart::Result::Failed),int(sensor.pollLateStart(millis())));
}
void test_transport_rejects_oversize_without_transaction() {
    uint16_t words[17]{};
    const auto before=I2cMock::transactionCount();
    TEST_ASSERT_FALSE(Sen6xTransport::receive(words,17));
    TEST_ASSERT_EQUAL(before,I2cMock::transactionCount());
}
void test_warm_mcu_restart_waits_before_stop_and_reset() {
    boot_peripherals_cold_start=false;
    Sen69c sensor; sensor.begin(); sensor.beginLateStart(true);
    const auto begin=millis();
    sensor.pollLateStart(millis()); setMillis(begin+23999); sensor.pollLateStart(millis());
    TEST_ASSERT_EQUAL(begin+23999,millis()); // no blocking delay
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0x0104));
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0xD304));
    advanceMillis(1); sensor.pollLateStart(millis()); sensor.pollLateStart(millis());
    TEST_ASSERT_EQUAL(1,I2cMock::sensorCommandCount(0x6B,0x0104));
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0xD304));
}
void test_start_reads_asc_even_after_cold_boot() {
    Sen69c sensor; start(sensor);
    TEST_ASSERT_GREATER_THAN(0,I2cMock::sensorCommandCount(0x6B,0x6711));
}
void test_asc_mismatch_fails_start_instead_of_claiming_success() {
    Sen69c sensor; sensor.begin(); word(0x6711,0); sensor.beginLateStart(true);
    for(unsigned i=0;i<300 && sensor.isBusy();++i) {sensor.pollLateStart(millis());advanceMillis(20);}
    TEST_ASSERT_FALSE(sensor.isOk()); TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0x0021));
}
void test_decode_sen69c_hcho_and_co2_are_separate_words() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_+61000);
    SensorData data; data.pressure=1010; data.pressure_valid=true;
    pollFrame(sensor,data);
    TEST_ASSERT_TRUE(data.co2_valid); TEST_ASSERT_EQUAL(987,data.co2);
    TEST_ASSERT_TRUE(data.hcho_valid); TEST_ASSERT_FLOAT_WITHIN(.01f,43.2f,data.hcho);
    TEST_ASSERT_FLOAT_WITHIN(.01f,45.67f,data.humidity);
    TEST_ASSERT_FLOAT_WITHIN(.01f,25,data.temperature);
    TEST_ASSERT_FLOAT_WITHIN(.01f,12.3f,data.pm05);
    TEST_ASSERT_TRUE(data.pressure_valid); TEST_ASSERT_EQUAL_FLOAT(1010,data.pressure);
}
void test_co2_signed_invalid_marker_does_not_become_32767ppm() {
    Sen69c sensor; start(sensor); frame(0x7FFF,0x7FFF); setMillis(sensor.start_ms_+61000);
    SensorData data; pollFrame(sensor,data);
    TEST_ASSERT_FALSE(data.co2_valid); TEST_ASSERT_EQUAL(0,data.co2);
    TEST_ASSERT_TRUE(data.hcho_valid); TEST_ASSERT_FLOAT_WITHIN(.01f,3276.7f,data.hcho);
}
void test_co2_and_hcho_warmup_are_independent() {
    Sen69c sensor; start(sensor); frame();
    TEST_ASSERT_TRUE(sensor.isCo2WarmupActive()); TEST_ASSERT_TRUE(sensor.isHchoWarmupActive());
    setMillis(sensor.start_ms_+25000); SensorData data; pollFrame(sensor,data);
    TEST_ASSERT_TRUE(data.co2_valid); TEST_ASSERT_FALSE(data.hcho_valid);
    TEST_ASSERT_FALSE(sensor.isCo2WarmupActive()); TEST_ASSERT_TRUE(sensor.isHchoWarmupActive());
    TEST_ASSERT_TRUE(sensor.isWarmupActive());
}
void test_missing_measurements_do_not_show_warmup_forever() {
    Sen69c sensor; start(sensor); setMillis(sensor.start_ms_+63000);
    TEST_ASSERT_FALSE(sensor.isCo2WarmupActive()); TEST_ASSERT_FALSE(sensor.isHchoWarmupActive());
}
void test_bad_measurement_crc_preserves_last_sample() {
    Sen69c sensor; start(sensor); frame(432,987,true); setMillis(sensor.start_ms_+61000);
    SensorData data; data.co2=777; data.co2_valid=true;
    pollFrame(sensor,data);
    TEST_ASSERT_EQUAL(777,data.co2); TEST_ASSERT_EQUAL(0,sensor.lastDataMs());
}
void test_co2_error_bit_12_does_not_invalidate_healthy_hcho() {
    Sen69c sensor; start(sensor); frame(); const uint16_t status[]={0,1U<<12};
    response(0xD206,status,2); setMillis(sensor.start_ms_+61000);
    SensorData data; pollFrame(sensor,data);
    TEST_ASSERT_FALSE(data.co2_valid); TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_FALSE(data.temp_valid); TEST_ASSERT_FALSE(data.hum_valid);
}
void test_frc_rejects_conditioning_and_resumes_after_failed_calibration() {
    Sen69c sensor; start(sensor); uint16_t correction;
    TEST_ASSERT_FALSE(sensor.calibrateFRC(420,false,0,correction));
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0x6707));
    setMillis(sensor.start_ms_+180000); word(0x6707,0xFFFF);
    TEST_ASSERT_FALSE(sensor.calibrateFRC(420,false,0,correction));
    TEST_ASSERT_TRUE(sensor.measuring_); TEST_ASSERT_TRUE(sensor.isCo2WarmupActive());
}
void test_voc_identity_mismatch_is_not_restored() {
    StorageManager storage; storage.begin();
    Sen69c sensor; sensor.begin(); sensor.setIdentity("SENSOR-B");
    Sen69c::VocRecord record; strcpy(record.serial,"SENSOR-A");
    TEST_ASSERT_TRUE(storage.saveBlobAtomic(Sen69c::VocPath,&record,sizeof(record)));
    sensor.loadVocState(storage); TEST_ASSERT_FALSE(sensor.voc_valid_);
    sensor.setIdentity("SENSOR-A"); sensor.loadVocState(storage);
    TEST_ASSERT_TRUE(sensor.voc_valid_);
    record.version = 2;
    TEST_ASSERT_TRUE(storage.saveBlobAtomic(Sen69c::VocPath,&record,sizeof(record)));
    sensor.loadVocState(storage); TEST_ASSERT_FALSE(sensor.voc_valid_);
}
void test_failed_stop_allows_recovery_after_conditioning_guard() {
    Sen69c sensor; start(sensor); setMillis(sensor.start_ms_+61000);
    I2cMock::setCommandFailure(0x6B,0x0104,true);
    TEST_ASSERT_FALSE(sensor.stop());
    I2cMock::setCommandFailure(0x6B,0x0104,false);
    TEST_ASSERT_FALSE(sensor.stop());
    advanceMillis(24000);
    TEST_ASSERT_TRUE(sensor.stop());
    TEST_ASSERT_TRUE(sensor.deviceReset());
}
void test_failed_reprobe_does_not_keep_previous_model_ready() {
    identity("SEN66"); RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    for(unsigned i=0;i<20 && sensor.isBusy();++i) {sensor.pollLateStart(millis());advanceMillis(20);}
    TEST_ASSERT_TRUE(sensor.isOk());
    identity("SEN66",true); sensor.beginLateStart(true);
    sensor.pollLateStart(millis()); advanceMillis(20);
    TEST_ASSERT_EQUAL(int(CooperativeStart::Result::Failed),int(sensor.pollLateStart(millis())));
    TEST_ASSERT_FALSE(sensor.isOk());
}
void test_pressure_does_not_overwrite_pending_identity_response() {
    identity("SEN69C"); word(0x6711,1);
    RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    for(unsigned i=0;i<300 && sensor.isBusy();++i) {sensor.pollLateStart(millis());advanceMillis(20);}
    TEST_ASSERT_TRUE(sensor.isOk());
    sensor.beginLateStart(true); sensor.pollLateStart(millis());
    sensor.updatePressure(1010);
    TEST_ASSERT_EQUAL(0,I2cMock::sensorCommandCount(0x6B,0x6720));
    advanceMillis(20); sensor.pollLateStart(millis());
    TEST_ASSERT_TRUE(sensor.isSen69c());
}
void test_offsets_changed_during_start_apply_before_first_sample() {
    boot_peripherals_cold_start = false;
    identity(); word(0x6711, 1);
    RealSen6x sensor; sensor.begin(); sensor.setOffsets(0, 0);
    sensor.beginLateStart(true);
    for (unsigned i=0; i<20 && sensor.phase_ != RealSen6x::Phase::Driver; ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_EQUAL(int(RealSen6x::Phase::Driver), int(sensor.phase_));
    sensor.setOffsets(1, 2);
    sensor.setOffsets(2, 3);
    for (unsigned i=0; i<1600 && sensor.isBusy(); ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_TRUE(sensor.isOk());
    TEST_ASSERT_EQUAL_FLOAT(2, sensor.sen69c_.temp_offset_);
    TEST_ASSERT_EQUAL_FLOAT(3, sensor.sen69c_.humidity_offset_);
    TEST_ASSERT_FALSE(sensor.offsets_pending_);
}
void test_sen66_offsets_changed_during_start_are_replayed() {
    identity("SEN66");
    RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    for (unsigned i=0; i<20 && sensor.phase_ != RealSen6x::Phase::Driver; ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    const auto writes = Sen66::state().set_offsets_call_count;
    sensor.setOffsets(2, 3);
    TEST_ASSERT_EQUAL(writes, Sen66::state().set_offsets_call_count);
    sensor.pollLateStart(millis());
    TEST_ASSERT_TRUE(sensor.isOk());
    TEST_ASSERT_EQUAL(writes + 1, Sen66::state().set_offsets_call_count);
}
void test_healthy_pm05_remains_valid_between_poll_stages() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    SensorData data; pollFrame(sensor, data);
    TEST_ASSERT_TRUE(data.pm05_valid);
    advanceMillis(1000);
    for (unsigned i=0; i<4; ++i) {
        bool changed; sensor.poll(data, changed); advanceMillis(20);
    }
    TEST_ASSERT_EQUAL(int(Sen69c::PollPhase::Numbers), int(sensor.poll_phase_));
    TEST_ASSERT_TRUE(data.pm05_valid);
    TEST_ASSERT_FLOAT_WITHIN(.01f, 12.3f, data.pm05);
}
void test_failed_number_command_invalidates_only_number_sample() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    SensorData data; pollFrame(sensor, data); advanceMillis(1000);
    I2cMock::setCommandFailure(0x6B, 0x0316, true);
    pollFrame(sensor, data);
    TEST_ASSERT_FALSE(data.pm05_valid); TEST_ASSERT_EQUAL_FLOAT(0, data.pm05);
    TEST_ASSERT_TRUE(data.co2_valid); TEST_ASSERT_TRUE(data.pm25_valid);
}
void test_failed_number_crc_invalidates_only_number_sample() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    SensorData data; pollFrame(sensor, data); advanceMillis(1000);
    const uint16_t numbers[] = {12, 34, 56, 78, 90};
    response(0x0316, numbers, 5, true);
    pollFrame(sensor, data);
    TEST_ASSERT_FALSE(data.pm05_valid); TEST_ASSERT_EQUAL_FLOAT(0, data.pm05);
    TEST_ASSERT_TRUE(data.co2_valid);
}
void test_post_control_restart_failure_obeys_guard_then_recovers() {
    identity(); word(0x6711, 1);
    RealSen6x sensor; sensor.begin(); sensor.beginLateStart(true);
    for (unsigned i=0; i<300 && sensor.isBusy(); ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_TRUE(sensor.isOk());
    advanceMillis(180000); word(0x6707, 0x8000);
    I2cMock::setCommandFailure(0x6B, 0x0021, true);
    uint16_t correction;
    TEST_ASSERT_FALSE(sensor.calibrateFRC(420, false, 0, correction));
    TEST_ASSERT_FALSE(sensor.isOk()); TEST_ASSERT_FALSE(sensor.isBusy());
    I2cMock::setCommandFailure(0x6B, 0x0021, false);
    const auto stops = I2cMock::sensorCommandCount(0x6B, 0x0104);
    sensor.beginLateStart(true);
    for (unsigned i=0; i<1100; ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_EQUAL(stops, I2cMock::sensorCommandCount(0x6B, 0x0104));
    for (unsigned i=0; i<500 && sensor.isBusy(); ++i) {
        sensor.pollLateStart(millis()); advanceMillis(20);
    }
    TEST_ASSERT_TRUE(sensor.isOk()); TEST_ASSERT_TRUE(sensor.isCo2WarmupActive());
}
void test_sticky_co2_error_recovers_after_clear_and_two_clean_status_reads() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    const uint16_t fault[] = {0, 1U << 12};
    response(0xD206, fault, 2); response(0xD210, fault, 2);
    const auto resets = I2cMock::sensorCommandCount(0x6B, 0xD304);
    SensorData data; pollFrame(sensor, data);
    TEST_ASSERT_FALSE(data.co2_valid); TEST_ASSERT_TRUE(data.hcho_valid);
    TEST_ASSERT_EQUAL(1, I2cMock::sensorCommandCount(0x6B, 0xD210));
    frame(); advanceMillis(1000); pollFrame(sensor, data);
    TEST_ASSERT_FALSE(data.co2_valid); // First clean status is not enough.
    advanceMillis(1000); pollFrame(sensor, data);
    TEST_ASSERT_TRUE(data.co2_valid); TEST_ASSERT_TRUE(data.temp_valid);
    TEST_ASSERT_EQUAL(resets, I2cMock::sensorCommandCount(0x6B, 0xD304));
}
void test_persistent_error_stays_masked_and_clear_attempts_are_throttled() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    const uint16_t fault[] = {0, 1U << 10};
    response(0xD206, fault, 2); response(0xD210, fault, 2);
    const auto begin = millis(); SensorData data;
    for (unsigned i=0; i<20; ++i) {
        pollFrame(sensor, data); advanceMillis(1000);
        TEST_ASSERT_FALSE(data.hcho_valid); TEST_ASSERT_TRUE(sensor.hasHchoFault());
        TEST_ASSERT_TRUE(data.co2_valid);
    }
    TEST_ASSERT_EQUAL(1, I2cMock::sensorCommandCount(0x6B, 0xD210));
    setMillis(begin + 31000); pollFrame(sensor, data);
    TEST_ASSERT_EQUAL(2, I2cMock::sensorCommandCount(0x6B, 0xD210));
    TEST_ASSERT_FALSE(data.hcho_valid);
}
void test_failed_status_clear_keeps_fault_and_does_not_retry_every_poll() {
    Sen69c sensor; start(sensor); frame(); setMillis(sensor.start_ms_ + 61000);
    const uint16_t fault[] = {0, 1U << 12};
    response(0xD206, fault, 2);
    I2cMock::setCommandFailure(0x6B, 0xD210, true);
    SensorData data; pollFrame(sensor, data); advanceMillis(1000);
    pollFrame(sensor, data);
    TEST_ASSERT_EQUAL(1, I2cMock::sensorCommandCount(0x6B, 0xD210));
    TEST_ASSERT_FALSE(data.co2_valid);
}
int main(int,char**) {
    UNITY_BEGIN();
    RUN_TEST(test_offsets_changed_during_start_apply_before_first_sample);
    RUN_TEST(test_sen66_offsets_changed_during_start_are_replayed);
    RUN_TEST(test_healthy_pm05_remains_valid_between_poll_stages);
    RUN_TEST(test_failed_number_command_invalidates_only_number_sample);
    RUN_TEST(test_failed_number_crc_invalidates_only_number_sample);
    RUN_TEST(test_post_control_restart_failure_obeys_guard_then_recovers);
    RUN_TEST(test_sticky_co2_error_recovers_after_clear_and_two_clean_status_reads);
    RUN_TEST(test_persistent_error_stays_masked_and_clear_attempts_are_throttled);
    RUN_TEST(test_failed_status_clear_keeps_fault_and_does_not_retry_every_poll);
    RUN_TEST(test_failed_stop_allows_recovery_after_conditioning_guard);
    RUN_TEST(test_failed_reprobe_does_not_keep_previous_model_ready);
    RUN_TEST(test_pressure_does_not_overwrite_pending_identity_response);
    RUN_TEST(test_model_probe_waits_and_selects_only_sen69c);
    RUN_TEST(test_model_probe_routes_sen66_to_existing_driver);
    RUN_TEST(test_unknown_model_is_not_started);
    RUN_TEST(test_identity_bad_crc_never_configures_sensor);
    RUN_TEST(test_unterminated_identity_is_rejected);
    RUN_TEST(test_transport_rejects_oversize_without_transaction);
    RUN_TEST(test_warm_mcu_restart_waits_before_stop_and_reset);
    RUN_TEST(test_start_reads_asc_even_after_cold_boot);
    RUN_TEST(test_asc_mismatch_fails_start_instead_of_claiming_success);
    RUN_TEST(test_decode_sen69c_hcho_and_co2_are_separate_words);
    RUN_TEST(test_co2_signed_invalid_marker_does_not_become_32767ppm);
    RUN_TEST(test_co2_and_hcho_warmup_are_independent);
    RUN_TEST(test_missing_measurements_do_not_show_warmup_forever);
    RUN_TEST(test_bad_measurement_crc_preserves_last_sample);
    RUN_TEST(test_co2_error_bit_12_does_not_invalidate_healthy_hcho);
    RUN_TEST(test_frc_rejects_conditioning_and_resumes_after_failed_calibration);
    RUN_TEST(test_voc_identity_mismatch_is_not_restored);
    return UNITY_END();
}
