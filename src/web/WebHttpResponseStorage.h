// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

#include <array>
#include <stddef.h>

namespace WebHttpResponseStorage {

struct HeaderView {
    const char *name = nullptr;
    const char *value = nullptr;
};

// ESP-IDF's esp_http_server retains response metadata pointers until the first
// send call. Keep request-owned copies so callers may safely pass temporary
// Arduino String values to the transport abstraction.
class Storage {
public:
    static constexpr size_t kMaxHeaders = 16;

    void reset() {
        status_.clear();
        content_type_.clear();
        for (size_t i = 0; i < header_count_; ++i) {
            headers_[i].name.clear();
            headers_[i].value.clear();
        }
        header_count_ = 0;
    }

    bool storeStatus(const String &status, const char *&stored) {
        status_ = status;
        if (status_ != status) {
            status_.clear();
            stored = nullptr;
            return false;
        }
        stored = status_.c_str();
        return true;
    }

    bool storeContentType(const char *content_type, const char *&stored) {
        if (!content_type) {
            stored = nullptr;
            return false;
        }
        content_type_ = content_type;
        if (content_type_ != content_type) {
            content_type_.clear();
            stored = nullptr;
            return false;
        }
        stored = content_type_.c_str();
        return true;
    }

    bool storeHeader(const char *name, const String &value, HeaderView &stored) {
        stored = {};
        if (!name || name[0] == '\0' || header_count_ >= headers_.size()) {
            return false;
        }

        Header &header = headers_[header_count_];
        header.name = name;
        header.value = value;
        if (header.name != name || header.value != value) {
            header.name.clear();
            header.value.clear();
            return false;
        }

        stored.name = header.name.c_str();
        stored.value = header.value.c_str();
        ++header_count_;
        return true;
    }

    size_t headerCount() const {
        return header_count_;
    }

private:
    struct Header {
        String name;
        String value;
    };

    String status_;
    String content_type_;
    std::array<Header, kMaxHeaders> headers_{};
    size_t header_count_ = 0;
};

}  // namespace WebHttpResponseStorage
