#include <unity.h>

#include <array>
#include <cstring>

#include "core/OtaImageIdentity.h"

using OtaImageIdentity::PrefixValidator;
using OtaImageIdentity::Status;

namespace {

using Image = std::array<uint8_t, 512>;

void write16(uint8_t *bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

void write32(uint8_t *bytes, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        bytes[i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

Image makeImage(const char *target = "aura-aq-v1",
                const char *flavor = "production") {
    Image image{};
    image[0] = 0xE9;
    image[1] = 1;
    write16(image.data() + 12, 9);
    write32(image.data() + 24, 0x3C000020);
    write32(image.data() + 28, image.size() - 32);
    write32(image.data() + 32, 0xABCD5432);
    std::memcpy(image.data() + 288, "AURA_OTA_TARGET", 16);
    write16(image.data() + 304, 1);
    write16(image.data() + 306, 64);
    std::memcpy(image.data() + 308, target, std::strlen(target));
    if (flavor) {
        std::memcpy(image.data() + 352, "AURA_OTA_FLAVOR", 16);
        write16(image.data() + 368, 1);
        write16(image.data() + 370, 32);
        std::memcpy(image.data() + 372, flavor, std::strlen(flavor));
    }
    for (size_t i = OtaImageIdentity::kPrefixSize; i < image.size(); ++i) {
        image[i] = static_cast<uint8_t>(i);
    }
    return image;
}

Status validate(const Image &image, const char *target = "aura-aq-v1",
                const char *flavor = "production",
                size_t declared_size = 512) {
    PrefixValidator validator;
    validator.reset(target, flavor, declared_size);
    validator.append(image.data(), image.size());
    return validator.status();
}

void assertStatus(Status expected, Status actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

} // namespace

void setUp() {}
void tearDown() {}

void test_identity_abi_is_fixed() {
    TEST_ASSERT_EQUAL_UINT32(288, OtaImageIdentity::kDescriptorOffset);
    TEST_ASSERT_EQUAL_UINT32(64, OtaImageIdentity::kDescriptorSize);
    TEST_ASSERT_EQUAL_UINT32(352, OtaImageIdentity::kFlavorDescriptorOffset);
    TEST_ASSERT_EQUAL_UINT32(32, OtaImageIdentity::kFlavorDescriptorSize);
    TEST_ASSERT_EQUAL_UINT32(384, OtaImageIdentity::kPrefixSize);
    TEST_ASSERT_EQUAL_UINT32(32, OtaImageIdentity::kTargetSize);
    TEST_ASSERT_EQUAL_UINT32(1, OtaImageIdentity::kDescriptorVersion);
    TEST_ASSERT_EQUAL_MEMORY("AURA_OTA_TARGET", OtaImageIdentity::kMagic, 16);
    TEST_ASSERT_EQUAL_MEMORY("AURA_OTA_FLAVOR", OtaImageIdentity::kFlavorMagic, 16);
}

void test_identity_accepts_each_exact_canonical_target() {
    assertStatus(Status::Compatible, validate(makeImage()));
    assertStatus(Status::Compatible, validate(makeImage("aura-aq-7-v1"), "aura-aq-7-v1"));
    assertStatus(Status::Compatible,
                 validate(makeImage("aura-aq-7-diag-v1", "diagnostic"),
                          "aura-aq-7-v1", "diagnostic"));
}

void test_identity_rejects_cross_model_in_both_directions() {
    assertStatus(Status::TargetMismatch, validate(makeImage(), "aura-aq-7-v1"));
    assertStatus(Status::TargetMismatch, validate(makeImage("aura-aq-7-v1")));

    PrefixValidator validator;
    validator.reset("aura-aq-v1", "production", 512);
    const Image image = makeImage("aura-aq-7-v1");
    validator.append(image.data(), image.size());
    TEST_ASSERT_EQUAL_STRING("aura-aq-7-v1", validator.target());
}

void test_production_rejects_diagnostic_and_diagnostic_has_production_exit() {
    assertStatus(Status::FlavorMismatch,
                 validate(makeImage("aura-aq-7-diag-v1", "diagnostic"),
                          "aura-aq-7-v1", "production"));
    assertStatus(Status::Compatible,
                 validate(makeImage("aura-aq-7-v1", "production"),
                          "aura-aq-7-v1", "diagnostic"));

    PrefixValidator validator;
    const Image image = makeImage("aura-aq-7-diag-v1", "diagnostic");
    validator.reset("aura-aq-7-v1", "production", image.size());
    validator.append(image.data(), image.size());
    TEST_ASSERT_EQUAL_STRING("aura-aq-7-diag-v1", validator.target());
    TEST_ASSERT_EQUAL_STRING("diagnostic", validator.flavor());
}

void test_hardware_mismatch_remains_the_primary_reason() {
    assertStatus(Status::TargetMismatch,
                 validate(makeImage("aura-aq-7-diag-v1", "diagnostic"),
                          "aura-aq-v1", "production"));
}

void test_identity_rejects_inconsistent_target_flavor_pairs() {
    assertStatus(Status::InconsistentIdentity,
                 validate(makeImage("aura-aq-7-v1", "diagnostic"),
                          "aura-aq-7-v1", "production"));
    assertStatus(Status::InconsistentIdentity,
                 validate(makeImage("aura-aq-7-diag-v1", "production"),
                          "aura-aq-7-v1", "diagnostic"));
    assertStatus(Status::InconsistentIdentity,
                 validate(makeImage("aura-aq-v1", "diagnostic")));
}

void test_identity_accepts_every_two_chunk_boundary() {
    const Image image = makeImage();
    for (size_t split = 0; split <= OtaImageIdentity::kPrefixSize; ++split) {
        PrefixValidator validator;
        validator.reset("aura-aq-v1", "production", image.size());
        const size_t first = validator.append(image.data(), split);
        const size_t second = validator.append(image.data() + split, image.size() - split);
        assertStatus(Status::Compatible, validator.status());
        TEST_ASSERT_EQUAL_UINT32(OtaImageIdentity::kPrefixSize, first + second);
        TEST_ASSERT_EQUAL_UINT32(OtaImageIdentity::kPrefixSize, validator.size());
        TEST_ASSERT_EQUAL_MEMORY(image.data(), validator.data(),
                                 OtaImageIdentity::kPrefixSize);
    }
}

void test_identity_accepts_single_byte_chunks_without_consuming_body() {
    const Image image = makeImage("aura-aq-7-v1");
    PrefixValidator validator;
    validator.reset("aura-aq-7-v1", "production", image.size());
    for (size_t i = 0; i < OtaImageIdentity::kPrefixSize; ++i) {
        TEST_ASSERT_EQUAL_UINT32(0, validator.append(nullptr, 0));
        assertStatus(Status::NeedMore, validator.status());
        TEST_ASSERT_EQUAL_UINT32(1, validator.append(image.data() + i, 1));
    }
    assertStatus(Status::Compatible, validator.status());
    TEST_ASSERT_EQUAL_UINT32(
        0, validator.append(image.data() + OtaImageIdentity::kPrefixSize,
                            image.size() - OtaImageIdentity::kPrefixSize));
    assertStatus(Status::Compatible, validator.finish());
}

void test_identity_rejects_every_truncated_prefix() {
    const Image image = makeImage();
    for (size_t size = 0; size < OtaImageIdentity::kPrefixSize; ++size) {
        PrefixValidator validator;
        validator.reset("aura-aq-v1", "production", image.size());
        TEST_ASSERT_EQUAL_UINT32(size, validator.append(image.data(), size));
        assertStatus(Status::NeedMore, validator.status());
        assertStatus(Status::Truncated, validator.finish());
        TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data() + size, image.size() - size));
        assertStatus(Status::Truncated, validator.status());
    }
}

void test_identity_rejects_short_declared_images_before_consuming_bytes() {
    const Image image = makeImage();
    for (size_t size = 0; size < OtaImageIdentity::kPrefixSize; ++size) {
        PrefixValidator validator;
        validator.reset("aura-aq-v1", "production", size);
        assertStatus(Status::Truncated, validator.status());
        TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data(), image.size()));
    }
}

