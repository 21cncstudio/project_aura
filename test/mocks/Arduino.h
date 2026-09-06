#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

using String = std::string;

#ifndef PROGMEM
#define PROGMEM
#endif

uint32_t millis();
void delay(uint32_t ms);

class Print {
public:
    template <typename T>
    size_t print(const T &) { return 0; }

    template <typename T>
    size_t println(const T &) { return 0; }

    size_t println() { return 0; }

    template <typename... Args>
    size_t printf(const char *, Args...) { return 0; }
};

class HardwareSerial : public Print {
public:
    void begin(unsigned long) {}
};

extern HardwareSerial Serial;
