#pragma once
#include <Arduino.h>

class Sfa3x {
public:
    bool begin();
    void start();
    void stop();
    bool readData(float &hcho_ppb);
    void poll();
    bool isDataValid() const { return data_valid_; }
    bool isOk() const { return ok_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    bool takeNewData(float &hcho_ppb);
    void invalidate();

private:
    bool readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms);
    bool writeCmd(uint16_t cmd);
    bool readBytes(uint8_t *buf, size_t len);

    bool ok_ = false;
    bool measuring_ = false;
    bool data_valid_ = false;
    bool has_new_data_ = false;
    float last_hcho_ppb_ = 0.0f;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint8_t fail_count_ = 0;
};
