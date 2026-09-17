// SPDX-License-Identifier: GPL-3.0-or-later
#include "Sen6x.h"
#include "Sen6xTransport.h"
#include "core/Logger.h"
#include <cstring>

bool Sen6x::begin() {
    model_ = Model::None; phase_ = Phase::Idle; storage_ = nullptr; identity_valid_ = false;
    offsets_pending_ = false;
    memset(product_, 0, sizeof(product_)); memset(serial_, 0, sizeof(serial_));
    sen66_.begin(); return sen69c_.begin();
}
const char *Sen6x::label() const {
    if (model_ == Model::Sen66) return "SEN66";
    if (model_ == Model::Sen69c) return "SEN69C";
    if (model_ == Model::Unsupported) return "SEN6x unsupported";
    return "SEN6x";
}
void Sen6x::setOffsets(float t, float h) {
    temp_offset_ = t; humidity_offset_ = h;
    offsets_pending_ = true;
    if (isBusy()) return;
    if (isSen69c()) sen69c_.setOffsets(t,h);
    else sen66_.setOffsets(t,h);
    offsets_pending_ = false;
}
void Sen6x::loadVocState(StorageManager &storage) {
    storage_ = &storage;
    sen66_.loadVocState(storage); // Preserve the installed SEN66 storage format.
}
void Sen6x::beginLateStart(bool asc) {
    asc_ = asc; identity_valid_ = false; phase_ = Phase::NameCommand; due_ms_ = millis();
}
CooperativeStart::Result Sen6x::failed() {
    phase_ = Phase::Idle;
    LOGW("SEN6x", "identity read failed or unsupported model");
    return CooperativeStart::Result::Failed;
}
CooperativeStart::Result Sen6x::pollLateStart(uint32_t now) {
    using namespace Sen6xTransport;
    if (phase_ == Phase::Idle) return CooperativeStart::Result::Idle;
    if (!due(now, due_ms_)) return CooperativeStart::Result::InProgress;
    switch (phase_) {
    case Phase::NameCommand:
        if (!command(0xD014)) return failed();
        phase_ = Phase::NameRead; due_ms_ = millis() + CommandMs; break;
    case Phase::NameRead:
        if (!receiveString(product_)) return failed();
        if (!strcmp(product_, "SEN66")) model_ = Model::Sen66;
        else if (!strcmp(product_, "SEN69C")) model_ = Model::Sen69c;
        else { model_ = Model::Unsupported; LOGW("SEN6x", "unsupported product: %s", product_); return failed(); }
        phase_ = Phase::SerialCommand; break;
    case Phase::SerialCommand:
        memset(serial_, 0, sizeof(serial_));
        if (!command(0xD033)) return failed();
        phase_ = Phase::SerialRead; due_ms_ = millis() + CommandMs; break;
    case Phase::SerialRead:
        if (!receiveString(serial_)) return failed();
        identity_valid_ = true;
        LOGI("SEN6x", "detected %s, serial %s", product_, serial_);
        if (isSen69c()) {
            sen69c_.setIdentity(serial_);
            if (storage_) sen69c_.loadVocState(*storage_);
            sen69c_.beginLateStart(asc_);
            sen69c_.setOffsets(temp_offset_, humidity_offset_);
        } else {
            sen66_.beginLateStart(asc_);
            sen66_.setOffsets(temp_offset_, humidity_offset_);
        }
        offsets_pending_ = false;
        phase_ = Phase::Driver; break;
    case Phase::Driver: {
        const auto result = isSen69c() ? sen69c_.pollLateStart(now) : sen66_.pollLateStart(now);
        if (result != CooperativeStart::Result::InProgress) phase_ = Phase::Idle;
        // UI settings may change while the selected driver is starting.
        // Replay the latest values only after its pending I2C response is done.
        if (result == CooperativeStart::Result::Success && offsets_pending_)
            setOffsets(temp_offset_, humidity_offset_);
        return result;
    }
    default: return failed();
    }
    return CooperativeStart::Result::InProgress;
}
bool Sen6x::start(bool asc) {
    if (isBusy()) return false;
    beginLateStart(asc);
    CooperativeStart::Result result;
    do { result = pollLateStart(millis()); delay(1); } while (result == CooperativeStart::Result::InProgress);
    return result == CooperativeStart::Result::Success;
}
bool Sen6x::isOk() const { return identity_valid_ && (isSen69c() ? sen69c_.isOk() : model_ == Model::Sen66 && sen66_.isOk()); }
bool Sen6x::isBusy() const { return phase_ != Phase::Idle || (isSen69c() ? sen69c_.isBusy() : model_ == Model::Sen66 && sen66_.isBusy()); }
bool Sen6x::isWarmupActive() const { return identity_valid_ && (isSen69c() ? sen69c_.isWarmupActive() : model_ == Model::Sen66 && sen66_.isWarmupActive()); }
int Sen6x::acquiredCo2() const { return isSen69c() ? sen69c_.acquiredCo2() : model_ == Model::Sen66 ? sen66_.acquiredCo2() : 0; }
uint32_t Sen6x::pm05LastDataMs() const { return isSen69c() ? sen69c_.pm05LastDataMs() : model_ == Model::Sen66 ? sen66_.lastDataMs() : 0; }
uint32_t Sen6x::lastDataMs() const { return isSen69c() ? sen69c_.lastDataMs() : model_ == Model::Sen66 ? sen66_.lastDataMs() : 0; }
void Sen6x::poll(SensorData &data, bool &changed) {
    changed = false;
    if (isBusy() || !identity_valid_) return;
    if (isSen69c()) sen69c_.poll(data, changed);
    else if (model_ == Model::Sen66) sen66_.poll(data, changed);
}
void Sen6x::saveVocState(StorageManager &s) { if (isBusy() || !identity_valid_) return; if (isSen69c()) sen69c_.saveVocState(s); else if (model_ == Model::Sen66) sen66_.saveVocState(s); }
void Sen6x::clearVocState(StorageManager &s) { if (isSen69c()) sen69c_.clearVocState(s); else if (model_ == Model::Sen66) sen66_.clearVocState(s); }
void Sen6x::updatePressure(float p) { if (isBusy() || !identity_valid_) return; if (isSen69c()) sen69c_.updatePressure(p); else if (model_ == Model::Sen66) sen66_.updatePressure(p); }
bool Sen6x::stop() { return !isBusy() && (isSen69c() ? sen69c_.stop() : model_ == Model::Sen66 && sen66_.stop()); }
bool Sen6x::deviceReset() { return !isBusy() && (isSen69c() ? sen69c_.deviceReset() : model_ == Model::Sen66 && sen66_.deviceReset()); }
bool Sen6x::setAscEnabled(bool e) { return isOk() && !isBusy() && (isSen69c() ? sen69c_.setAscEnabled(e) : model_ == Model::Sen66 && sen66_.setAscEnabled(e)); }
bool Sen6x::calibrateFRC(uint16_t r, bool h, float p, uint16_t &c) { return isOk() && !isBusy() && (isSen69c() ? sen69c_.calibrateFRC(r,h,p,c) : model_ == Model::Sen66 && sen66_.calibrateFRC(r,h,p,c)); }
