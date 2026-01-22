#include "Arduino.h"
#include "ArduinoMock.h"

static uint32_t g_millis = 0;

HardwareSerial Serial;

uint32_t millis() {
    return g_millis;
}

void delay(uint32_t ms) {
    g_millis += ms;
}

void setMillis(uint32_t ms) {
    g_millis = ms;
}

void advanceMillis(uint32_t delta) {
    g_millis += delta;
}

uint32_t getMillis() {
    return g_millis;
}
