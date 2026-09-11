import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from idf_build_env import BuildEnv


class IdfBuildEnvTests(unittest.TestCase):
    def test_both_profiles_keep_the_canonical_ota_and_touch_identity(self):
        for profile, environment, target, address in (
            ("4_3", "project_aura", "aura-aq-v1", "0x14"),
            ("7_dual_i2c", "project_aura_7", "aura-aq-7-v1", "0x5d"),
        ):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as directory:
                env = BuildEnv(profile, directory)
                env.run("set_build_id.py")
                identity = json.loads((Path(directory) / "generated/build-identity.json").read_text())
                self.assertEqual(environment, identity["environment"])
                self.assertEqual(target, identity["hardware_target"])
                self.assertEqual(target, identity["ota_image_target"])
                self.assertEqual(address, identity["gt911_address"])
                self.assertFalse(identity["periodic_memory_monitor_enabled"])

    def test_unknown_profile_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsupported hardware profile"):
            BuildEnv("unknown", "unused")

    def test_mismatched_target_still_fails_in_shared_identity_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            env = BuildEnv("7_dual_i2c", directory)
            env.options["custom_hardware_target"] = "aura-aq-v1"
            with self.assertRaisesRegex(RuntimeError, "custom_hardware_target does not match"):
                env.run("set_build_id.py")

    def test_mismatched_touch_address_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            env = BuildEnv("7_dual_i2c", directory)
            env.options["custom_gt911_address"] = "0x14"
            with self.assertRaisesRegex(RuntimeError, "custom_gt911_address does not match"):
                env.run("set_build_id.py")


if __name__ == "__main__":
    unittest.main()
