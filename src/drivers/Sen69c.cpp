// SPDX-License-Identifier: GPL-3.0-or-later
#include "Sen69c.h"
#include "Sen6xTransport.h"
#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "modules/StorageManager.h"
#include <cmath>
#include <cstring>

namespace {
constexpr uint16_t Start = 0x0021, Stop = 0x0104, Ready = 0x0202;
constexpr uint16_t Values = 0x04B5, Numbers = 0x0316, Status = 0xD206;
constexpr uint16_t Reset = 0xD304, Temperature = 0x60B2, Voc = 0x6181;
constexpr uint16_t Asc = 0x6711, Pressure = 0x6720, Frc = 0x6707;
constexpr uint32_t Co2Error = 1UL << 12, HchoError = 1UL << 10;
constexpr uint32_t FanError = 1UL << 4, PmError = 1UL << 11;
constexpr uint32_t GasError = 1UL << 7, RhtError = 1UL << 6;
constexpr uint32_t ErrorMask = Co2Error | HchoError | FanError | PmError | GasError | RhtError;
constexpr uint16_t ClearStatus = 0xD210;
constexpr uint32_t StatusClearIntervalMs = 30000;
uint16_t pressureWord(float pressure) {
    return static_cast<uint16_t>(lroundf(fmaxf(700, fminf(1200, pressure))));
}
}

