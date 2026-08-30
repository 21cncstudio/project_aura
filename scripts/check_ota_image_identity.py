"""Fail either production build if its embedded OTA model identity is absent.

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
    if (
        identity.get("hardware_target") != expected_target
        or identity.get("environment") != str(env.subst("$PIOENV"))
    ):
        raise RuntimeError("OTA hardware target build identity does not match the selected environment")
    result = inspect_image(image_path, expected_target)
    print(
        f"[ota-image-identity] verified target={result['hardware_target']} "
        f"offset={result['descriptor_offset']} size={result['descriptor_size']} "
        f"ESP_checksum=valid ESP_hash={'valid' if result['esp_hash_verified'] else 'absent'}"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_ota_image_identity)
