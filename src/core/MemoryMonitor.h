#pragma once

#include <Arduino.h>

class MemoryMonitor {
public:
    void begin(uint32_t interval_ms);
    void poll(uint32_t now_ms);
    void logNow(const char *reason);

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    uint32_t interval_ms_ = 0;
    uint32_t last_log_ms_ = 0;
    bool enabled_ = true;
};