void test_identity_rejects_unknown_running_target_and_uninitialized_state() {
    const Image image = makeImage();
    PrefixValidator validator;
    assertStatus(Status::InvalidTarget, validator.status());
    TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data(), image.size()));
    const char *targets[] = {nullptr, "", "unknown", "aura-aq-v10", "aura-aq-7-v1-extra"};
    for (const char *target : targets) {
        validator.reset(target, "production", image.size());
        assertStatus(Status::InvalidTarget, validator.status());
        TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data(), image.size()));
    }
}

void test_identity_rejects_unknown_running_flavor() {
    const Image image = makeImage();
    PrefixValidator validator;
    const char *flavors[] = {nullptr, "", "unknown", "prod", "diagnostic-extra"};
    for (const char *flavor : flavors) {
        validator.reset("aura-aq-v1", flavor, image.size());
        assertStatus(Status::InvalidFlavor, validator.status());
        TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data(), image.size()));
    }
}

void test_identity_keeps_a_canonical_copy_of_running_target() {
    char expected[] = "aura-aq-v1";
    char expected_flavor[] = "production";
    PrefixValidator validator;
    const Image image = makeImage();
    validator.reset(expected, expected_flavor, image.size());
    expected[0] = '!';
    expected_flavor[0] = '!';
    validator.append(image.data(), image.size());
    assertStatus(Status::Compatible, validator.status());
}

