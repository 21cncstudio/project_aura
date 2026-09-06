"""Fail any Aura firmware build if its embedded OTA identity is absent.

The check runs on the completed BIN, so a retained symbol at the wrong linker
offset cannot silently produce an unprotected release artifact.
"""

import json
from pathlib import Path
import sys

Import("env")

sys.path.insert(0, str(Path(env["PROJECT_DIR"]) / "tools"))
from ota_image_identity import inspect_image


def check_ota_image_identity(source, target, env):
    image_path = Path(str(target[0]))
    identity_path = Path(env.subst("$BUILD_DIR")) / "generated" / "build-identity.json"
    identity = json.loads(identity_path.read_text(encoding="utf-8"))
    expected_target = str(env.GetProjectOption("custom_hardware_target", ""))
    expected_flavor = str(identity.get("firmware_flavor", ""))
    expected_image_target = str(identity.get("ota_image_target", ""))
    if (
        identity.get("hardware_target") != expected_target
        or identity.get("environment") != str(env.subst("$PIOENV"))
    ):
        raise RuntimeError("OTA hardware target build identity does not match the selected environment")
    result = inspect_image(image_path, expected_target, expected_flavor)
    if (
        result.get("firmware_flavor") != expected_flavor
        or result.get("ota_image_target") != expected_image_target
    ):
        raise RuntimeError("OTA embedded firmware flavor does not match the selected environment")
    print(
        f"[ota-image-identity] verified target={result['hardware_target']} "
        f"image_target={result['ota_image_target']} flavor={result['firmware_flavor']} "
        f"offset={result['descriptor_offset']} size={result['descriptor_size']} "
        f"ESP_checksum=valid ESP_hash={'valid' if result['esp_hash_verified'] else 'absent'}"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_ota_image_identity)
