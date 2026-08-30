import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = PROJECT_ROOT / "scripts" / "release_identity.ps1"
PREPARE_SCRIPT_PATH = PROJECT_ROOT / "scripts" / "prepare_installer_release.ps1"


def ps_quote(value: Path | str) -> str:
    return "'" + str(value).replace("'", "''") + "'"


def init_repository(root: Path) -> str:
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    subprocess.run(
        ["git", "-C", str(root), "config", "user.email", "test@example.com"],
        check=True,
    )
    subprocess.run(
        ["git", "-C", str(root), "config", "user.name", "Test"],
        check=True,
    )
    (root / "tracked.txt").write_text("baseline\n", encoding="utf-8")
    subprocess.run(["git", "-C", str(root), "add", "tracked.txt"], check=True)
    subprocess.run(
        ["git", "-C", str(root), "commit", "-q", "-m", "baseline"],
        check=True,
    )
    return subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()


def run_identity(
    identity_path: Path,
    repository: Path,
    environment: str,
    *,
    process_environment: dict[str, str] | None = None,
):
    command = (
        f". {ps_quote(SCRIPT_PATH)}; "
        "try { "
        f"$value = Read-AuraBuildIdentity -IdentityPath {ps_quote(identity_path)} "
        f"-Environment {ps_quote(environment)} -RepositoryRoot {ps_quote(repository)}; "
        "$value | ConvertTo-Json -Compress "
        "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
    )
    return run_powershell(command, process_environment=process_environment)


def run_powershell(
    command: str, *, process_environment: dict[str, str] | None = None
):
    return subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            command,
        ],
        text=True,
        capture_output=True,
        encoding="utf-8",
        env=process_environment,
    )


def failing_status_environment(root: Path, commit: str) -> dict[str, str]:
    shim_directory = root / "fake-git"
    shim_directory.mkdir()
    (shim_directory / "git.cmd").write_text(
        "@echo off\r\n"
        "if \"%1\"==\"rev-parse\" (\r\n"
        f"  echo {commit}\r\n"
        "  exit /b 0\r\n"
        ")\r\n"
        "if \"%1\"==\"status\" exit /b 23\r\n"
        "exit /b 1\r\n",
        encoding="ascii",
    )
    environment = os.environ.copy()
    environment["PATH"] = str(shim_directory) + os.pathsep + environment["PATH"]
    return environment


def find_node() -> str | None:
    candidates = [
        shutil.which("node"),
        str(Path(os.environ.get("ProgramFiles", "")) / "nodejs" / "node.exe"),
        str(
            Path(os.environ.get("USERPROFILE", ""))
            / ".cache"
            / "codex-runtimes"
            / "codex-primary-runtime"
            / "dependencies"
            / "node"
            / "bin"
            / "node.exe"
        ),
    ]
    return next((candidate for candidate in candidates if candidate and Path(candidate).is_file()), None)


