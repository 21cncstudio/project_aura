#pragma once
// Lab Arduino surface: the String class here is the REAL Arduino-ESP32 3.3.12
// WString.h/.cpp, not the std::string alias from test/mocks/Arduino.h.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "WString.h"
uint32_t millis();
void delay(uint32_t ms);
