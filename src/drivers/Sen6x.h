// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "drivers/Sen66.h"
#include "drivers/Sen69c.h"

// Runtime selection; the two sensor implementations remain independent.
class Sen6x {
public:
    enum class Model : uint8_t { None, Sen66, Sen69c, Unsupported };
    bool begin();
    void setOffsets(float temperature, float humidity);
    void loadVocState(StorageManager &storage);
    void saveVocState(StorageManager &storage);
    void clearVocState(StorageManager &storage);
    void beginLateStart(bool asc);
    CooperativeStart::Result pollLateStart(uint32_t now);
    bool start(bool asc);
    bool stop();
    bool deviceReset();
    bool setAscEnabled(bool enabled);
    bool calibrateFRC(uint16_t reference, bool has_pressure, float pressure, uint16_t &correction);
    void updatePressure(float pressure);
    void poll(SensorData &data, bool &changed);
    bool isOk() const;
    bool isBusy() const;
    bool isWarmupActive() const;
    uint32_t lastDataMs() const;
    bool isCo2WarmupActive() const { return isOk() && isSen69c() && sen69c_.isCo2WarmupActive(); }
    bool isHchoWarmupActive() const { return isSen69c() && (isBusy() || (isOk() && sen69c_.isHchoWarmupActive())); }
    bool hasHchoFault() const { return isSen69c() && !isBusy() && (!identity_valid_ || sen69c_.hasHchoFault()); }
    bool takeHcho(float &value) { return isSen69c() && sen69c_.takeHcho(value); }
    void invalidateHcho() { if (isSen69c()) sen69c_.invalidateHcho(); }
    uint32_t hchoLastDataMs() const { return isSen69c() ? sen69c_.hchoLastDataMs() : 0; }
    bool isSen69c() const { return model_ == Model::Sen69c; }
    Model model() const { return model_; }
    const char *label() const;
    const char *serial() const { return serial_; }
private:
    enum class Phase : uint8_t { Idle, NameCommand, NameRead, SerialCommand, SerialRead, Driver };
    Sen66 sen66_;
    Sen69c sen69c_;
    Model model_ = Model::None;
    Phase phase_ = Phase::Idle;
    uint32_t due_ms_ = 0;
    bool asc_ = true;
    bool identity_valid_ = false;
    bool offsets_pending_ = false;
    float temp_offset_ = 0, humidity_offset_ = 0;
    char product_[32]{};
    char serial_[32]{};
    StorageManager *storage_ = nullptr;
    CooperativeStart::Result failed();
};
