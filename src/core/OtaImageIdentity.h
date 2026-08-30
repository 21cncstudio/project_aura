// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace OtaImageIdentity {

// ESP-IDF places .rodata_custom_desc immediately after the first segment's
// standard app descriptor. This is part of the normal BIN, not a file wrapper.
constexpr size_t kImageHeaderSize = 24;
constexpr size_t kSegmentHeaderSize = 8;
constexpr size_t kAppDescriptorSize = 256;
constexpr size_t kDescriptorOffset =
    kImageHeaderSize + kSegmentHeaderSize + kAppDescriptorSize;
constexpr size_t kTargetSize = 32;
constexpr uint16_t kDescriptorVersion = 1;
constexpr size_t kDescriptorSize = 64;
constexpr size_t kPrefixSize = kDescriptorOffset + kDescriptorSize;
constexpr char kMagic[16] = "AURA_OTA_TARGET";
constexpr char kTarget43[] = "aura-aq-v1";
constexpr char kTarget7[] = "aura-aq-7-v1";

// Integer fields use little-endian byte order in the image. The parser decodes
// bytes explicitly and never casts unaligned/untrusted upload data to this type.
struct Descriptor {
    char magic[16];
    uint16_t version;
    uint16_t size;
    char hardware_target[kTargetSize];
    uint8_t reserved[12];
};

static_assert(sizeof(Descriptor) == kDescriptorSize, "OTA identity ABI size");
static_assert(offsetof(Descriptor, version) == 16, "OTA identity ABI version");
static_assert(offsetof(Descriptor, size) == 18, "OTA identity ABI length");
static_assert(offsetof(Descriptor, hardware_target) == 20, "OTA identity ABI target");
static_assert(offsetof(Descriptor, reserved) == 52, "OTA identity ABI reserved");

enum class Status : uint8_t {
    NeedMore,
    Compatible,
    InvalidImage,
    MissingMetadata,
    UnsupportedMetadata,
    InvalidTarget,
    TargetMismatch,
    Truncated,
};

// This only gates hardware compatibility from a bounded image prefix. Normal
// OTA image verification must still validate the rest of the image at finalize.
// A label is not a digital signature or physical-board attestation.
class PrefixValidator {
public:
    void reset(const char *expected_target, size_t image_size);

    // Consume at most the still-needed prefix. On Compatible, the caller writes
    // data()/size() once, then the unconsumed remainder of this same chunk.
    // Rejected input and already-complete prefixes consume no more bytes.
    size_t append(const uint8_t *bytes, size_t length);
    Status finish();

    Status status() const { return status_; }
    const uint8_t *data() const { return prefix_; }
    size_t size() const { return prefix_size_; }
    const char *target() const { return target_; }

private:
    void validate();

    uint8_t prefix_[kPrefixSize] = {};
    size_t prefix_size_ = 0;
    size_t image_size_ = 0;
    const char *expected_target_ = nullptr;
    char target_[kTargetSize] = {};
    Status status_ = Status::InvalidTarget;
};

} // namespace OtaImageIdentity
