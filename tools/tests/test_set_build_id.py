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
    ):
        super().__init__(PROJECT_DIR=str(project_dir))
        self._build_dir = build_dir
        self._environment = environment
        self._options = {
            "custom_hardware_profile": profile,
            "custom_hardware_target": target,
            "custom_build_id_suffix": suffix,
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
            self.assertIn('"hardware_target": "aura-aq-v1"', clean_identity)
            self.assertIn('"environment": "project_aura"', clean_identity)

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
                suffix="7_dual_i2c_scl6",
                environment="project_aura_7",
            )
            self.assertIn('APP_HARDWARE_TARGET "aura-aq-7-v1"', header)
            self.assertIn('"hardware_profile": "7_dual_i2c_scl6"', identity)

            with self.assertRaisesRegex(RuntimeError, "does not match"):
                run_build_id_script(
                    project,
                    temp_root / "build-invalid",
                    profile="7_dual_i2c_scl6",
                    target="aura-aq-v1",
                    suffix="7_dual_i2c_scl6",
                    environment="project_aura_7",
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
