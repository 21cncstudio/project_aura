"""Inspect the versioned Aura hardware target in an ordinary ESP32-S3 app BIN.

This is an accidental cross-model guard, not a signature verifier. The marker
is in ESP-IDF's fixed custom app descriptor slot, never found by string search.
Reference: https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/
api-reference/system/app_image_format.html#adding-a-custom-structure-to-an-application
"""

from __future__ import annotations

import argparse
from functools import reduce
import hashlib
import json
import operator
from pathlib import Path
import struct
import sys


DESCRIPTOR_OFFSET = 24 + 8 + 256
DESCRIPTOR_SIZE = 64
FLAVOR_DESCRIPTOR_OFFSET = DESCRIPTOR_OFFSET + DESCRIPTOR_SIZE
FLAVOR_DESCRIPTOR_SIZE = 32
PREFIX_SIZE = FLAVOR_DESCRIPTOR_OFFSET + FLAVOR_DESCRIPTOR_SIZE
MAGIC = b"AURA_OTA_TARGET\0"
FLAVOR_MAGIC = b"AURA_OTA_FLAVOR\0"
TARGETS = ("aura-aq-v1", "aura-aq-7-v1")
DIAGNOSTIC_TARGETS = {
    "aura-aq-diag-v1": "aura-aq-v1",
    "aura-aq-7-diag-v1": "aura-aq-7-v1",
}
IMAGE_TARGETS = TARGETS + tuple(DIAGNOSTIC_TARGETS)
FLAVORS = ("production", "diagnostic")


class OtaIdentityError(ValueError):
    def __init__(self, code: str, message: str):
        self.code = code
        super().__init__(f"{code}: {message}")


def parse_prefix(
    prefix: bytes,
    image_size: int,
    expected_target: str,
    expected_flavor: str,
) -> tuple[str, str]:
    """Mirror the firmware's bounded, fail-closed compatibility precheck."""
    if expected_target not in TARGETS:
        raise OtaIdentityError("INVALID_TARGET", "Unknown expected hardware target")
    if expected_flavor not in FLAVORS:
        raise OtaIdentityError("INVALID_FLAVOR", "Unknown or impossible running firmware flavor")
    if image_size < PREFIX_SIZE or len(prefix) < PREFIX_SIZE:
        raise OtaIdentityError("TRUNCATED", "Image does not contain the complete OTA identity prefix")
    load_address, data_length = struct.unpack_from("<II", prefix, 24)
    chip_id = struct.unpack_from("<H", prefix, 12)[0]
    app_magic = struct.unpack_from("<I", prefix, 32)[0]
    if (
        prefix[0] != 0xE9
        or not 1 <= prefix[1] <= 16
        or chip_id != 9
        or prefix[23] not in (0, 1)
        or not 0x3C000000 <= load_address < 0x3E000000
        or data_length > 0x3E000000 - load_address
        or data_length < 256 + DESCRIPTOR_SIZE + FLAVOR_DESCRIPTOR_SIZE
        or data_length % 4
        or data_length > image_size - 32
        or app_magic != 0xABCD5432
    ):
        raise OtaIdentityError("INVALID_IMAGE", "Invalid ESP32-S3 app image prefix")
    descriptor = prefix[DESCRIPTOR_OFFSET:PREFIX_SIZE]
    if descriptor[:16] != MAGIC:
        raise OtaIdentityError("MISSING_METADATA", "BIN has no Aura hardware target at the fixed descriptor offset")
    version, size = struct.unpack_from("<HH", descriptor, 16)
    if version != 1 or size != DESCRIPTOR_SIZE or any(descriptor[52:64]):
        raise OtaIdentityError("UNSUPPORTED_METADATA", "Unsupported Aura OTA descriptor version or layout")
    field = descriptor[20:52]
    target = next((candidate for candidate in IMAGE_TARGETS if field == candidate.encode("ascii").ljust(32, b"\0")), None)
    if target is None:
        raise OtaIdentityError("INVALID_TARGET", "Unknown or malformed Aura hardware target")
    physical_target = DIAGNOSTIC_TARGETS.get(target, target)
    if physical_target != expected_target:
        raise OtaIdentityError("TARGET_MISMATCH", f"BIN targets {target}, expected {expected_target}")

    flavor_descriptor = prefix[FLAVOR_DESCRIPTOR_OFFSET:PREFIX_SIZE]
    if flavor_descriptor[:16] != FLAVOR_MAGIC:
        raise OtaIdentityError("MISSING_FLAVOR_METADATA", "BIN has no Aura firmware flavor at the fixed descriptor offset")
    version, size = struct.unpack_from("<HH", flavor_descriptor, 16)
    if version != 1 or size != FLAVOR_DESCRIPTOR_SIZE:
        raise OtaIdentityError("UNSUPPORTED_FLAVOR_METADATA", "Unsupported Aura OTA flavor descriptor version or layout")
    flavor_field = flavor_descriptor[20:32]
    flavor = next((candidate for candidate in FLAVORS if flavor_field == candidate.encode("ascii").ljust(12, b"\0")), None)
    if flavor is None:
        raise OtaIdentityError("INVALID_FLAVOR", "Unknown or malformed Aura firmware flavor")

    valid_pair = (
        (target in TARGETS and flavor == "production")
        or (target in DIAGNOSTIC_TARGETS and flavor == "diagnostic")
    )
    if not valid_pair:
        raise OtaIdentityError("INCONSISTENT_IDENTITY", "OTA image target and firmware flavor are inconsistent")
    if expected_flavor == "production" and flavor != "production":
        raise OtaIdentityError("FLAVOR_MISMATCH", "Production firmware cannot accept diagnostic firmware")
    return target, flavor