bool Sen69c::begin() {
    *this = Sen69c{};
    state_unknown_ = !boot_peripherals_cold_start;
    unknown_since_ms_ = millis();
    reset_ms_ = millis();
    return true;
}
void Sen69c::setIdentity(const char *serial) {
    voc_valid_ = false;
    memset(voc_.serial, 0, sizeof(voc_.serial));
    if (serial) strncpy(voc_.serial, serial, sizeof(voc_.serial)-1);
}
void Sen69c::setOffsets(float temperature, float humidity) {
    if (!std::isfinite(temperature) || !std::isfinite(humidity)) return;
    temp_offset_ = temperature;
    humidity_offset_ = humidity;
    temperature_dirty_ = true;
    if (ok_ && !isBusy() && poll_phase_ == PollPhase::Idle) {
        if (!applyTemperature()) LOGW("SEN69C", "temperature offset write failed");
        delay(Sen6xTransport::CommandMs);
    }
}
void Sen69c::loadVocState(StorageManager &storage) {
    VocRecord saved{};
    voc_valid_ = voc_.serial[0] && storage.loadBlob(VocPath, &saved, sizeof(saved)) &&
                 saved.version == 1 && !memcmp(saved.serial, voc_.serial, sizeof(saved.serial));
    if (voc_valid_) memcpy(voc_.state, saved.state, sizeof(voc_.state));
}
void Sen69c::saveVocState(StorageManager &storage) {
    if (!ok_ || isBusy() || poll_phase_ != PollPhase::Idle || !voc_.serial[0] ||
        millis() - last_voc_ms_ < Config::SEN66_VOC_STATE_SAVE_MS) return;
    last_voc_ms_ = millis();
    uint16_t words[4]{};
    if (!readWords(Voc, words, 4)) return;
    for (size_t i = 0; i < 4; ++i) {
        voc_.state[2*i] = words[i] >> 8;
        voc_.state[2*i+1] = words[i];
    }
    voc_valid_ = true;
    if (!storage.saveBlobAtomic(VocPath, &voc_, sizeof(voc_)))
        LOGW("SEN69C", "VOC state save failed");
}
void Sen69c::clearVocState(StorageManager &storage) {
    storage.removeBlob(VocPath);
    voc_valid_ = false;
    memset(voc_.state, 0, sizeof(voc_.state));
}
bool Sen69c::readWords(uint16_t cmd, uint16_t *words, size_t count, uint32_t delay_ms) {
    if (!Sen6xTransport::command(cmd)) return false;
    delay(delay_ms);
    return Sen6xTransport::receive(words, count);
}
bool Sen69c::applyTemperature() {
    // Match Aura's existing enclosure correction while keeping a software
    // remainder if a new hardware offset could not be written.
    const float offset = fmaxf(-100, fminf(100, temp_offset_ - Config::BASE_TEMP_OFFSET));
    const uint16_t words[] = {uint16_t(int16_t(lroundf(offset*200))), 0, 0, 0};
    if (!Sen6xTransport::write(Temperature, words, 4)) return false;
    temperature_applied_ = true;
    temperature_dirty_ = false;
    applied_offset_ = offset;
    return true;
}
bool Sen69c::conditioning() const {
    return (state_unknown_ && millis() - unknown_since_ms_ < Co2ConditioningMs) ||
           (measuring_ && millis() - start_ms_ < Co2ConditioningMs);
}
void Sen69c::beginLateStart(bool asc) {
    asc_ = asc;
    ok_ = false;
    poll_phase_ = PollPhase::Idle;
    co2_ready_ = false;
    hcho_new_ = false;
    // On an MCU-only restart the sensor's conditioning phase is unknown.
    // Wait the full interval before STOP/reset/configuration; never block UI.
    due_ms_ = millis();
    if (state_unknown_) due_ms_ += Co2ConditioningMs;
    else if (measuring_ && millis() - start_ms_ < Co2ConditioningMs)
        due_ms_ = start_ms_ + Co2ConditioningMs;
    start_phase_ = StartPhase::Guard;
}
CooperativeStart::Result Sen69c::finishStart(bool success) {
    start_phase_ = StartPhase::Idle;
    ok_ = success;
    if (!success) {
        state_unknown_ = true;
        unknown_since_ms_ = millis();
        measuring_ = false;
        LOGW("SEN69C", "initialization failed");
    }
    return success ? CooperativeStart::Result::Success : CooperativeStart::Result::Failed;
}
CooperativeStart::Result Sen69c::pollLateStart(uint32_t now) {
    using namespace Sen6xTransport;
    if (start_phase_ == StartPhase::Idle) return CooperativeStart::Result::Idle;
    if (!due(now, due_ms_)) return CooperativeStart::Result::InProgress;
    uint16_t words[4]{};
    switch (start_phase_) {
    case StartPhase::Guard:
        start_phase_ = (state_unknown_ || measuring_) ? StartPhase::Stop : StartPhase::Reset;
        break;
    case StartPhase::Stop:
        if (!command(Stop)) return finishStart(false);
        measuring_ = false; state_unknown_ = false;
        due_ms_ = millis() + 1400; start_phase_ = StartPhase::Reset;
        break;
    case StartPhase::Reset:
        if (!command(Reset)) return finishStart(false);
        status_ = 0; temperature_applied_ = false; hcho_last_ms_ = 0;
        errors_absent_once_ = 0; status_clear_due_ms_ = millis();
        last_data_ms_ = 0; gas_start_ms_ = 0; last_pressure_ms_ = 0;
        reset_ms_ = now; due_ms_ = millis() + 1200; start_phase_ = StartPhase::Temperature;
        break;
    case StartPhase::Temperature:
        if (!applyTemperature()) return finishStart(false);
        due_ms_ = millis() + CommandMs; start_phase_ = StartPhase::Voc;
        break;
    case StartPhase::Voc:
        if (voc_valid_) {
            for (size_t i=0; i<4; ++i) words[i] = (uint16_t(voc_.state[2*i])<<8) | voc_.state[2*i+1];
            if (!write(Voc, words, 4)) return finishStart(false);
        }
        due_ms_ = millis() + CommandMs; start_phase_ = StartPhase::AscRead;
        break;
    case StartPhase::AscRead:
        if (!command(Asc)) return finishStart(false);
        due_ms_ = millis() + CommandMs; start_phase_ = StartPhase::AscReceive;
        break;
    case StartPhase::AscReceive:
        if (!receive(words, 1) || words[0] > 1) return finishStart(false);
        start_phase_ = (words[0] == uint16_t(asc_)) ? StartPhase::Start : StartPhase::AscWrite;
        break;
    case StartPhase::AscWrite:
        words[0] = asc_;
        if (!write(Asc, words, 1)) return finishStart(false);
        due_ms_ = millis() + 120; start_phase_ = StartPhase::AscVerify;
        break;
    case StartPhase::AscVerify:
        if (!command(Asc)) return finishStart(false);
        due_ms_ = millis() + CommandMs; start_phase_ = StartPhase::AscVerifyReceive;
        break;
    case StartPhase::AscVerifyReceive:
        if (!receive(words, 1) || words[0] != uint16_t(asc_)) return finishStart(false);
        start_phase_ = StartPhase::Start;
        break;
    case StartPhase::Start:
        state_unknown_ = true; // Even a failed write may have reached the sensor.
        if (!command(Start)) return finishStart(false);
        start_ms_ = now; gas_start_ms_ = now; last_voc_ms_ = now;
        due_ms_ = millis() + 50; start_phase_ = StartPhase::Started;
        break;
    case StartPhase::Started:
        measuring_ = true; state_unknown_ = false; failures_ = 0;
        return finishStart(true);
    default: return finishStart(false);
    }
    return CooperativeStart::Result::InProgress;
}
bool Sen69c::start(bool asc) {
    if (isBusy()) return false;
    beginLateStart(asc);
    while (isBusy()) { pollLateStart(millis()); delay(1); }
    return ok_;
}
bool Sen69c::stop() {
    if (isBusy() || conditioning()) return false;
    if (!measuring_ && !state_unknown_) return true;
    poll_phase_ = PollPhase::Idle;
    if (!Sen6xTransport::command(Stop)) {
        // A failed transfer may still have stopped the device. Request bounded
        // reinitialization through the manager instead of polling a stopped unit.
        ok_ = false; measuring_ = false;
        state_unknown_ = true; unknown_since_ms_ = millis(); return false;
    }
    delay(1400);
    measuring_ = false; state_unknown_ = false; co2_ready_ = false; hcho_new_ = false;
    return true;
}
bool Sen69c::deviceReset() {
    if (isBusy() || !stop()) return false;
    ok_ = false;
    if (!Sen6xTransport::command(Reset)) {
        state_unknown_ = true; unknown_since_ms_ = millis(); return false;
    }
    delay(1200);
    ok_ = false; status_ = 0; temperature_applied_ = false;
    errors_absent_once_ = 0; status_clear_due_ms_ = millis();
    hcho_last_ms_ = 0; last_data_ms_ = 0; reset_ms_ = millis();
    return true;
}
bool Sen69c::startMeasurement() {
    state_unknown_ = true;
    unknown_since_ms_ = millis();
    if (!Sen6xTransport::command(Start)) {
        ok_ = false; measuring_ = false; unknown_since_ms_ = millis(); return false;
    }
    start_ms_ = millis(); co2_ready_ = false;
    delay(50);
    measuring_ = true; state_unknown_ = false; poll_phase_ = PollPhase::Idle;
    return true;
}
bool Sen69c::applyAsc(bool enabled) {
    uint16_t value = 0;
    if (!readWords(Asc, &value, 1) || value > 1) return false;
    if (value == uint16_t(enabled)) return true;
    value = enabled;
    if (!Sen6xTransport::write(Asc, &value, 1)) return false;
    delay(120);
    return readWords(Asc, &value, 1) && value == uint16_t(enabled);
}
bool Sen69c::setAscEnabled(bool enabled) {
    if (!ok_ || isBusy() || !stop()) return false;
    const bool applied = applyAsc(enabled);
    const bool restarted = startMeasurement();
    if (applied) asc_ = enabled;
    return applied && restarted;
}
bool Sen69c::calibrateFRC(uint16_t reference, bool has_pressure, float pressure, uint16_t &correction) {
    correction = 0xFFFF;
    if (!ok_ || isBusy() || !measuring_ || millis()-start_ms_ < 180000 ||
        reference < 400 || reference > 5000 || (has_pressure && !std::isfinite(pressure))) return false;
    if (has_pressure) {
        const uint16_t hpa = pressureWord(pressure);
        if (!Sen6xTransport::write(Pressure, &hpa, 1)) return false;
        delay(20);
    }
    if (!stop()) return false;
    bool applied = Sen6xTransport::write(Frc, &reference, 1);
    if (applied) { delay(500); applied = Sen6xTransport::receive(&correction, 1) && correction != 0xFFFF; }
    const bool restarted = startMeasurement();
    return applied && restarted;
}
void Sen69c::updatePressure(float pressure) {
    if (!std::isfinite(pressure)) return;
    pressure_hpa_ = pressureWord(pressure);
    pressure_pending_ = true;
    if (!ok_ || isBusy() || poll_phase_ != PollPhase::Idle ||
        (last_pressure_ms_ && millis()-last_pressure_ms_ < Config::SEN66_PRESSURE_UPDATE_MS)) return;
    if (Sen6xTransport::write(Pressure, &pressure_hpa_, 1)) {
        last_pressure_ms_ = millis(); pressure_pending_ = false; delay(20);
    }
}
bool Sen69c::isWarmupActive() const {
    return ok_ && measuring_ && millis()-gas_start_ms_ < Config::SEN66_GAS_WARMUP_MS;
}
bool Sen69c::isCo2WarmupActive() const {
    return ok_ && measuring_ && !(status_ & (Co2Error|FanError)) && failures_ == 0 &&
           !co2_ready_ && millis()-start_ms_ < Co2ConditioningMs + 2000;
}
bool Sen69c::isHchoWarmupActive() const {
    return ok_ && measuring_ && !hasHchoFault() && failures_ == 0 &&
           !hcho_last_ms_ && millis()-gas_start_ms_ < HchoUnavailableMs + 2000;
}
bool Sen69c::hasHchoFault() const { return !ok_ || failures_ >= 3 || (status_ & (HchoError|FanError)); }
bool Sen69c::takeHcho(float &value) {
    if (!hcho_new_) return false;
    value = hcho_; hcho_new_ = false; return true;
}
void Sen69c::decodeValues(const uint16_t *w, SensorData &d) {
    const bool air_ok = !(status_ & FanError);
    const bool pm_ok = air_ok && !(status_ & PmError);
    d.pm1_valid = pm_ok && w[0] != 0xFFFF; d.pm1 = d.pm1_valid ? w[0]/10.0f : 0;
    d.pm25_valid = pm_ok && w[1] != 0xFFFF; d.pm25 = d.pm25_valid ? w[1]/10.0f : 0;
    d.pm4_valid = pm_ok && w[2] != 0xFFFF; d.pm4 = d.pm4_valid ? w[2]/10.0f : 0;
    d.pm10_valid = pm_ok && w[3] != 0xFFFF; d.pm10 = d.pm10_valid ? w[3]/10.0f : 0;
    d.pm_valid = d.pm1_valid || d.pm25_valid || d.pm4_valid || d.pm10_valid;
    d.hum_valid = air_ok && !(status_ & (RhtError|Co2Error)) && w[4] != 0x7FFF;
    d.humidity = d.hum_valid ? int16_t(w[4])/100.0f + humidity_offset_ : 0;
    d.temp_valid = air_ok && !(status_ & (RhtError|Co2Error)) && w[5] != 0x7FFF;
    d.temperature = d.temp_valid ? int16_t(w[5])/200.0f + temp_offset_ - Config::BASE_TEMP_OFFSET -
                                    (temperature_applied_ ? applied_offset_ : 0) : 0;
    d.voc_valid = air_ok && !(status_ & GasError) && w[6] != 0x7FFF;
    d.voc_index = d.voc_valid ? int(lroundf(int16_t(w[6])/10.0f)) : 0;
    d.nox_valid = air_ok && !(status_ & GasError) && w[7] != 0x7FFF;
    d.nox_index = d.nox_valid ? int(lroundf(int16_t(w[7])/10.0f)) : 0;
    d.hcho_valid = air_ok && !(status_ & HchoError) && w[8] != 0xFFFF &&
                   millis()-gas_start_ms_ >= HchoUnavailableMs;
    d.hcho = d.hcho_valid ? w[8]/10.0f : 0;
    hcho_new_ = d.hcho_valid;
    if (hcho_new_) { hcho_ = d.hcho; hcho_last_ms_ = millis(); }
    d.co2_valid = air_ok && !(status_ & Co2Error) && int16_t(w[9]) >= 0 && w[9] != 0x7FFF &&
                  millis()-start_ms_ >= Co2ConditioningMs;
    d.co2 = d.co2_valid ? int16_t(w[9]) : 0;
    acquired_co2_ = d.co2;
    co2_ready_ = d.co2_valid;
}
void Sen69c::failPoll() {
    if (failures_ < 3 && ++failures_ == 3) LOGW("SEN69C", "measurement transport failed");
    errors_absent_once_ = 0;
    poll_phase_ = PollPhase::Idle;
}
void Sen69c::acceptStatus(uint32_t raw, bool allow_recovery) {
    const uint32_t errors = raw & ErrorMask;
    uint32_t held_errors = status_ & ErrorMask;
    // D210 returns the status BEFORE clearing. Keep affected channels masked
    // until two later, consecutive 1 Hz status reads no longer report the error.
    if (allow_recovery) held_errors &= ~(errors_absent_once_ & ~errors);
    held_errors |= errors;
    errors_absent_once_ = allow_recovery ? held_errors & ~errors : 0;
    const uint32_t effective = (raw & ~ErrorMask) | held_errors;
    if (status_ != effective) {
        LOGW("SEN69C", "device status 0x%08lX", static_cast<unsigned long>(effective));
        status_ = effective;
    }
}
void Sen69c::poll(SensorData &d, bool &changed) {
    changed = false;
    if (!ok_ || isBusy() || !measuring_) return;
    const uint32_t now = millis();
    uint16_t w[10]{};
    using namespace Sen6xTransport;
    if (poll_phase_ != PollPhase::Idle && !due(now, due_ms_)) return;
    switch (poll_phase_) {
    case PollPhase::Idle:
        if (now-last_poll_ms_ < 1000) return;
        last_poll_ms_ = now;
        if (temperature_dirty_) { applyTemperature(); delay(CommandMs); }
        if (pressure_pending_) updatePressure(pressure_hpa_);
        if (!command(Status)) { failPoll(); return; }
        poll_phase_ = PollPhase::Status; break;
    case PollPhase::Status:
        if (!receive(w, 2)) { failPoll(); return; }
        acceptStatus((uint32_t(w[0]) << 16) | w[1], true);
        if ((((uint32_t(w[0]) << 16) | w[1]) & ErrorMask) && due(now, status_clear_due_ms_)) {
            // Limit clear attempts even when the transfer fails or a hardware
            // fault persists. No sensor reset or measurement restart is needed.
            status_clear_due_ms_ = millis() + StatusClearIntervalMs;
            if (!command(ClearStatus)) { failPoll(); return; }
            poll_phase_ = PollPhase::ClearedStatus; break;
        }
        if (!command(Ready)) { failPoll(); return; }
        poll_phase_ = PollPhase::Ready; break;
    case PollPhase::ClearedStatus:
        if (!receive(w, 2)) { failPoll(); return; }
        acceptStatus((uint32_t(w[0]) << 16) | w[1], false);
        if (!command(Ready)) { failPoll(); return; }
        poll_phase_ = PollPhase::Ready; break;
    case PollPhase::Ready:
        if (!receive(w, 1)) { failPoll(); return; }
        if (w[0] != 1) { poll_phase_ = PollPhase::Idle; return; }
        if (!command(Values)) { failPoll(); return; }
        poll_phase_ = PollPhase::Values; break;
    case PollPhase::Values:
        if (!receive(w, 10)) { failPoll(); return; }
        decodeValues(w, d); last_data_ms_ = now; failures_ = 0; changed = true;
        if (status_ & (FanError | PmError)) { d.pm05_valid = false; d.pm05 = 0; }
        // Keep the last fresh number sample while its new response is pending.
        if (!command(Numbers)) {
            d.pm05_valid = false; d.pm05 = 0;
            poll_phase_ = PollPhase::Idle; return;
        }
        poll_phase_ = PollPhase::Numbers; break;
    case PollPhase::Numbers:
        d.pm05_valid = receive(w, 5) && w[0] != 0xFFFF && !(status_ & (FanError|PmError));
        d.pm05 = d.pm05_valid ? w[0]/10.0f : 0;
        if (d.pm05_valid) pm05_last_ms_ = now;
        changed = true; poll_phase_ = PollPhase::Idle; break;
    }
    due_ms_ = millis() + CommandMs;
}
