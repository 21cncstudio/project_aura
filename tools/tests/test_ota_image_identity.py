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
    MAGIC,
    PREFIX_SIZE,
    OtaIdentityError,
    inspect_image,
    parse_prefix,
    validate_image_integrity,
)


def make_image(target="aura-aq-v1", *, append_hash=True):
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
    checksum = reduce(operator.xor, image[32:], 0xEF)
    image.extend(bytes(15))
    image.append(checksum)
    if append_hash:
        image.extend(hashlib.sha256(image).digest())
    return bytes(image)


class OtaImageIdentityTests(unittest.TestCase):
    def assert_rejected(self, code, image, expected="aura-aq-v1", size=None):
        with self.assertRaises(OtaIdentityError) as caught:
            parse_prefix(image[:352], len(image) if size is None else size, expected)
        self.assertEqual(code, caught.exception.code)

    def test_contract_offsets_are_fixed(self):
        self.assertEqual(288, DESCRIPTOR_OFFSET)
        self.assertEqual(64, DESCRIPTOR_SIZE)
        self.assertEqual(352, PREFIX_SIZE)
        self.assertEqual(b"AURA_OTA_TARGET\0", MAGIC)

    def test_correct_targets_and_hash(self):
        for target in ("aura-aq-v1", "aura-aq-7-v1"):
            image = make_image(target)
            self.assertEqual(target, parse_prefix(image[:352], len(image), target))
            self.assertTrue(validate_image_integrity(image))

    def test_checksum_without_appended_hash(self):
        image = make_image(append_hash=False)
        self.assertFalse(validate_image_integrity(image))

    def test_both_cross_model_directions_rejected(self):
        self.assert_rejected("TARGET_MISMATCH", make_image(), "aura-aq-7-v1")
        self.assert_rejected("TARGET_MISMATCH", make_image("aura-aq-7-v1"))

    def test_unknown_current_or_image_target_rejected(self):
        self.assert_rejected("INVALID_TARGET", make_image(), "unknown")
        self.assert_rejected("INVALID_TARGET", make_image("unknown"))

    def test_legacy_or_relocated_marker_rejected(self):
        image = bytearray(make_image())
        image[352:416] = image[288:352]
        image[288:352] = bytes(64)
        self.assert_rejected("MISSING_METADATA", image)

    def test_every_prefix_truncation_rejected(self):
        image = make_image()
        for length in range(352):
            self.assert_rejected("TRUNCATED", image[:length], size=len(image))
            self.assert_rejected("TRUNCATED", image, size=length)

    def test_invalid_esp_header_and_segment_rejected(self):
        for offset, value in ((0, 0), (1, 0), (1, 17), (12, 0), (23, 2), (32, 0)):
            image = bytearray(make_image())
            image[offset] = value
            self.assert_rejected("INVALID_IMAGE", image)
        for length in (0, 319, 321, 0xFFFFFFFF):
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
            result = inspect_image(path, "aura-aq-7-v1")
            self.assertEqual("aura-aq-7-v1", result["hardware_target"])
            self.assertEqual(hashlib.sha256(image).hexdigest(), result["image_sha256"])
            self.assertEqual(len(image), result["image_size"])
            self.assertEqual(288, result["descriptor_offset"])
            self.assertTrue(result["esp_checksum_verified"])
            self.assertTrue(result["esp_hash_verified"])
            with self.assertRaises(OtaIdentityError):
                inspect_image(path, "aura-aq-v1")


if __name__ == "__main__":
    unittest.main()
