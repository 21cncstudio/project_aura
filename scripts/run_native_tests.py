"""Run native Unity tests through the canonical Windows PlatformIO installation.

Use scripts/run_tests.ps1, optionally with -Environment and -Filter. The normal
PlatformIO build and NativeTestOutputReader still run every executable. A fresh
JSON report must identify actual Unity source paths for every selected suite.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid


DEFAULT_INVOCATIONS = (
    ("native_test", ()),
    ("native_test_sfa30_driver", ("test_sfa30_driver",)),
    ("native_test_sfa40_driver", ("test_sfa40_driver",)),
    ("native_test_dfr_optional_gas_driver", ("test_dfr_optional_gas_driver",)),
    ("native_test_gp8403_driver", ("test_gp8403_driver",)),
    ("native_test_i2c_4_3_profile", ("test_i2c_topology", "test_sensor_i2c_routing")),
    ("native_test_i2c_7_profile", ("test_i2c_topology", "test_sensor_i2c_routing")),
    ("native_test_ch422g_4_3_profile", ("test_ch422g_reset", "test_ch422g_ready_probe")),
    ("native_test_ch422g_7_profile", ("test_ch422g_reset", "test_ch422g_ready_probe")),
    ("native_test_startup_probe_policy", ("test_startup_probe_policy",)),
)
RUNTIME_DLLS = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")


class NativeTestError(RuntimeError):
    pass


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fingerprint(value: object) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True).encode("utf-8")
    ).hexdigest()[:16]


def platformio_runtime() -> dict:
    import platformio

    python = Path.home() / ".platformio" / "penv" / "Scripts" / "python.exe"
    if os.name != "nt" or Path(sys.executable).resolve() != python.resolve():
        raise NativeTestError(
            f"Use scripts/run_tests.ps1 with the canonical penv interpreter: {python}"
        )
    module_path = Path(platformio.__file__).resolve()
    if not module_path.is_relative_to(python.parent.parent.resolve()):
        raise NativeTestError(f"PlatformIO loaded outside the canonical penv: {module_path}")
    return {
        "python": str(python.resolve()),
        "python_version": list(sys.version_info[:3]),
        "platformio": platformio.__title__,
        "platformio_version": platformio.__version__,
        "platformio_module": str(module_path),
    }


def compiler_first_environment(base: dict, compiler_dir: Path) -> dict:
    result = dict(base)
    result["PATH"] = str(compiler_dir) + os.pathsep + base.get("PATH", "")
    # The PIO subprocess and its SCons children must not import a user-site core.
    result.pop("PYTHONPATH", None)
    result.pop("PYTHONHOME", None)
    result["PYTHONNOUSERSITE"] = "1"
    return result


def toolchain_identity(compiler_dir: Path, child_env: dict) -> dict:
    files = ("gcc.exe", "g++.exe", *RUNTIME_DLLS)
    for name in files:
        if not (compiler_dir / name).is_file():
            raise NativeTestError(f"Missing supported MinGW toolchain file: {compiler_dir / name}")
    versions = {}
    for name in ("gcc.exe", "g++.exe"):
        compiler = str(compiler_dir / name)
        target = subprocess.check_output(
            [compiler, "-dumpmachine"], env=child_env, text=True, timeout=10
        ).strip()
        versions[name] = subprocess.check_output(
            [compiler, "-dumpfullversion"], env=child_env, text=True, timeout=10
        ).strip()
        if target != "x86_64-w64-mingw32":
            raise NativeTestError(f"Unsupported native compiler target: {target}")
    if versions["gcc.exe"] != versions["g++.exe"]:
        raise NativeTestError("gcc and g++ versions do not match")
    return {
        "bin": str(compiler_dir),
        "version": versions["g++.exe"],
        "files": {name: file_sha256(compiler_dir / name) for name in files},
    }


def choose_invocations(environments: list[str], filters: list[str]) -> list[tuple]:
    if not environments:
        return [("native_test", tuple(filters))] if filters else list(DEFAULT_INVOCATIONS)
    defaults = dict(DEFAULT_INVOCATIONS)
    if len(environments) != len(set(environments)):
        raise NativeTestError("Select each environment only once")
    return [
        (environment, tuple(filters) or defaults.get(environment, ()))
        for environment in environments
    ]


def selected_suites(config, environment: str, filters: tuple) -> dict:
    from platformio.test.helpers import list_test_suites

    if environment not in config.envs():
        raise NativeTestError(f"Unknown environment: {environment}")
    section = f"env:{environment}"
    if config.get(section, "platform") != "native" or config.get(
        section, "test_framework"
    ) != "unity":
        raise NativeTestError(f"Only native Unity environments are allowed: {environment}")
    expected = {
        (suite.env_name, suite.test_name): Path(suite.test_dir).resolve()
        for suite in list_test_suites(config, [environment], filters, [])
        if not suite.is_finished()
    }
    if not expected:
        raise NativeTestError(f"No suites selected for {environment}: {filters}")
    return expected


def isolated_build_dir(project: Path, identity: dict, expected: dict) -> Path:
    # One target per runtime/toolchain and exact selection. Sequential suites in
    # a full run share compiled project objects, but never another selection's
    # SCons database or program.exe.
    return (
        project / ".pio" / "native-tests"
        / ("runtime-" + fingerprint(identity))
        / ("selection-" + fingerprint(sorted(expected)))
    )


def validate_report(report: dict, project: Path, expected: dict) -> dict:
    if report.get("version") != "1.0" or Path(
        report.get("project_dir", "")
    ).resolve() != project.resolve():
        raise NativeTestError("Missing or mismatched PlatformIO report identity")
    suites = report.get("test_suites")
    if not isinstance(suites, list):
        raise NativeTestError("PlatformIO report has no test_suites list")
    seen = set()
    counts = {}
    for suite in suites:
        key = (suite.get("env_name"), suite.get("test_name"))
        cases = suite.get("test_cases")
        if not isinstance(cases, list):
            raise NativeTestError(f"Missing case records for {key}")
        if key in seen:
            raise NativeTestError(f"Duplicate suite report: {key}")
        seen.add(key)
        if key not in expected:
            if cases or suite.get("timestamp") or suite.get("status") != "SKIPPED":
                raise NativeTestError(f"Unexpected active suite: {key}")
            continue
        if suite.get("status") != "PASSED" or not cases:
            raise NativeTestError(f"Selected suite did not pass with real cases: {key}")
        if suite.get("testcase_nums") != len(cases):
            raise NativeTestError(f"Inconsistent case count for {key}")
        passed = 0
        for case in cases:
            if case.get("status") not in ("PASSED", "SKIPPED"):
                raise NativeTestError(f"Non-passing Unity case in {key}: {case.get('name')}")
            source = case.get("source") or {}
            filename = source.get("file")
            if not isinstance(filename, str) or not filename:
                raise NativeTestError(f"Unity case has no source path in {key}")
            actual_file = (project / Path(filename.replace("\\", "/"))).resolve()
            if not actual_file.is_relative_to(expected[key]) or not actual_file.is_file():
                raise NativeTestError(
                    f"Wrong Unity suite: selected {key}, actual source {filename}"
                )
            passed += case["status"] == "PASSED"
        if not passed:
            raise NativeTestError(f"No passed Unity cases for selected suite: {key}")
        counts[f"{key[0]}:{key[1]}"] = len(cases)
    missing = set(expected) - seen
    if missing:
        raise NativeTestError(f"Selected suites missing from report: {sorted(missing)}")
    if report.get("error_nums") != 0 or report.get("failure_nums") != 0:
        raise NativeTestError("PlatformIO reported failed or errored cases")
    if report.get("testcase_nums") != sum(counts.values()):
        raise NativeTestError("PlatformIO total case count does not match selected suites")
    return counts


@contextmanager
def exclusive_launcher_lock(path: Path):
    # The OS releases the lock after an interrupted/crashed launcher. Keep the
    # file rather than deleting a pathname another launcher may already hold.
    import msvcrt

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        if stream.tell() == 0:
            stream.write(b"\0")
            stream.flush()
        stream.seek(0)
        try:
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise NativeTestError("Another native test launcher is running for this project") from error
        try:
            yield
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def pio_command(python: str, project: Path, environment: str, filters: tuple,
                report_path: Path, verbosity: int) -> list[str]:
    command = [
        python, "-I", "-m", "platformio", "test",
        "--project-dir", str(project), "--project-conf", str(project / "platformio.ini"),
        "-e", environment, "--json-output-path", str(report_path),
    ]
    for pattern in filters:
        command.extend(("-f", pattern))
    if verbosity:
        command.append("-" + "v" * verbosity)
    return command


def run_pio(command: list[str], project: Path, child_env: dict) -> int:
    # Inherit stdout/stderr so this remains a normal, visible PlatformIO run.
    with subprocess.Popen(command, cwd=project, env=child_env) as process:
        try:
            return process.wait()
        except KeyboardInterrupt:
            print("Interrupted; waiting for PlatformIO to stop before releasing its lock.", flush=True)
            process.wait()
            raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-e", "--environment", action="append", default=[])
    parser.add_argument("-f", "--filter", action="append", default=[])
    parser.add_argument("--compiler-dir", type=Path, default=Path("C:/msys64/mingw64/bin"))
    parser.add_argument("-v", "--verbose", action="count", default=0)
    args = parser.parse_args(argv)
    project = Path(__file__).resolve().parents[1]
    manifest_path = None
    manifest = {"status": "FAILED", "invocations": []}
    try:
        if args.verbose > 3:
            raise NativeTestError("Verbosity must be between 0 and 3")
        if not (project / "platformio.ini").is_file():
            raise NativeTestError(f"Project configuration missing: {project / 'platformio.ini'}")
        runtime = platformio_runtime()
        compiler_dir = args.compiler_dir.resolve()
        child_env = compiler_first_environment(dict(os.environ), compiler_dir)
        child_env["PLATFORMIO_CORE_DIR"] = str(Path(runtime["python"]).parents[2])
        identity = {"runtime": runtime, "toolchain": toolchain_identity(compiler_dir, child_env)}
        invocations = choose_invocations(args.environment, args.filter)
        os.chdir(project)
        from platformio.project.config import ProjectConfig
        config = ProjectConfig(str(project / "platformio.ini"))
        selections = [
            selected_suites(config, environment, filters)
            for environment, filters in invocations
        ]
        with exclusive_launcher_lock(project / ".pio" / "native-tests" / "launcher.lock"):
            run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:8]
            reports = project / ".pio" / "native-tests" / "reports" / run_id
            reports.mkdir(parents=True)
            manifest_path = reports / "launcher.json"
            manifest.update(identity=identity, project_dir=str(project), run_id=run_id)
            print(f"Native runtime: {runtime['python']}", flush=True)
            print(f"Compiler and DLL PATH first: {compiler_dir}", flush=True)
            print(f"Native evidence: {reports}", flush=True)
            for index, ((environment, filters), expected) in enumerate(zip(invocations, selections)):
                build_dir = isolated_build_dir(project, identity, expected)
                run_env = dict(child_env, PLATFORMIO_BUILD_DIR=str(build_dir),
                               PLATFORMIO_BUILD_CACHE_DIR=str(build_dir / "cache"))
                report_path = reports / f"{index:02d}-pio.json"
                command = pio_command(runtime["python"], project, environment,
                                      filters, report_path, args.verbose)
                entry = {"command": command, "build_dir": str(build_dir),
                         "selected_suites": sorted(expected), "report": str(report_path)}
                manifest["invocations"].append(entry)
                print(f"Native build directory: {build_dir}", flush=True)
                entry["exit_code"] = run_pio(command, project, run_env)
                if entry["exit_code"] != 0:
                    raise NativeTestError(f"PlatformIO failed with exit code {entry['exit_code']}")
                report = json.loads(report_path.read_text(encoding="utf-8"))
                entry["verified_cases"] = validate_report(report, project, expected)
                print(f"Verified Unity source identity for {len(expected)} suite(s).", flush=True)
        manifest["status"] = "PASSED"
        return 0
    except (NativeTestError, OSError, ValueError, ImportError, subprocess.SubprocessError) as error:
        manifest["error"] = str(error)
        print(f"Native test launcher: {error}", file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        manifest["error"] = "Interrupted"
        return 130
    finally:
        if manifest_path:
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