class ReleaseIdentityScriptTests(unittest.TestCase):
    def write_identity(
        self,
        path: Path,
        *,
        commit: str,
        environment: str = "project_aura",
        profile: str = "4_3",
        target: str = "aura-aq-v1",
        build_id: str | None = None,
    ) -> None:
        suffix = "-7-dual-i2c-scl6" if environment == "project_aura_7" else ""
        path.write_text(
            json.dumps(
                {
                    "schema": "project-aura.build-identity.v1",
                    "environment": environment,
                    "source_commit": commit,
                    "build_id": build_id or f"{commit[:7]}{suffix}",
                    "hardware_profile": profile,
                    "hardware_target": target,
                    "partitions_file": "partitions_16MB_littlefs.csv",
                }
            ),
            encoding="utf-8",
        )

    def test_accepts_each_clean_production_identity(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repository = root / "repo"
            repository.mkdir()
            commit = init_repository(repository)

            legacy = root / "legacy.json"
            self.write_identity(legacy, commit=commit)
            legacy_result = run_identity(legacy, repository, "project_aura")
            self.assertEqual(legacy_result.returncode, 0, legacy_result.stderr)
            self.assertEqual(json.loads(legacy_result.stdout)["HardwareTarget"], "aura-aq-v1")

            seven = root / "seven.json"
            self.write_identity(
                seven,
                commit=commit,
                environment="project_aura_7",
                profile="7_dual_i2c_scl6",
                target="aura-aq-7-v1",
            )
            seven_result = run_identity(seven, repository, "project_aura_7")
            self.assertEqual(seven_result.returncode, 0, seven_result.stderr)
            self.assertEqual(json.loads(seven_result.stdout)["ArtifactSlug"], "7")

    def test_rejects_cross_profile_and_missing_fields(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repository = root / "repo"
            repository.mkdir()
            commit = init_repository(repository)

            cross_pair = root / "cross-pair.json"
            self.write_identity(
                cross_pair,
                commit=commit,
                environment="project_aura_7",
                profile="7_dual_i2c_scl6",
                target="aura-aq-v1",
            )
            result = run_identity(cross_pair, repository, "project_aura_7")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("does not match", result.stderr)

            missing = root / "missing.json"
            missing.write_text('{"schema":"project-aura.build-identity.v1"}', encoding="utf-8")
            result = run_identity(missing, repository, "project_aura")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing required property", result.stderr)

    def test_rejects_clean_identity_after_source_tree_becomes_dirty(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repository = root / "repo"
            repository.mkdir()
            commit = init_repository(repository)
            identity = root / "identity.json"
            self.write_identity(identity, commit=commit)

            (repository / "untracked.txt").write_text("dirty\n", encoding="utf-8")
            result = run_identity(identity, repository, "project_aura")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("expected=", result.stderr)
            self.assertIn("-dirty", result.stderr)

    def test_git_status_failure_is_never_treated_as_clean(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repository = root / "repo"
            repository.mkdir()
            commit = init_repository(repository)
            identity = root / "identity.json"
            self.write_identity(identity, commit=commit)
            environment = failing_status_environment(root, commit)

            result = run_identity(
                identity,
                repository,
                "project_aura",
                process_environment=environment,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Could not inspect Git working tree state", result.stderr)

    def test_prepare_release_rejects_git_status_failure_before_signing(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            commit = "0123456789abcdef0123456789abcdef01234567"
            environment = failing_status_environment(root, commit)
            node_path = find_node()
            self.assertIsNotNone(node_path)
            result = subprocess.run(
                [
                    "powershell.exe",
                    "-NoProfile",
                    "-NonInteractive",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(PREPARE_SCRIPT_PATH),
                    "-Version",
                    "1.1.5",
                    "-KeyDirectory",
                    str(root / "missing-key"),
                    "-NodePath",
                    str(node_path),
                ],
                text=True,
                capture_output=True,
                encoding="utf-8",
                env=environment,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Could not inspect Git working tree state", result.stderr)

    def test_release_channel_and_version_shapes_are_bound(self):
        invalid_stable = run_powershell(
            f". {ps_quote(SCRIPT_PATH)}; try {{ "
            "Assert-AuraReleaseChannelVersion -Channel 'stable' -Version '1.1.6-beta' "
            "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
        )
        self.assertNotEqual(invalid_stable.returncode, 0)
        self.assertIn("Stable releases require", invalid_stable.stderr)

        invalid_beta = run_powershell(
            f". {ps_quote(SCRIPT_PATH)}; try {{ "
            "Assert-AuraReleaseChannelVersion -Channel 'beta' -Version '1.1.6' "
            "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
        )
        self.assertNotEqual(invalid_beta.returncode, 0)
        self.assertIn("Beta releases require", invalid_beta.stderr)

        invalid_effective_version = run_powershell(
            f". {ps_quote(SCRIPT_PATH)}; try {{ "
            "Get-AuraEffectiveVersion -Version '1.1.6-beta' -BuildId '0123456_bad' "
            "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
        )
        self.assertNotEqual(invalid_effective_version.returncode, 0)
        self.assertIn("Invalid effective release version", invalid_effective_version.stderr)

    def test_post_build_stamp_detects_changed_binary(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repository = root / "repo"
            repository.mkdir()
            commit = init_repository(repository)
            identity = root / "identity.json"
            self.write_identity(identity, commit=commit)

            build = root / "build"
            build.mkdir()
            for name in ("bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin"):
                (build / name).write_bytes((name + "\n").encode())
            boot_app0 = root / "boot_app0.bin"
            boot_app0.write_bytes(b"boot-app0\n")
            stamp = root / "release-artifacts.json"

            setup = (
                f". {ps_quote(SCRIPT_PATH)}; try {{ "
                f"$identity = Read-AuraBuildIdentity -IdentityPath {ps_quote(identity)} "
                f"-Environment 'project_aura' -RepositoryRoot {ps_quote(repository)}; "
                f"$inputs = Get-AuraArtifactInputs -BuildDirectory {ps_quote(build)} "
                f"-BootApp0Path {ps_quote(boot_app0)}; "
                f"Write-AuraReleaseArtifactStamp -StampPath {ps_quote(stamp)} "
                "-Identity $identity -ArtifactInputs $inputs; "
                f"Read-AuraReleaseArtifactStamp -StampPath {ps_quote(stamp)} "
                "-Identity $identity -ArtifactInputs $inputs | ConvertTo-Json -Compress "
                "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
            )
            result = run_powershell(setup)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["files"][3]["file_name"], "firmware.bin")

            (build / "firmware.bin").write_bytes(b"changed\n")
            verify = (
                f". {ps_quote(SCRIPT_PATH)}; try {{ "
                f"$identity = Read-AuraBuildIdentity -IdentityPath {ps_quote(identity)} "
                f"-Environment 'project_aura' -RepositoryRoot {ps_quote(repository)}; "
                f"$inputs = Get-AuraArtifactInputs -BuildDirectory {ps_quote(build)} "
                f"-BootApp0Path {ps_quote(boot_app0)}; "
                f"Read-AuraReleaseArtifactStamp -StampPath {ps_quote(stamp)} "
                "-Identity $identity -ArtifactInputs $inputs | Out-Null "
                "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
            )
            result = run_powershell(verify)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("changed after the successful build", result.stderr)

    def test_published_asset_is_immutable_but_identical_retry_is_allowed(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact = Path(temp_dir) / "firmware.bin"
            artifact.write_bytes(b"immutable firmware\n")
            digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
            valid = (
                f". {ps_quote(SCRIPT_PATH)}; try {{ "
                f"$asset = [pscustomobject]@{{size={artifact.stat().st_size};digest='sha256:{digest}'}}; "
                f"Assert-AuraPublishedAssetIsIdentical -Asset $asset -LocalPath {ps_quote(artifact)} "
                "-AssetName 'firmware.bin'; 'ok' "
                "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
            )
            result = run_powershell(valid)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("ok", result.stdout)

            changed = (
                f". {ps_quote(SCRIPT_PATH)}; try {{ "
                f"$asset = [pscustomobject]@{{size={artifact.stat().st_size};digest='sha256:{'0' * 64}'}}; "
                f"Assert-AuraPublishedAssetIsIdentical -Asset $asset -LocalPath {ps_quote(artifact)} "
                "-AssetName 'firmware.bin' "
                "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
            )
            result = run_powershell(changed)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("published assets are immutable", result.stderr)


if __name__ == "__main__":
    unittest.main()
