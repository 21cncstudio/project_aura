#pragma once
#include <Arduino.h>

class Bmp580 {
public:
    bool begin();
    bool start();
    void poll();
    bool takeNewData(float &pressure_hpa, float &temperature_c);
    bool isOk() const { return ok_; }
    bool isPressureValid() const { return pressure_valid_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    void invalidate();

private:
    bool detect(uint8_t addr);
    bool writeU8(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t *buf, size_t len);
    bool readU8(uint8_t reg, uint8_t &value);
    bool softReset();
    bool waitNvmReady(uint32_t timeout_ms);
    bool configure();
    bool readRaw();
    bool compute(float &pressure_hpa, float &temperature_c);
    void tryRecover(uint32_t now, const char *reason);
    void handleNoData(uint32_t now, const char *reason);

    bool ok_ = false;
    uint8_t addr_ = 0;
    bool pressure_has_ = false;
    float pressure_filtered_ = 0.0f;
    float temperature_c_ = 0.0f;
    int32_t raw_temperature_ = 0;
    uint32_t raw_pressure_ = 0;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint32_t no_data_since_ms_ = 0;
    uint32_t last_recover_ms_ = 0;
    bool pressure_valid_ = false;
    bool has_new_data_ = false;
};
