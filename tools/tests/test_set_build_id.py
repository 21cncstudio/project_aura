import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = PROJECT_ROOT / "scripts" / "set_build_id.py"


class FakeSconsEnvironment(dict):
    def __init__(
        self,
        project_dir: Path,
        build_dir: Path,
        profile: str = "4_3",
        target: str = "aura-aq-v1",
        suffix: str = "",
        environment: str = "project_aura",
        gt911_address: str = "0x14",
        startup_diagnostics: str = "false",
        periodic_memory_monitor_enabled: str = "false",
    ):
        super().__init__(PROJECT_DIR=str(project_dir))
        self._build_dir = build_dir
        self._environment = environment
        self._options = {
            "custom_hardware_profile": profile,
            "custom_hardware_target": target,
            "custom_build_id_suffix": suffix,
            "custom_gt911_address": gt911_address,
            "custom_gt911_startup_diagnostics": startup_diagnostics,
            "custom_periodic_memory_monitor_enabled": periodic_memory_monitor_enabled,
        }

    def GetProjectOption(self, name, default=""):
        return self._options.get(name, default)

    def AppendUnique(self, **_kwargs):
        pass

    def Append(self, **_kwargs):
        pass

    def subst(self, value):
        if value == "$BUILD_DIR":
            return str(self._build_dir)
        if value == "$PIOENV":
            return self._environment
        return value


def run_build_id_script(project_dir: Path, build_dir: Path, **environment_options):
    source = SCRIPT_PATH.read_text(encoding="utf-8").replace('Import("env")', "")
    namespace = {
        "__file__": str(SCRIPT_PATH),
        "__name__": "set_build_id_test",
        "env": FakeSconsEnvironment(project_dir, build_dir, **environment_options),
    }
    exec(compile(source, str(SCRIPT_PATH), "exec"), namespace)
    header = (build_dir / "generated" / "AppBuildId.generated.h").read_text(
        encoding="utf-8"
    )
    identity = (build_dir / "generated" / "build-identity.json").read_text(
        encoding="utf-8"
    )
    return header, identity


