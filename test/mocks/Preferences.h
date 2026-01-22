#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "Arduino.h"

class Preferences {
public:
    bool begin(const char *, bool = false) { return true; }

    void clear() {
        bytes_.clear();
        u32_.clear();
        bools_.clear();
        strings_.clear();
    }

    bool remove(const char *key) {
        bool removed = false;
        removed |= bytes_.erase(key) > 0;
        removed |= u32_.erase(key) > 0;
        removed |= bools_.erase(key) > 0;
        removed |= strings_.erase(key) > 0;
        return removed;
    }

    bool isKey(const char *key) const {
        return bytes_.count(key) > 0 ||
               u32_.count(key) > 0 ||
               bools_.count(key) > 0 ||
               strings_.count(key) > 0;
    }

    size_t getBytesLength(const char *key) const {
        auto it = bytes_.find(key);
        return (it == bytes_.end()) ? 0 : it->second.size();
    }

    size_t getBytes(const char *key, void *buf, size_t maxLen) const {
        auto it = bytes_.find(key);
        if (it == bytes_.end()) {
            return 0;
        }
        size_t len = it->second.size();
        size_t copy_len = (len < maxLen) ? len : maxLen;
        if (buf && copy_len > 0) {
            memcpy(buf, it->second.data(), copy_len);
        }
        return copy_len;
    }

    size_t putBytes(const char *key, const void *buf, size_t len) {
        if (!buf || len == 0) {
            bytes_.erase(key);
            return 0;
        }
        const uint8_t *ptr = static_cast<const uint8_t *>(buf);
        bytes_[key] = std::vector<uint8_t>(ptr, ptr + len);
        return len;
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) const {
        auto it = u32_.find(key);
        return (it == u32_.end()) ? defaultValue : it->second;
    }

    bool putUInt(const char *key, uint32_t value) {
        u32_[key] = value;
        return true;
    }

    bool getBool(const char *key, bool defaultValue = false) const {
        auto it = bools_.find(key);
        return (it == bools_.end()) ? defaultValue : it->second;
    }

    bool putBool(const char *key, bool value) {
        bools_[key] = value;
        return true;
    }

    String getString(const char *key, const String &defaultValue = String()) const {
        auto it = strings_.find(key);
        return (it == strings_.end()) ? defaultValue : it->second;
    }

    bool putString(const char *key, const String &value) {
        strings_[key] = value;
        return true;
    }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> bytes_;
    std::unordered_map<std::string, uint32_t> u32_;
    std::unordered_map<std::string, bool> bools_;
    std::unordered_map<std::string, String> strings_;
};
