from functools import reduce
import hashlib
import operator
from pathlib import Path
import struct
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ota_image_identity import (
    DESCRIPTOR_OFFSET,
    DESCRIPTOR_SIZE,
    FLAVOR_DESCRIPTOR_OFFSET,
    FLAVOR_DESCRIPTOR_SIZE,
    FLAVOR_MAGIC,
    MAGIC,
    PREFIX_SIZE,
    OtaIdentityError,
    inspect_image,
    parse_prefix,
    validate_image_integrity,
)


def make_image(target="aura-aq-v1", flavor="production", *, append_hash=True):
    image = bytearray(416)
    image[0] = 0xE9
    image[1] = 1
    image[23] = int(append_hash)
    struct.pack_into("<H", image, 12, 9)
    struct.pack_into("<II", image, 24, 0x3C000020, 384)
    struct.pack_into("<I", image, 32, 0xABCD5432)
    image[288:304] = b"AURA_OTA_TARGET\0"
    struct.pack_into("<HH", image, 304, 1, 64)
    image[308:340] = target.encode("ascii").ljust(32, b"\0")
    if flavor is not None:
        image[352:368] = FLAVOR_MAGIC
        struct.pack_into("<HH", image, 368, 1, 32)
        image[372:384] = flavor.encode("ascii").ljust(12, b"\0")
    checksum = reduce(operator.xor, image[32:], 0xEF)
    image.extend(bytes(15))
    image.append(checksum)
    if append_hash:
        image.extend(hashlib.sha256(image).digest())
    return bytes(image)


