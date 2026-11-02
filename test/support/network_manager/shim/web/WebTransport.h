#pragma once
#include <Arduino.h>
#include <memory>
class WebRequest {};
class WebServerBackend { public: virtual ~WebServerBackend() {} WebRequest req; WebRequest &request() { return req; }
    const char *name() const { return "mock"; } void begin() {} };
std::unique_ptr<WebServerBackend> createDefaultWebServerBackend(uint16_t);
