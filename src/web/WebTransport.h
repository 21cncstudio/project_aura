// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <memory>
#include <stddef.h>
#include <stdint.h>

enum class WebUploadStatus : uint8_t {
    Start = 0,
    Write,
    End,
    Aborted,
    Unknown,
};

struct WebUpload {
    WebUploadStatus status = WebUploadStatus::Unknown;
    String filename;
    size_t totalSize = 0;
    size_t currentSize = 0;
    uint8_t *buf = nullptr;
};

class WebRequest {
public:
    virtual ~WebRequest() = default;

    virtual bool hasArg(const char *name) const = 0;
    virtual String arg(const char *name) const = 0;
    virtual String uri() const = 0;
    virtual void sendHeader(const char *name, const String &value, bool first = false) = 0;
    virtual void send(int status_code, const char *content_type, const String &content) = 0;
    virtual void send(int status_code, const char *content_type, const char *content) = 0;
    virtual bool clientConnected() const = 0;
    virtual void stopClient() = 0;
    virtual bool beginStreamResponse(int status_code,
                                     const char *content_type,
                                     size_t content_length,
                                     bool gzip_encoded = false) = 0;
    virtual int32_t writeStreamChunk(const uint8_t *data, size_t size, int &last_error) = 0;
    virtual bool waitUntilWritable(uint16_t wait_ms, int &last_error) = 0;
    virtual void endStreamResponse() = 0;
    virtual WebUpload upload() = 0;
};

using WebHandlerFn = void (*)();

class WebServerBackend {
public:
    virtual ~WebServerBackend() = default;

    virtual WebRequest &request() = 0;
    virtual void onGet(const char *uri, WebHandlerFn handler) = 0;
    virtual void onPost(const char *uri, WebHandlerFn handler) = 0;
    virtual void onPostUpload(const char *uri, WebHandlerFn handler, WebHandlerFn upload_handler) = 0;
    virtual void onNotFound(WebHandlerFn handler) = 0;
    virtual const char *name() const = 0;
    virtual void begin() = 0;
    virtual void stop() = 0;
};

std::unique_ptr<WebServerBackend> createDefaultWebServerBackend(uint16_t port = 80);