class OtaImageIdentityTests(unittest.TestCase):
    def assert_rejected(
        self,
        code,
        image,
        expected="aura-aq-v1",
        expected_flavor="production",
        size=None,
    ):
        with self.assertRaises(OtaIdentityError) as caught:
            parse_prefix(
                image[:PREFIX_SIZE],
                len(image) if size is None else size,
                expected,
                expected_flavor,
            )
        self.assertEqual(code, caught.exception.code)

    def test_contract_offsets_are_fixed(self):
        self.assertEqual(288, DESCRIPTOR_OFFSET)
        self.assertEqual(64, DESCRIPTOR_SIZE)
        self.assertEqual(352, FLAVOR_DESCRIPTOR_OFFSET)
        self.assertEqual(32, FLAVOR_DESCRIPTOR_SIZE)
        self.assertEqual(384, PREFIX_SIZE)
        self.assertEqual(b"AURA_OTA_TARGET\0", MAGIC)
        self.assertEqual(b"AURA_OTA_FLAVOR\0", FLAVOR_MAGIC)

    def test_correct_targets_and_hash(self):
        for target in ("aura-aq-v1", "aura-aq-7-v1"):
            image = make_image(target)
            self.assertEqual(
                (target, "production"),
                parse_prefix(image[:PREFIX_SIZE], len(image), target, "production"),
            )
            self.assertTrue(validate_image_integrity(image))
        diagnostic = make_image("aura-aq-7-diag-v1", "diagnostic")
        self.assertEqual(
            ("aura-aq-7-diag-v1", "diagnostic"),
            parse_prefix(
                diagnostic[:PREFIX_SIZE],
                len(diagnostic),
                "aura-aq-7-v1",
                "diagnostic",
            ),
        )

    def test_production_rejects_diagnostic_but_diagnostic_can_exit_to_production(self):
        diagnostic = make_image("aura-aq-7-diag-v1", "diagnostic")
        self.assert_rejected("FLAVOR_MISMATCH", diagnostic, "aura-aq-7-v1")
        production = make_image("aura-aq-7-v1", "production")
        self.assertEqual(
            ("aura-aq-7-v1", "production"),
            parse_prefix(
                production[:PREFIX_SIZE],
                len(production),
                "aura-aq-7-v1",
                "diagnostic",
            ),
        )

    def test_old_production_guard_rejects_new_diagnostic_on_first_transition(self):
        production = make_image("aura-aq-7-v1", "production")
        diagnostic = make_image("aura-aq-7-diag-v1", "diagnostic")
        expected_field = b"aura-aq-7-v1".ljust(32, b"\0")
        self.assertEqual(expected_field, production[308:340])
        self.assertNotEqual(expected_field, diagnostic[308:340])
        # The original 64-byte descriptor remains byte-for-byte valid for a
        # production upgrade; the diagnostic image uses an unknown old target.
        self.assertEqual(bytes(12), production[340:352])
        self.assertEqual(bytes(12), diagnostic[340:352])

    def test_target_and_flavor_pair_must_be_consistent(self):
        self.assert_rejected(
            "INCONSISTENT_IDENTITY",
            make_image("aura-aq-7-v1", "diagnostic"),
            "aura-aq-7-v1",
        )
        self.assert_rejected(
            "INCONSISTENT_IDENTITY",
            make_image("aura-aq-7-diag-v1", "production"),
            "aura-aq-7-v1",
            "diagnostic",
        )

    def test_checksum_without_appended_hash(self):
        image = make_image(append_hash=False)
        self.assertFalse(validate_image_integrity(image))

    def test_both_cross_model_directions_rejected(self):
        self.assert_rejected("TARGET_MISMATCH", make_image(), "aura-aq-7-v1")
        self.assert_rejected("TARGET_MISMATCH", make_image("aura-aq-7-v1"))

    def test_unknown_current_or_image_target_rejected(self):
        self.assert_rejected("INVALID_TARGET", make_image(), "unknown")
        self.assert_rejected("INVALID_TARGET", make_image("unknown"))

    def test_unknown_current_or_image_flavor_rejected(self):
        self.assert_rejected("INVALID_FLAVOR", make_image(), expected_flavor="unknown")
        self.assert_rejected("INVALID_FLAVOR", make_image(flavor="unknown"))
        self.assert_rejected(
            "INVALID_FLAVOR", make_image(), "aura-aq-v1", "diagnostic"
        )

    def test_legacy_target_only_image_is_rejected_by_new_guard(self):
        self.assert_rejected("MISSING_FLAVOR_METADATA", make_image(flavor=None))

    def test_legacy_or_relocated_marker_rejected(self):
        image = bytearray(make_image())
        image[352:416] = image[288:352]
        image[288:352] = bytes(64)
        self.assert_rejected("MISSING_METADATA", image)

    def test_every_prefix_truncation_rejected(self):
        image = make_image()
        for length in range(PREFIX_SIZE):
            self.assert_rejected("TRUNCATED", image[:length], size=len(image))
            self.assert_rejected("TRUNCATED", image, size=length)

    def test_invalid_esp_header_and_segment_rejected(self):
        for offset, value in ((0, 0), (1, 0), (1, 17), (12, 0), (23, 2), (32, 0)):
            image = bytearray(make_image())
            image[offset] = value
            self.assert_rejected("INVALID_IMAGE", image)
        for length in (0, 319, 348, 353, 0xFFFFFFFF):
            image = bytearray(make_image())
            struct.pack_into("<I", image, 28, length)
            self.assert_rejected("INVALID_IMAGE", image)
        image = bytearray(make_image())
        struct.pack_into("<I", image, 24, 0x3DFFFF00)
        self.assert_rejected("INVALID_IMAGE", image)

    def test_metadata_versions_and_reserved_bytes_rejected(self):
        for offset, value in ((304, 0), (304, 2), (306, 63), (306, 65), (351, 1)):
            image = bytearray(make_image())
            image[offset] = value
            self.assert_rejected("UNSUPPORTED_METADATA", image)

    def test_flavor_metadata_versions_and_fields_rejected(self):
        for offset, value in ((368, 0), (368, 2), (370, 31), (370, 33)):
            image = bytearray(make_image())
            image[offset] = value
            self.assert_rejected("UNSUPPORTED_FLAVOR_METADATA", image)
        for offset in range(352, 368):
            image = bytearray(make_image())
            image[offset] ^= 1
            self.assert_rejected("MISSING_FLAVOR_METADATA", image)
        image = bytearray(make_image())
        image[372:384] = b"x" * 12
        self.assert_rejected("INVALID_FLAVOR", image)

    def test_unterminated_or_noncanonical_target_rejected(self):
        image = bytearray(make_image())
        image[308:340] = b"x" * 32
        self.assert_rejected("INVALID_TARGET", image)
        for offset in range(318, 340):
            image = bytearray(make_image())
            image[offset] = 1
            self.assert_rejected("INVALID_TARGET", image)

    def test_modified_body_or_hash_rejected(self):
        for offset in (360, 430, 431, -1):
            image = bytearray(make_image())
            image[offset] ^= 1
            with self.assertRaises(OtaIdentityError) as caught:
                validate_image_integrity(image)
            self.assertEqual("INVALID_IMAGE", caught.exception.code)

    def test_inconsistent_segment_framing_rejected(self):
        image = bytearray(make_image())
        image[1] = 16
        with self.assertRaises(OtaIdentityError):
            validate_image_integrity(image)
        image = bytearray(make_image())
        struct.pack_into("<I", image, 28, 383)
        with self.assertRaises(OtaIdentityError):
            validate_image_integrity(image)

    def test_extra_bytes_or_truncated_footer_rejected(self):
        image = make_image()
        for invalid in (image[:-1], image + b"x", image[:-34]):
            with self.assertRaises(OtaIdentityError):
                validate_image_integrity(invalid)

    def test_artifact_report_comes_from_fixed_offset_and_exact_file(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "name-is-not-trusted.bin"
            image = make_image("aura-aq-7-v1")
            path.write_bytes(image)
            result = inspect_image(path, "aura-aq-7-v1", "production")
            self.assertEqual("aura-aq-7-v1", result["hardware_target"])
            self.assertEqual("aura-aq-7-v1", result["ota_image_target"])
            self.assertEqual("production", result["firmware_flavor"])
            self.assertEqual(hashlib.sha256(image).hexdigest(), result["image_sha256"])
            self.assertEqual(len(image), result["image_size"])
            self.assertEqual(288, result["descriptor_offset"])
            self.assertTrue(result["esp_checksum_verified"])
            self.assertTrue(result["esp_hash_verified"])
            with self.assertRaises(OtaIdentityError):
                inspect_image(path, "aura-aq-v1", "production")


if __name__ == "__main__":
    unittest.main()
