// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "web/WebStreamDiagnostics.h"

// One instance per synchronous HTTP backend request, owned by its HTTP task.
// No session allocation, retries, socket settings, or HTTP framing changes.
class WebHttpSendCapture {
public:
    void reset() { error_ = {}; active_ = false; body_ = nullptr; body_size_ = 0; }
    const WebStreamError &error() const { return error_; }
    bool active() const { return active_; }
    void failSetup(int32_t sdk_error, bool from_sdk = true) {
        if (error_.terminal) return;
        error_ = {};
        error_.sdk_error = from_sdk ? sdk_error : 0;
        error_.sdk_error_valid = from_sdk;
        error_.terminal = true;
    }

    template <typename SendChunk>
    bool chunk(WebStreamStage stage, const uint8_t *body, size_t size, SendChunk send_chunk) {
        if (error_.terminal) return false;
        const uint32_t wire_bytes = error_.wire_bytes;
        error_ = {};
        error_.wire_bytes = wire_bytes;
        error_.stage = stage;
        body_ = body;
        body_size_ = size;
        active_ = true;
        const int32_t result = send_chunk();
        active_ = false;
        body_ = nullptr;
        body_size_ = 0;
        error_.sdk_error = result;
        error_.sdk_error_valid = true;
        error_.terminal = result != 0;
        if (!error_.terminal) error_.partial_body_bytes = 0;
        return !error_.terminal;
    }

    template <typename SocketSend>
    int socketSend(const char *buffer, size_t size, SocketSend send_socket) {
        errno = 0;
        const int result = send_socket();
        const int socket_errno = result < 0 ? errno : 0; // Before logging or any SDK work.
        if (active_) {
            if (result > 0) {
                error_.wire_bytes += static_cast<uint32_t>(result);
                const uintptr_t address = reinterpret_cast<uintptr_t>(buffer);
                const uintptr_t body_address = reinterpret_cast<uintptr_t>(body_);
                if (body_ && address >= body_address && address - body_address < body_size_) {
                    error_.partial_body_bytes += static_cast<uint32_t>(result);
                }
            } else if (result < 0 || size != 0) {
                error_.send_result = result;
                error_.socket_errno = socket_errno;
                error_.errno_valid = result < 0 && socket_errno != 0;
            }
        }
        // A zero-byte send of nonempty data must not spin in httpd_send_all.
        // Keep its errno invalid instead of inventing a network cause.
        return result == 0 && size != 0 ? -1 : result;
    }

private:
    WebStreamError error_{};
    const uint8_t *body_ = nullptr;
    size_t body_size_ = 0;
    bool active_ = false;
};