def validate_image_integrity(image: bytes) -> bool:
    """Validate segment framing, ESP checksum and optional appended simple hash.

    Called after parse_prefix. This is a build/artifact check; the device's
    prefix validator leaves whole-image verification to its OTA finalizer.
    Canonical project BINs have no appended secure-boot signature or wrapper.
    """
    offset = 24
    checksum = 0xEF
    for _ in range(image[1]):
        if offset + 8 > len(image):
            raise OtaIdentityError("INVALID_IMAGE", "Truncated ESP segment header")
        data_length = struct.unpack_from("<I", image, offset + 4)[0]
        offset += 8
        end = offset + data_length
        if data_length % 4 or end > len(image):
            raise OtaIdentityError("INVALID_IMAGE", "Invalid ESP segment length")
        checksum = reduce(operator.xor, memoryview(image)[offset:end], checksum)
        offset = end
    checksum_offset = offset | 15
    if checksum_offset >= len(image) or image[checksum_offset] != checksum:
        raise OtaIdentityError("INVALID_IMAGE", "ESP image checksum is invalid")
    digest_start = checksum_offset + 1
    if image[23]:
        if len(image) != digest_start + 32:
            raise OtaIdentityError("INVALID_IMAGE", "Unexpected ESP image hash/footer length")
        if hashlib.sha256(image[:digest_start]).digest() != image[digest_start:]:
            raise OtaIdentityError("INVALID_IMAGE", "ESP appended image hash is invalid")
        return True
    if len(image) != digest_start:
        raise OtaIdentityError("INVALID_IMAGE", "Unexpected trailing bytes after ESP checksum")
    return False


def inspect_image(path: Path | str, expected_target: str, expected_flavor: str) -> dict:
    path = Path(path)
    image = path.read_bytes()
    target, flavor = parse_prefix(
        image[:PREFIX_SIZE], len(image), expected_target, expected_flavor
    )
    hash_verified = validate_image_integrity(image)
    return {
        "schema": "project-aura.ota-image-identity.v2",
        "image": str(path.resolve()),
        "hardware_target": DIAGNOSTIC_TARGETS.get(target, target),
        "ota_image_target": target,
        "firmware_flavor": flavor,
        "descriptor_offset": DESCRIPTOR_OFFSET,
        "descriptor_size": DESCRIPTOR_SIZE,
        "descriptor_version": 1,
        "flavor_descriptor_offset": FLAVOR_DESCRIPTOR_OFFSET,
        "flavor_descriptor_size": FLAVOR_DESCRIPTOR_SIZE,
        "flavor_descriptor_version": 1,
        "image_size": len(image),
        "image_sha256": hashlib.sha256(image).hexdigest(),
        "esp_checksum_verified": True,
        "esp_hash_verified": hash_verified,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--expected-target", required=True, choices=TARGETS)
    parser.add_argument("--expected-flavor", required=True, choices=FLAVORS)
    args = parser.parse_args()
    try:
        result = inspect_image(args.image, args.expected_target, args.expected_flavor)
    except (OtaIdentityError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
