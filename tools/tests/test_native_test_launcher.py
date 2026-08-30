import copy
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from scripts import run_native_tests as launcher
from scripts.run_native_tests import (
    DEFAULT_INVOCATIONS,
    NativeTestError,
    choose_invocations,
    compiler_first_environment,
    exclusive_launcher_lock,
    isolated_build_dir,
    pio_command,
    platformio_runtime,
    validate_report,
)


class NativeTestLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.project = Path(self.temp.name).resolve()
        self.suite = "test_ch422g_register_write"
        self.key = ("native_test", self.suite)
        self.expected_dir = self.project / "test" / self.suite
        self.expected_dir.mkdir(parents=True)
        self.source = self.expected_dir / (self.suite + ".cpp")
        self.source.write_text("// test source\n", encoding="utf-8")
        self.expected = {self.key: self.expected_dir}
        self.report = {
            "version": "1.0", "project_dir": str(self.project),
            "testcase_nums": 1, "error_nums": 0, "failure_nums": 0,
            "test_suites": [{
                "env_name": self.key[0], "test_name": self.suite,
                "status": "PASSED", "timestamp": "2026-08-27T12:00:00",
                "testcase_nums": 1,
                "test_cases": [{
                    "name": "test_masked_write", "status": "PASSED",
                    "source": {"file": f"test/{self.suite}/{self.source.name}", "line": 12},
                }],
            }],
        }

    def case(self):
        return self.report["test_suites"][0]["test_cases"][0]

    def test_accepts_real_relative_windows_and_absolute_unity_paths(self):
        for filename in (
            f"test/{self.suite}/{self.source.name}",
            f"test\\{self.suite}\\{self.source.name}",
            str(self.source),
        ):
            with self.subTest(filename=filename):
                self.case()["source"]["file"] = filename
                counts = validate_report(self.report, self.project, self.expected)
                self.assertEqual({f"native_test:{self.suite}": 1}, counts)

    def test_rejects_stale_app_version_executable_even_if_pio_reports_pass(self):
        other_dir = self.project / "test" / "test_app_version"
        other_dir.mkdir()
        (other_dir / "test_app_version.cpp").write_text("", encoding="utf-8")
        self.case()["source"]["file"] = "test/test_app_version/test_app_version.cpp"
        with self.assertRaisesRegex(NativeTestError, "Wrong Unity suite"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_basename_or_missing_source_instead_of_guessing_suite(self):
        for filename in (self.source.name, "", None):
            with self.subTest(filename=filename):
                self.case()["source"]["file"] = filename
                with self.assertRaises(NativeTestError):
                    validate_report(self.report, self.project, self.expected)

    def test_rejects_path_traversal_outside_selected_suite(self):
        other = self.project / "test" / "outside.cpp"
        other.write_text("", encoding="utf-8")
        self.case()["source"]["file"] = f"test/{self.suite}/../outside.cpp"
        with self.assertRaisesRegex(NativeTestError, "Wrong Unity suite"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_selected_suite_without_real_cases(self):
        self.report["test_suites"][0].update(test_cases=[], testcase_nums=0)
        self.report["testcase_nums"] = 0
        with self.assertRaisesRegex(NativeTestError, "real cases"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_missing_selected_suite(self):
        self.report["test_suites"] = []
        with self.assertRaisesRegex(NativeTestError, "missing from report"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_unexpected_active_suite_but_allows_pio_skipped_entries(self):
        skipped = {
            "env_name": "native_test", "test_name": "test_unselected",
            "status": "SKIPPED", "test_cases": [], "timestamp": None,
        }
        self.report["test_suites"].append(skipped)
        validate_report(self.report, self.project, self.expected)
        skipped["timestamp"] = "2026-08-27T12:00:00"
        with self.assertRaisesRegex(NativeTestError, "Unexpected active suite"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_duplicate_suite_reports(self):
        self.report["test_suites"].append(copy.deepcopy(self.report["test_suites"][0]))
        with self.assertRaisesRegex(NativeTestError, "Duplicate suite"):
            validate_report(self.report, self.project, self.expected)

    def test_rejects_failure_and_inconsistent_report_totals(self):
        for changes in (
            {"failure_nums": 1},
            {"error_nums": 1},
            {"testcase_nums": 20},
            {"project_dir": str(self.project.parent)},
            {"version": "unknown"},
        ):
            with self.subTest(changes=changes):
                report = dict(self.report, **changes)
                with self.assertRaises(NativeTestError):
                    validate_report(report, self.project, self.expected)
        self.case()["status"] = "FAILED"
        with self.assertRaisesRegex(NativeTestError, "Non-passing Unity case"):
            validate_report(self.report, self.project, self.expected)

    def test_ignored_cases_do_not_substitute_for_a_passed_suite(self):
        self.case()["status"] = "SKIPPED"
        with self.assertRaisesRegex(NativeTestError, "No passed Unity cases"):
            validate_report(self.report, self.project, self.expected)

    def test_compiler_path_is_first_without_mutating_caller_environment(self):
        original = {"PATH": "android-bin" + os.pathsep + "old-compiler",
                    "PYTHONPATH": "other-core", "PYTHONHOME": "other-python"}
        result = compiler_first_environment(original, Path("supported-mingw"))
        self.assertTrue(result["PATH"].startswith("supported-mingw" + os.pathsep))
        self.assertEqual("1", result["PYTHONNOUSERSITE"])
        self.assertNotIn("PYTHONPATH", result)
        self.assertNotIn("PYTHONHOME", result)
        self.assertEqual("other-core", original["PYTHONPATH"])
        self.assertTrue(original["PATH"].startswith("android-bin"))

    def test_build_directory_separates_runtime_toolchain_and_suite_selection(self):
        identity = {"python": "3.11.7", "gcc": "16.1", "dll_sha": "first"}
        baseline = isolated_build_dir(self.project, identity, self.expected)
        for changed_identity, changed_selection in (
            ({**identity, "python": "3.12"}, self.expected),
            ({**identity, "dll_sha": "replacement"}, self.expected),
            (identity, {("native_test", "test_app_version"): self.expected_dir}),
        ):
            with self.subTest(identity=changed_identity, selection=changed_selection):
                self.assertNotEqual(
                    baseline, isolated_build_dir(self.project, changed_identity, changed_selection)
                )
        self.assertEqual(baseline, isolated_build_dir(self.project, identity, self.expected))
        self.assertTrue(baseline.is_relative_to(self.project / ".pio" / "native-tests"))

    @unittest.skipUnless(os.name == "nt", "Official launcher uses the Windows penv layout")
    def test_canonical_penv_can_upgrade_python_or_platformio_distribution(self):
        penv = self.project / ".platformio" / "penv"
        python = penv / "Scripts" / "python.exe"
        fake_pio = SimpleNamespace(
            __title__="platformio", __version__="6.2.0",
            __file__=str(penv / "Lib" / "site-packages" / "platformio" / "__init__.py"),
        )
        with (
            patch.object(launcher.Path, "home", return_value=self.project),
            patch.object(launcher.sys, "executable", str(python)),
            patch.object(launcher.sys, "version_info", (3, 12, 0)),
            patch.dict("sys.modules", {"platformio": fake_pio}),
        ):
            observed = platformio_runtime()
        self.assertEqual([3, 12, 0], observed["python_version"])
        self.assertEqual("platformio", observed["platformio"])
        self.assertEqual("6.2.0", observed["platformio_version"])

    def test_defaults_preserve_all_nine_official_invocations(self):
        self.assertEqual(list(DEFAULT_INVOCATIONS), choose_invocations([], []))
        self.assertEqual(9, len(DEFAULT_INVOCATIONS))
        self.assertIn(
            ("native_test_gp8403_driver", ("test_gp8403_driver",)),
            DEFAULT_INVOCATIONS,
        )
        self.assertIn(
            ("native_test_startup_probe_policy", ("test_startup_probe_policy",)),
            DEFAULT_INVOCATIONS,
        )
        self.assertIn(
            (
                "native_test_i2c_4_3_profile",
                ("test_i2c_topology", "test_sensor_i2c_routing"),
            ),
            DEFAULT_INVOCATIONS,
        )
        self.assertIn(
            (
                "native_test_i2c_7_profile",
                ("test_i2c_topology", "test_sensor_i2c_routing"),
            ),
            DEFAULT_INVOCATIONS,
        )
        self.assertIn(
            ("native_test_ch422g_7_profile", ("test_ch422g_7_reset",)),
            DEFAULT_INVOCATIONS,
        )
        self.assertEqual([("native_test", (self.suite,))], choose_invocations([], [self.suite]))
        self.assertEqual(
            [("native_test_sfa30_driver", ("test_sfa30_driver",))],
            choose_invocations(["native_test_sfa30_driver"], []),
        )
        with self.assertRaisesRegex(NativeTestError, "only once"):
            choose_invocations(["native_test", "native_test"], [])

    def test_command_uses_official_pio_build_and_reader_with_fresh_json_report(self):
        report = self.project / "fresh-report.json"
        command = pio_command("pinned-python", self.project, "native_test",
                              (self.suite,), report, 3)
        self.assertEqual(["pinned-python", "-I", "-m", "platformio", "test"], command[:5])
        self.assertEqual(str(report), command[command.index("--json-output-path") + 1])
        self.assertEqual(self.suite, command[command.index("-f") + 1])
        self.assertEqual("-vvv", command[-1])
        self.assertFalse(any(argument.startswith("--without-") for argument in command))

    @unittest.skipUnless(os.name == "nt", "Official launcher uses a Windows OS file lock")
    def test_lock_rejects_overlap_and_is_reusable_after_release(self):
        path = self.project / "launcher.lock"
        with exclusive_launcher_lock(path):
            with self.assertRaisesRegex(NativeTestError, "Another native test launcher"):
                with exclusive_launcher_lock(path):
                    self.fail("Second launcher acquired the lock")
        with exclusive_launcher_lock(path):
            pass

    @unittest.skipUnless(os.name == "nt", "Official launcher uses a Windows OS file lock")
    def test_launcher_needs_both_successful_pio_exit_and_fresh_verified_report(self):
        (self.project / "platformio.ini").write_text("[env:native_test]\n", encoding="utf-8")
        runtime = {"python": str(self.project / "penv" / "Scripts" / "python.exe")}
        fake_config = SimpleNamespace(ProjectConfig=lambda path: object())

        for pio_exit, write_report, expected_exit in ((0, True, 0), (1, True, 1), (0, False, 1)):
            with self.subTest(pio_exit=pio_exit, write_report=write_report):
                def fake_pio(command, project, child_env):
                    self.assertEqual(project, self.project)
                    build = Path(child_env["PLATFORMIO_BUILD_DIR"])
                    self.assertTrue(build.is_relative_to(self.project / ".pio" / "native-tests"))
                    self.assertEqual(str(build / "cache"), child_env["PLATFORMIO_BUILD_CACHE_DIR"])
                    if write_report:
                        output = Path(command[command.index("--json-output-path") + 1])
                        self.assertFalse(output.exists(), "A previous PIO report must never be reused")
                        output.write_text(json.dumps(self.report), encoding="utf-8")
                    return pio_exit

                with (
                    patch.object(launcher, "__file__", str(self.project / "scripts" / "run_native_tests.py")),
                    patch.object(launcher, "platformio_runtime", return_value=runtime),
                    patch.object(launcher, "toolchain_identity", return_value={}),
                    patch.object(launcher, "selected_suites", return_value=self.expected),
                    patch.object(launcher, "run_pio", side_effect=fake_pio),
                    patch.object(launcher.os, "chdir"),
                    patch.dict("sys.modules", {"platformio.project.config": fake_config}),
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()),
                ):
                    self.assertEqual(expected_exit, launcher.main(["-f", self.suite]))

        manifests = [
            json.loads(path.read_text(encoding="utf-8"))
            for path in (self.project / ".pio" / "native-tests" / "reports").glob("*/launcher.json")
        ]
        self.assertEqual(["FAILED", "FAILED", "PASSED"], sorted(item["status"] for item in manifests))


if __name__ == "__main__":
    unittest.main()