void test_identity_reset_clears_previous_upload_state() {
    PrefixValidator validator;
    const Image wrong = makeImage("aura-aq-7-v1");
    validator.reset("aura-aq-v1", "production", wrong.size());
    validator.append(wrong.data(), wrong.size());
    assertStatus(Status::TargetMismatch, validator.status());
    validator.reset("aura-aq-v1", "production", 512);
    assertStatus(Status::NeedMore, validator.status());
    TEST_ASSERT_EQUAL_UINT32(0, validator.size());
    TEST_ASSERT_EQUAL_STRING("", validator.target());
    TEST_ASSERT_EQUAL_STRING("", validator.flavor());
    std::array<uint8_t, OtaImageIdentity::kPrefixSize> empty{};
    TEST_ASSERT_EQUAL_MEMORY(empty.data(), validator.data(), empty.size());
    const Image correct = makeImage();
    validator.append(correct.data(), correct.size());
    assertStatus(Status::Compatible, validator.status());
}

void test_identity_rejects_null_nonempty_chunk() {
    PrefixValidator validator;
    validator.reset("aura-aq-v1", "production", 512);
    TEST_ASSERT_EQUAL_UINT32(0, validator.append(nullptr, 1));
    assertStatus(Status::InvalidImage, validator.status());
}

void test_identity_rejects_invalid_esp_header() {
    Image image = makeImage();
    image[0] = 0xEA;
    assertStatus(Status::InvalidImage, validate(image));
    for (uint8_t count : {0, 17, 255}) {
        image = makeImage();
        image[1] = count;
        assertStatus(Status::InvalidImage, validate(image));
    }
    image = makeImage();
    write16(image.data() + 12, 0);
    assertStatus(Status::InvalidImage, validate(image));
    write16(image.data() + 12, 0x0109);
    assertStatus(Status::InvalidImage, validate(image));
    image = makeImage();
    image[23] = 2;
    assertStatus(Status::InvalidImage, validate(image));
    image = makeImage();
    image[32] ^= 1;
    assertStatus(Status::InvalidImage, validate(image));
}

void test_identity_rejects_invalid_first_segment_lengths() {
    for (uint32_t length : {0U, 256U, 319U, 348U, 353U, 484U, 0xFFFFFFFCU}) {
        Image image = makeImage();
        write32(image.data() + 28, length);
        assertStatus(Status::InvalidImage, validate(image));
    }
    const Image image = makeImage();
    assertStatus(Status::InvalidImage,
                 validate(image, "aura-aq-v1", "production", 511));
}

void test_identity_rejects_segment_outside_esp32s3_drom() {
    for (uint32_t address : {0U, 0x3BFFFFFFU, 0x3E000000U, 0xFFFFFFF0U, 0x3DFFFF00U}) {
        Image image = makeImage();
        write32(image.data() + 24, address);
        assertStatus(Status::InvalidImage, validate(image));
    }
}

void test_identity_requires_metadata_at_exact_offset() {
    for (size_t i = 0; i < 16; ++i) {
        Image image = makeImage();
        image[288 + i] ^= 0x01;
        assertStatus(Status::MissingMetadata, validate(image));
    }
    Image image = makeImage();
    std::memcpy(image.data() + 416, image.data() + 288, 64);
    std::memset(image.data() + 288, 0, 64);
    assertStatus(Status::MissingMetadata, validate(image));
}

void test_identity_requires_flavor_metadata_at_exact_offset() {
    Image legacy = makeImage("aura-aq-v1", nullptr);
    assertStatus(Status::MissingFlavorMetadata, validate(legacy));
    for (size_t i = 0; i < 16; ++i) {
        Image image = makeImage();
        image[OtaImageIdentity::kFlavorDescriptorOffset + i] ^= 0x01;
        assertStatus(Status::MissingFlavorMetadata, validate(image));
    }
}

