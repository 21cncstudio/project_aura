// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OtaImageIdentity.h"

#include <string.h>

namespace OtaImageIdentity {
namespace {

constexpr uint8_t kEspImageMagic = 0xE9;
constexpr uint16_t kEsp32S3ChipId = 0x0009;
constexpr uint32_t kAppDescriptorMagic = 0xABCD5432;
constexpr uint32_t kDromLow = 0x3C000000;
constexpr uint32_t kDromHigh = 0x3E000000;

uint16_t read16(const uint8_t *bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t read32(const uint8_t *bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

const char *canonicalHardwareTarget(const char *target) {
    if (!target) {
        return nullptr;
    }
    if (strcmp(target, kTarget43) == 0) {
        return kTarget43;
    }
    if (strcmp(target, kTarget7) == 0) {
        return kTarget7;
    }
    return nullptr;
}

const char *physicalTargetForImageTarget(const char *target) {
    if (strcmp(target, kTarget43Diagnostic) == 0) {
        return kTarget43;
    }
    if (strcmp(target, kTarget7Diagnostic) == 0) {
        return kTarget7;
    }
    return target;
}

const char *canonicalFlavor(const char *flavor) {
    if (!flavor) {
        return nullptr;
    }
    if (strcmp(flavor, kFlavorProduction) == 0) {
        return kFlavorProduction;
    }
    if (strcmp(flavor, kFlavorDiagnostic) == 0) {
        return kFlavorDiagnostic;
    }
    return nullptr;
}

bool matchesTargetField(const uint8_t *field, const char *target) {
    const size_t length = strlen(target);
    if (memcmp(field, target, length) != 0) {
        return false;
    }
    // Require a terminator and canonical zero padding, not just a matching
    // prefix with hidden trailing bytes or an unterminated target field.
    for (size_t i = length; i < kTargetSize; ++i) {
        if (field[i] != 0) {
            return false;
        }
    }
    return true;
}

const char *canonicalImageTarget(const uint8_t *field) {
    if (matchesTargetField(field, kTarget43)) {
        return kTarget43;
    }
    if (matchesTargetField(field, kTarget7)) {
        return kTarget7;
    }
    if (matchesTargetField(field, kTarget43Diagnostic)) {
        return kTarget43Diagnostic;
    }
    if (matchesTargetField(field, kTarget7Diagnostic)) {
        return kTarget7Diagnostic;
    }
    return nullptr;
}

bool matchesFlavorField(const uint8_t *field, const char *flavor) {
    const size_t length = strlen(flavor);
    if (memcmp(field, flavor, length) != 0) {
        return false;
    }
    for (size_t i = length; i < kFlavorSize; ++i) {
        if (field[i] != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

void PrefixValidator::reset(const char *expected_target,
                            const char *expected_flavor,
                            size_t image_size) {
    memset(prefix_, 0, sizeof(prefix_));
    memset(target_, 0, sizeof(target_));
    memset(flavor_, 0, sizeof(flavor_));
    prefix_size_ = 0;
    image_size_ = image_size;
    // Keep an internal canonical string, not a pointer into caller storage.
    expected_target_ = canonicalHardwareTarget(expected_target);
    expected_flavor_ = canonicalFlavor(expected_flavor);
    status_ = expected_target_ ? (expected_flavor_ ? Status::NeedMore
                                                   : Status::InvalidFlavor)
                               : Status::InvalidTarget;
    if (status_ == Status::NeedMore && image_size_ < kPrefixSize) {
        status_ = Status::Truncated;
    }
}

size_t PrefixValidator::append(const uint8_t *bytes, size_t length) {
    if (status_ != Status::NeedMore || length == 0) {
        return 0;
    }
    if (!bytes) {
        status_ = Status::InvalidImage;
        return 0;
    }
    const size_t remaining = sizeof(prefix_) - prefix_size_;
    const size_t consumed = length < remaining ? length : remaining;
    memcpy(prefix_ + prefix_size_, bytes, consumed);
    prefix_size_ += consumed;
    if (prefix_size_ == sizeof(prefix_)) {
        validate();
    }
    return consumed;
}

Status PrefixValidator::finish() {
    if (status_ == Status::NeedMore) {
        status_ = Status::Truncated;
    }
    return status_;
}

void PrefixValidator::validate() {
    const size_t first_data_offset = kImageHeaderSize + kSegmentHeaderSize;
    const uint32_t segment_address = read32(prefix_ + kImageHeaderSize);
    const uint32_t segment_size = read32(prefix_ + kImageHeaderSize + 4);
    if (prefix_[0] != kEspImageMagic || prefix_[1] == 0 || prefix_[1] > 16 ||
        read16(prefix_ + 12) != kEsp32S3ChipId || prefix_[23] > 1 ||
        segment_address < kDromLow || segment_address >= kDromHigh ||
        segment_size > kDromHigh - segment_address ||
        segment_size < kAppDescriptorSize + kDescriptorSize + kFlavorDescriptorSize ||
        (segment_size & 3U) != 0 ||
        segment_size > image_size_ - first_data_offset ||
        read32(prefix_ + first_data_offset) != kAppDescriptorMagic) {
        status_ = Status::InvalidImage;
        return;
    }

    const uint8_t *descriptor = prefix_ + kDescriptorOffset;
    if (memcmp(descriptor, kMagic, sizeof(kMagic)) != 0) {
        status_ = Status::MissingMetadata;
        return;
    }
    if (read16(descriptor + 16) != kDescriptorVersion ||
        read16(descriptor + 18) != kDescriptorSize) {
        status_ = Status::UnsupportedMetadata;
        return;
    }
    for (size_t i = 52; i < kDescriptorSize; ++i) {
        if (descriptor[i] != 0) {
            status_ = Status::UnsupportedMetadata;
            return;
        }
    }

    const uint8_t *field = descriptor + 20;
    const char *image_target = canonicalImageTarget(field);
    if (!image_target) {
        status_ = Status::InvalidTarget;
        return;
    }
    memcpy(target_, image_target, strlen(image_target) + 1);
    if (strcmp(physicalTargetForImageTarget(image_target), expected_target_) != 0) {
        status_ = Status::TargetMismatch;
        return;
    }

    const uint8_t *flavor_descriptor = prefix_ + kFlavorDescriptorOffset;
    if (memcmp(flavor_descriptor, kFlavorMagic, sizeof(kFlavorMagic)) != 0) {
        status_ = Status::MissingFlavorMetadata;
        return;
    }
    if (read16(flavor_descriptor + 16) != kFlavorDescriptorVersion ||
        read16(flavor_descriptor + 18) != kFlavorDescriptorSize) {
        status_ = Status::UnsupportedFlavorMetadata;
        return;
    }

    const uint8_t *flavor_field = flavor_descriptor + 20;
    const char *image_flavor = nullptr;
    if (matchesFlavorField(flavor_field, kFlavorProduction)) {
        image_flavor = kFlavorProduction;
    } else if (matchesFlavorField(flavor_field, kFlavorDiagnostic)) {
        image_flavor = kFlavorDiagnostic;
    }
    if (!image_flavor) {
        status_ = Status::InvalidFlavor;
        return;
    }
    memcpy(flavor_, image_flavor, strlen(image_flavor) + 1);
    const bool valid_pair =
        (strcmp(image_target, kTarget43) == 0 &&
         strcmp(image_flavor, kFlavorProduction) == 0) ||
        (strcmp(image_target, kTarget7) == 0 &&
         strcmp(image_flavor, kFlavorProduction) == 0) ||
        (strcmp(image_target, kTarget43Diagnostic) == 0 &&
         strcmp(image_flavor, kFlavorDiagnostic) == 0) ||
        (strcmp(image_target, kTarget7Diagnostic) == 0 &&
         strcmp(image_flavor, kFlavorDiagnostic) == 0);
    if (!valid_pair) {
        status_ = Status::InconsistentIdentity;
        return;
    }

    // Production is a closed lane. Diagnostic firmware may accept another
    // diagnostic build or a production build as an explicit OTA exit path.
    status_ = strcmp(expected_flavor_, kFlavorDiagnostic) == 0 ||
                      strcmp(image_flavor, kFlavorProduction) == 0
                  ? Status::Compatible
                  : Status::FlavorMismatch;
}

} // namespace OtaImageIdentity
