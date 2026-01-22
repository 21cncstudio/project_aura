#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

using String = std::string;

uint32_t millis();
void delay(uint32_t ms);

class HardwareSerial {
public:
    void begin(unsigned long) {}

    template <typename T>
    size_t print(const T &) { return 0; }

    template <typename T>
    size_t println(const T &) { return 0; }

    size_t println() { return 0; }

    template <typename... Args>
    size_t printf(const char *, Args...) { return 0; }
};

extern HardwareSerial Serial;
