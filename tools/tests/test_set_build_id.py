import subprocess
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = PROJECT_ROOT / "scripts" / "set_build_id.py"


class FakeSconsEnvironment(dict):
    def __init__(self, project_dir: Path, build_dir: Path):
        super().__init__(PROJECT_DIR=str(project_dir))
        self._build_dir = build_dir

    def GetProjectOption(self, name, default=""):
        options = {
            "custom_hardware_profile": "4_3",
            "custom_build_id_suffix": "",
        }
        return options.get(name, default)

    def AppendUnique(self, **_kwargs):
        pass

    def Append(self, **_kwargs):
        pass

    def subst(self, value):
        if value == "$BUILD_DIR":
            return str(self._build_dir)
        return value


def run_build_id_script(project_dir: Path, build_dir: Path) -> str:
    source = SCRIPT_PATH.read_text(encoding="utf-8").replace('Import("env")', "")
    namespace = {
        "__file__": str(SCRIPT_PATH),
        "__name__": "set_build_id_test",
        "env": FakeSconsEnvironment(project_dir, build_dir),
    }
    exec(compile(source, str(SCRIPT_PATH), "exec"), namespace)
    return (build_dir / "generated" / "AppBuildId.generated.h").read_text(
        encoding="utf-8"
    )


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

            clean_header = run_build_id_script(project, temp_root / "build-clean")
            self.assertNotIn("-dirty", clean_header)

            source_dir = project / "src"
            source_dir.mkdir()
            (source_dir / "untracked.cpp").write_text("int added = 1;\n", encoding="utf-8")

            dirty_header = run_build_id_script(project, temp_root / "build-dirty")
            self.assertIn("-dirty", dirty_header)


if __name__ == "__main__":
    unittest.main()