class BuildIdScriptTests(unittest.TestCase):
    def test_diagnostic_identity_is_distinct_without_relaxing_profile_identity(self):
        def git_result(command, **_kwargs):
            return " M diagnostic.cpp" if command[1] == "status" else "5383b77"

        with tempfile.TemporaryDirectory() as temp_dir, patch(
            "subprocess.check_output", side_effect=git_result
        ):
            temp_root = Path(temp_dir)
            header, identity_json = run_build_id_script(
                temp_root, temp_root / "build-diag",
                profile="7_dual_i2c_scl6", target="aura-aq-7-v1",
                suffix="7-dual-i2c-scl6", environment="project_aura_7_gt911_5d",
                gt911_address="0x5D", startup_diagnostics="true",
            )
            self.assertIn("5383b77-7-dual-i2c-scl6-gt911-5d-diag-dirty", header)
            identity = json.loads(identity_json)
            self.assertTrue(identity["diagnostic_only"])
            self.assertEqual("0x5d", identity["gt911_address"])
            self.assertTrue(identity["gt911_startup_diagnostics"])
            self.assertEqual("aura-aq-7-v1", identity["hardware_target"])
            self.assertEqual("diagnostic", identity["firmware_flavor"])
            self.assertEqual("aura-aq-7-diag-v1", identity["ota_image_target"])
            self.assertFalse(identity["periodic_memory_monitor_enabled"])
            self.assertIn('APP_HARDWARE_TARGET "aura-aq-7-v1"', header)
            self.assertIn('APP_FIRMWARE_FLAVOR "diagnostic"', header)
            self.assertIn('APP_OTA_IMAGE_TARGET "aura-aq-7-diag-v1"', header)

            with self.assertRaisesRegex(RuntimeError, "does not match"):
                run_build_id_script(
                    temp_root, temp_root / "build-bad-suffix",
                    profile="7_dual_i2c_scl6", target="aura-aq-7-v1",
                    suffix="unverified-profile", gt911_address="0x5D",
                    startup_diagnostics="true",
                )

    def test_boot_only_memory_monitor_keeps_production_identity_for_both_profiles(self):
        def git_result(command, **_kwargs):
            return " M memory.cpp" if command[1] == "status" else "5383b77"

        with tempfile.TemporaryDirectory() as temp_dir, patch(
            "subprocess.check_output", side_effect=git_result
        ):
            temp_root = Path(temp_dir)
            profiles = (
                {
                    "profile": "4_3",
                    "target": "aura-aq-v1",
                    "suffix": "",
                    "environment": "project_aura",
                    "gt911_address": "0x14",
                    "expected_build_id": "5383b77-dirty",
                },
                {
                    "profile": "7_dual_i2c_scl6",
                    "target": "aura-aq-7-v1",
                    "suffix": "7-dual-i2c-scl6",
                    "environment": "project_aura_7",
                    "gt911_address": "0x5D",
                    "expected_build_id": "5383b77-7-dual-i2c-scl6-dirty",
                },
            )
            for profile in profiles:
                with self.subTest(profile=profile["profile"]):
                    header, identity_json = run_build_id_script(
                        temp_root,
                        temp_root / f"build-{profile['environment']}",
                        profile=profile["profile"],
                        target=profile["target"],
                        suffix=profile["suffix"],
                        environment=profile["environment"],
                        gt911_address=profile["gt911_address"],
                    )
                    self.assertIn(profile["expected_build_id"], header)
                    self.assertNotIn("memlog-off-diag", header)
                    identity = json.loads(identity_json)
                    self.assertEqual(profile["profile"], identity["hardware_profile"])
                    self.assertEqual(profile["target"], identity["hardware_target"])
                    self.assertEqual("production", identity["firmware_flavor"])
                    self.assertEqual(profile["target"], identity["ota_image_target"])
                    self.assertFalse(identity["gt911_startup_diagnostics"])
                    self.assertFalse(identity["periodic_memory_monitor_enabled"])
                    self.assertNotIn("diagnostic_only", identity)

    def test_periodic_memory_monitor_requires_a_separate_diagnostic_lane(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            with self.assertRaisesRegex(
                RuntimeError, "separately defined diagnostic firmware lane"
            ):
                run_build_id_script(
                    temp_root,
                    temp_root / "build-periodic-opt-in",
                    periodic_memory_monitor_enabled="true",
                )

    def test_address_and_diagnostics_follow_the_split_profiles(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            with self.assertRaisesRegex(RuntimeError, "does not match"):
                run_build_id_script(
                    temp_root, temp_root / "build-four-address", gt911_address="0x5D"
                )
            with self.assertRaisesRegex(RuntimeError, "must be 0x14 or 0x5D"):
                run_build_id_script(
                    temp_root, temp_root / "build-invalid-address",
                    profile="7_dual_i2c_scl6", target="aura-aq-7-v1",
                    suffix="7-dual-i2c-scl6", gt911_address="0xBA",
                )
            with self.assertRaisesRegex(RuntimeError, "require the 7-inch dual-I2C"):
                run_build_id_script(
                    temp_root, temp_root / "build-four-diag",
                    startup_diagnostics="true",
                )
            with self.assertRaisesRegex(RuntimeError, "must be true or false"):
                run_build_id_script(
                    temp_root, temp_root / "build-invalid-diag-flag",
                    startup_diagnostics="sometimes",
                )
            with self.assertRaisesRegex(RuntimeError, "must be true or false"):
                run_build_id_script(
                    temp_root,
                    temp_root / "build-invalid-memory-flag",
                    periodic_memory_monitor_enabled="sometimes",
                )

    def test_untracked_build_input_marks_artifact_dirty(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            project = temp_root / "project"
            subprocess.run(["git", "init", "-q", str(project)], check=True)
            subprocess.run(
                ["git", "-C", str(project), "config", "user.email", "test@example.com"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(project), "config", "user.name", "Test"],
                check=True,
            )
            tracked = project / "tracked.txt"
            tracked.write_text("baseline\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(project), "add", "tracked.txt"], check=True)
            subprocess.run(
                ["git", "-C", str(project), "commit", "-q", "-m", "baseline"],
                check=True,
            )

            clean_header, clean_identity = run_build_id_script(
                project, temp_root / "build-clean"
            )
            self.assertNotIn("-dirty", clean_header)
            clean_identity_payload = json.loads(clean_identity)
            self.assertEqual("aura-aq-v1", clean_identity_payload["hardware_target"])
            self.assertEqual("production", clean_identity_payload["firmware_flavor"])
            self.assertEqual("aura-aq-v1", clean_identity_payload["ota_image_target"])
            self.assertEqual("project_aura", clean_identity_payload["environment"])
            self.assertEqual("0x14", clean_identity_payload["gt911_address"])
            self.assertFalse(clean_identity_payload["gt911_startup_diagnostics"])
            self.assertFalse(clean_identity_payload["periodic_memory_monitor_enabled"])
            self.assertNotIn("diagnostic_only", clean_identity_payload)

            source_dir = project / "src"
            source_dir.mkdir()
            (source_dir / "untracked.cpp").write_text("int added = 1;\n", encoding="utf-8")

            dirty_header, dirty_identity = run_build_id_script(
                project, temp_root / "build-dirty"
            )
            self.assertIn("-dirty", dirty_header)
            self.assertIn("-dirty", dirty_identity)

    def test_seven_inch_identity_uses_strict_target_pair(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            project = temp_root / "project"
            subprocess.run(["git", "init", "-q", str(project)], check=True)
            subprocess.run(
                ["git", "-C", str(project), "config", "user.email", "test@example.com"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(project), "config", "user.name", "Test"],
                check=True,
            )
            tracked = project / "tracked.txt"
            tracked.write_text("baseline\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(project), "add", "tracked.txt"], check=True)
            subprocess.run(
                ["git", "-C", str(project), "commit", "-q", "-m", "baseline"],
                check=True,
            )

            header, identity = run_build_id_script(
                project,
                temp_root / "build-seven",
                profile="7_dual_i2c_scl6",
                target="aura-aq-7-v1",
                suffix="7-dual-i2c-scl6",
                environment="project_aura_7",
                gt911_address="0x5D",
            )
            self.assertIn('APP_HARDWARE_TARGET "aura-aq-7-v1"', header)
            self.assertIn('APP_FIRMWARE_FLAVOR "production"', header)
            self.assertIn('APP_OTA_IMAGE_TARGET "aura-aq-7-v1"', header)
            identity_payload = json.loads(identity)
            self.assertEqual("7_dual_i2c_scl6", identity_payload["hardware_profile"])
            self.assertEqual("0x5d", identity_payload["gt911_address"])
            self.assertFalse(identity_payload["gt911_startup_diagnostics"])
            self.assertFalse(identity_payload["periodic_memory_monitor_enabled"])
            self.assertEqual("production", identity_payload["firmware_flavor"])
            self.assertEqual("aura-aq-7-v1", identity_payload["ota_image_target"])
            self.assertNotIn("diagnostic_only", identity_payload)

            with self.assertRaisesRegex(RuntimeError, "does not match"):
                run_build_id_script(
                    project,
                    temp_root / "build-invalid",
                    profile="7_dual_i2c_scl6",
                    target="aura-aq-v1",
                    suffix="7-dual-i2c-scl6",
                    environment="project_aura_7",
                    gt911_address="0x5D",
                )

    def test_git_status_failure_aborts_the_build_identity_generation(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            project = temp_root / "project"
            subprocess.run(["git", "init", "-q", str(project)], check=True)
            subprocess.run(
                ["git", "-C", str(project), "config", "user.email", "test@example.com"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(project), "config", "user.name", "Test"],
                check=True,
            )
            (project / "tracked.txt").write_text("baseline\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(project), "add", "tracked.txt"], check=True)
            subprocess.run(
                ["git", "-C", str(project), "commit", "-q", "-m", "baseline"],
                check=True,
            )

            real_check_output = subprocess.check_output

            def fail_status(command, *args, **kwargs):
                if command[1:] == ["status", "--porcelain"]:
                    raise subprocess.CalledProcessError(17, command)
                return real_check_output(command, *args, **kwargs)

            with patch("subprocess.check_output", side_effect=fail_status):
                with self.assertRaisesRegex(
                    RuntimeError, "Required Git command failed: git status --porcelain"
                ):
                    run_build_id_script(project, temp_root / "build-status-failed")


if __name__ == "__main__":
    unittest.main()