void test_identity_rejects_unsupported_or_invalid_flavor_metadata() {
    for (uint16_t version : {0, 2, 65535}) {
        Image image = makeImage();
        write16(image.data() + 368, version);
        assertStatus(Status::UnsupportedFlavorMetadata, validate(image));
    }
    for (uint16_t length : {0, 31, 33, 65535}) {
        Image image = makeImage();
        write16(image.data() + 370, length);
        assertStatus(Status::UnsupportedFlavorMetadata, validate(image));
    }
    for (const char *flavor : {"", "prod", "unknown", "DIAGNOSTIC"}) {
        assertStatus(Status::InvalidFlavor, validate(makeImage("aura-aq-v1", flavor)));
    }
    Image image = makeImage();
    std::memset(image.data() + 372, 'x', OtaImageIdentity::kFlavorSize);
    assertStatus(Status::InvalidFlavor, validate(image));
}

void test_identity_rejects_unsupported_metadata_version_or_size() {
    for (uint16_t version : {0, 2, 65535}) {
        Image image = makeImage();
        write16(image.data() + 304, version);
        assertStatus(Status::UnsupportedMetadata, validate(image));
    }
    for (uint16_t length : {0, 63, 65, 65535}) {
        Image image = makeImage();
        write16(image.data() + 306, length);
        assertStatus(Status::UnsupportedMetadata, validate(image));
    }
    for (size_t i = 340; i < 352; ++i) {
        Image image = makeImage();
        image[i] = 1;
        assertStatus(Status::UnsupportedMetadata, validate(image));
    }
}

void test_identity_rejects_invalid_target_fields() {
    for (const char *target : {"", "unknown", "aura-aq-v10", "aura-aq-7-v2", "AURA-AQ-V1"}) {
        assertStatus(Status::InvalidTarget, validate(makeImage(target)));
    }
    Image image = makeImage();
    std::memset(image.data() + 308, 'a', 32);
    assertStatus(Status::InvalidTarget, validate(image));
    for (size_t i = 308 + 10; i < 340; ++i) {
        image = makeImage();
        image[i] = 'x';
        assertStatus(Status::InvalidTarget, validate(image));
    }
}

void test_identity_rejection_is_sticky_and_consumes_nothing_more() {
    PrefixValidator validator;
    Image image = makeImage("aura-aq-7-v1");
    validator.reset("aura-aq-v1", "production", image.size());
    TEST_ASSERT_EQUAL_UINT32(OtaImageIdentity::kPrefixSize,
                             validator.append(image.data(), image.size()));
    assertStatus(Status::TargetMismatch, validator.status());
    image = makeImage();
    TEST_ASSERT_EQUAL_UINT32(0, validator.append(image.data(), image.size()));
    assertStatus(Status::TargetMismatch, validator.finish());
    TEST_ASSERT_EQUAL_STRING("aura-aq-7-v1", validator.target());
    TEST_ASSERT_EQUAL_STRING("", validator.flavor());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_identity_abi_is_fixed);
    RUN_TEST(test_identity_accepts_each_exact_canonical_target);
    RUN_TEST(test_identity_rejects_cross_model_in_both_directions);
    RUN_TEST(test_production_rejects_diagnostic_and_diagnostic_has_production_exit);
    RUN_TEST(test_hardware_mismatch_remains_the_primary_reason);
    RUN_TEST(test_identity_rejects_inconsistent_target_flavor_pairs);
    RUN_TEST(test_identity_accepts_every_two_chunk_boundary);
    RUN_TEST(test_identity_accepts_single_byte_chunks_without_consuming_body);
    RUN_TEST(test_identity_rejects_every_truncated_prefix);
    RUN_TEST(test_identity_rejects_short_declared_images_before_consuming_bytes);
    RUN_TEST(test_identity_rejects_unknown_running_target_and_uninitialized_state);
    RUN_TEST(test_identity_rejects_unknown_running_flavor);
    RUN_TEST(test_identity_keeps_a_canonical_copy_of_running_target);
    RUN_TEST(test_identity_reset_clears_previous_upload_state);
    RUN_TEST(test_identity_rejects_null_nonempty_chunk);
    RUN_TEST(test_identity_rejects_invalid_esp_header);
    RUN_TEST(test_identity_rejects_invalid_first_segment_lengths);
    RUN_TEST(test_identity_rejects_segment_outside_esp32s3_drom);
    RUN_TEST(test_identity_requires_metadata_at_exact_offset);
    RUN_TEST(test_identity_requires_flavor_metadata_at_exact_offset);
    RUN_TEST(test_identity_rejects_unsupported_metadata_version_or_size);
    RUN_TEST(test_identity_rejects_unsupported_or_invalid_flavor_metadata);
    RUN_TEST(test_identity_rejects_invalid_target_fields);
    RUN_TEST(test_identity_rejection_is_sticky_and_consumes_nothing_more);
    return UNITY_END();
}
