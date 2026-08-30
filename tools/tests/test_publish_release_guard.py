import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = PROJECT_ROOT / "scripts" / "publish_github_release.ps1"
SOURCE_COMMIT = "0123456789abcdef0123456789abcdef01234567"


def create_release_directory(root: Path, *, target: str = "aura-aq-v1", build_id: str = "0123456"):
    profile = "4_3" if target == "aura-aq-v1" else "7_dual_i2c_scl6"
    environment = "project_aura" if target == "aura-aq-v1" else "project_aura_7"
    slug = "4_3" if target == "aura-aq-v1" else "7"
    ota_name = f"project_aura_{slug}_1.1.6_ota_firmware.bin"
    firmware = b"firmware fixture\n"
    (root / ota_name).write_bytes(firmware)
    digest = hashlib.sha256(firmware).hexdigest()
    (root / "release-identity.json").write_text(
        json.dumps(
            {
                "schema": "project-aura.build-identity.v1",
                "environment": environment,
                "source_commit": SOURCE_COMMIT,
                "build_id": build_id,
                "hardware_profile": profile,
                "hardware_target": target,
                "artifact_slug": slug,
                "partitions_file": "partitions_16MB_littlefs.csv",
            }
        ),
        encoding="utf-8",
    )
    (root / "release-artifacts.json").write_text(
        json.dumps(
            {
                "schema": "project-aura.release-artifacts.v1",
                "environment": environment,
                "source_commit": SOURCE_COMMIT,
                "build_id": build_id,
                "hardware_profile": profile,
                "hardware_target": target,
                "files": [
                    {
                        "file_name": "firmware.bin",
                        "size_bytes": len(firmware),
                        "sha256": digest,
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    manifest = {
        "version": "1.1.6",
        "hardware_target": target,
        "hardware_profile": profile,
        "build_id": build_id,
    }
    (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    (root / "manifest-update.json").write_text(json.dumps(manifest), encoding="utf-8")
    (root / "sha256sums.txt").write_text(f"{digest}  {ota_name}\n", encoding="ascii")
    return ota_name


def invoke_publisher(assets: Path, *extra: str, env=None):
    return subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(SCRIPT_PATH),
            "-Version",
            "1.1.6",
            "-AssetsDir",
            str(assets),
            *extra,
        ],
        text=True,
        capture_output=True,
        encoding="utf-8",
        env=env,
    )


def create_fake_github_environment(
    root: Path,
    *,
    scenario: str,
    asset_name: str,
    asset_size: int,
    asset_digest: str,
):
    fake_bin = root / "fake-bin"
    fake_bin.mkdir()
    log_path = root / "gh-calls.jsonl"
    server_path = root / "fake-gh.py"
    server_path.write_text(
        r'''import json
import os
from pathlib import Path
import sys


args = sys.argv[1:]
scenario = os.environ["AURA_FAKE_GH_SCENARIO"]
source_commit = os.environ["AURA_FAKE_GH_SOURCE_COMMIT"]
asset_name = os.environ["AURA_FAKE_GH_ASSET_NAME"]
asset_size = int(os.environ["AURA_FAKE_GH_ASSET_SIZE"])
asset_digest = os.environ["AURA_FAKE_GH_ASSET_DIGEST"]
event = {"args": args}
if "--input" in args:
    payload_path = Path(args[args.index("--input") + 1])
    event["payload"] = json.loads(payload_path.read_text(encoding="utf-8-sig"))
with Path(os.environ["AURA_FAKE_GH_LOG"]).open("a", encoding="utf-8") as handle:
    handle.write(json.dumps(event, sort_keys=True) + "\n")


def emit(value):
    print(json.dumps(value, separators=(",", ":")))


if args[:2] == ["release", "view"]:
    if scenario.startswith("existing_"):
        emit(
            {
                "databaseId": 101,
                "url": "https://example.invalid/release/101",
                "name": "fixture",
                "tagName": "v1.1.6",
                "isDraft": True,
            }
        )
        raise SystemExit(0)
    print("release not found", file=sys.stderr)
    raise SystemExit(1)

if args and args[0] == "api":
    method = args[args.index("--method") + 1]
    path = next(item for item in args if item.startswith("repos/"))
    if method == "GET" and path == "repos/21cncstudio/project_aura":
        full_name = "attacker/repository" if scenario == "repository_mismatch" else "21cncstudio/project_aura"
        emit({"full_name": full_name})
        raise SystemExit(0)
    if method == "GET" and "/releases/tags/" in path:
        if scenario.startswith("existing_"):
            emit(
                {
                    "id": 101,
                    "html_url": "https://example.invalid/release/101",
                    "name": "fixture",
                    "tag_name": "v1.1.6",
                    "draft": True,
                    "target_commitish": source_commit,
                }
            )
            raise SystemExit(0)
        print("HTTP 404: Not Found", file=sys.stderr)
        raise SystemExit(1)
    if method == "GET" and "/releases?per_page=100" in path:
        if scenario.startswith("resume_draft"):
            target_commitish = "f" * 40 if scenario == "resume_draft_wrong_source" else source_commit
            emit(
                [
                    [
                        {
                            "id": 303,
                            "html_url": "https://example.invalid/release/303",
                            "name": "fixture draft",
                            "tag_name": "v1.1.6",
                            "draft": True,
                            "target_commitish": target_commitish,
                        }
                    ]
                ]
            )
        else:
            emit([[]])
        raise SystemExit(0)
    if "/git/ref/tags/" in path:
        if scenario.startswith("new_") or scenario.startswith("resume_draft"):
            print("HTTP 404: Not Found", file=sys.stderr)
            raise SystemExit(1)
        sha = "f" * 40 if scenario == "existing_wrong_commit" else source_commit
        emit({"ref": "refs/tags/v1.1.6", "object": {"type": "commit", "sha": sha}})
        raise SystemExit(0)
    if method == "GET" and "/assets" in path:
        if scenario == "existing_conflict":
            emit(
                [
                    [
                        {
                            "name": asset_name,
                            "size": asset_size,
                            "digest": "sha256:" + ("0" * 64),
                        }
                    ]
                ]
            )
        elif scenario == "existing_duplicate_pages":
            duplicate = {
                "name": asset_name,
                "size": asset_size,
                "digest": "sha256:" + asset_digest,
            }
            emit([[duplicate], [duplicate]])
        elif scenario == "existing_identical":
            emit(
                [
                    [
                        {
                            "name": asset_name,
                            "size": asset_size,
                            "digest": "sha256:" + asset_digest,
                        }
                    ]
                ]
            )
        else:
            emit([[]])
        raise SystemExit(0)
    if method == "POST" and path.endswith("/releases"):
        emit({"id": 202, "html_url": "https://example.invalid/release/202"})
        raise SystemExit(0)
    if method == "PATCH" and "/releases/" in path:
        release_id = int(path.rsplit("/", 1)[-1])
        emit({"id": release_id,
              "html_url": "https://example.invalid/release/final"})
        raise SystemExit(0)

if args[:2] == ["release", "upload"]:
    if scenario == "new_upload_failure":
        print("simulated upload failure", file=sys.stderr)
        raise SystemExit(1)
    raise SystemExit(0)

print("unexpected fake gh command: " + repr(args), file=sys.stderr)
raise SystemExit(97)
''',
        encoding="utf-8",
    )
    (fake_bin / "gh.cmd").write_text(
        f'@echo off\r\n"{sys.executable}" "{server_path}" %*\r\n',
        encoding="ascii",
    )
    (fake_bin / "git.cmd").write_text(
        '@echo off\r\n'
        'if "%1"=="credential" (\r\n'
        '  echo username=fake-user\r\n'
        '  echo password=fake-token\r\n'
        '  exit /b 0\r\n'
        ')\r\n'
        'exit /b 1\r\n',
        encoding="ascii",
    )
    environment = os.environ.copy()
    environment["PATH"] = str(fake_bin) + os.pathsep + environment["PATH"]
    environment["AURA_FAKE_GH_SCENARIO"] = scenario
    environment["AURA_FAKE_GH_LOG"] = str(log_path)
    environment["AURA_FAKE_GH_SOURCE_COMMIT"] = SOURCE_COMMIT
    environment["AURA_FAKE_GH_ASSET_NAME"] = asset_name
    environment["AURA_FAKE_GH_ASSET_SIZE"] = str(asset_size)
    environment["AURA_FAKE_GH_ASSET_DIGEST"] = asset_digest
    return environment, log_path


def read_fake_calls(log_path: Path):
    if not log_path.exists():
        return []
    return [json.loads(line) for line in log_path.read_text(encoding="utf-8").splitlines()]


def is_api_call(call, method: str, path_suffix: str):
    args = call["args"]
    path = (
        next(item for item in args if item.startswith("repos/"))
        if args and args[0] == "api"
        else ""
    )
    return (
        args
        and args[0] == "api"
        and args[args.index("--method") + 1] == method
        and path.split("?", 1)[0].endswith(path_suffix)
    )


class PublishReleaseGuardTests(unittest.TestCase):
    def test_legacy_transport_keeps_source_check_and_final_patch_order(self):
        source = SCRIPT_PATH.read_text(encoding="utf-8")
        execution_start = source.index('Write-Step "Checking existing release by tag: $Tag"')
        source_check = source.index(
            '$remoteTagCommit = Get-TagSourceCommitViaRest', execution_start
        )
        legacy_start = source.rindex('$isNewRelease = $null -eq $release')
        legacy_flow = source[legacy_start:]
        create_draft = legacy_flow.index('Creating draft release $Tag')
        asset_preflight = legacy_flow.index('Preflighting existing release assets')
        asset_upload = legacy_flow.index('Invoke-CurlUpload')
        final_patch = legacy_flow.index('Finalizing release metadata')
        self.assertLess(source_check, legacy_start)
        self.assertLess(create_draft, asset_preflight)
        self.assertLess(asset_preflight, asset_upload)
        self.assertLess(asset_upload, final_patch)

    def test_seven_inch_external_publication_is_disabled_before_network(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            assets = Path(temp_dir)
            create_release_directory(
                assets,
                target="aura-aq-7-v1",
                build_id="0123456-7-dual-i2c-scl6",
            )
            result = invoke_publisher(assets, "-HardwareTarget", "aura-aq-7-v1")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("disabled until the 7-inch profile", result.stderr)

    def test_rejects_cross_target_identity_before_network(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            assets = Path(temp_dir)
            create_release_directory(
                assets,
                target="aura-aq-7-v1",
                build_id="0123456-7-dual-i2c-scl6",
            )
            result = invoke_publisher(assets)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("does not match selected hardware target", result.stderr)

    def test_rejects_dirty_build_identity_before_network(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            assets = Path(temp_dir)
            create_release_directory(assets, build_id="0123456-dirty")
            result = invoke_publisher(assets)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Only a clean, commit-bound", result.stderr)

    def test_rejects_hash_drift_and_pruning_before_network(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            assets = Path(temp_dir)
            ota_name = create_release_directory(assets)
            (assets / ota_name).write_bytes(b"changed\n")
            result = invoke_publisher(assets)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("hash does not match", result.stderr)

        with tempfile.TemporaryDirectory() as temp_dir:
            assets = Path(temp_dir)
            create_release_directory(assets)
            result = invoke_publisher(assets, "-PruneAssetsToList")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("disabled for target-aware releases", result.stderr)

    def run_fake_scenario(self, scenario: str):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        assets = root / "assets"
        assets.mkdir()
        ota_name = create_release_directory(assets)
        ota_path = assets / ota_name
        environment, log_path = create_fake_github_environment(
            root,
            scenario=scenario,
            asset_name=ota_name,
            asset_size=ota_path.stat().st_size,
            asset_digest=hashlib.sha256(ota_path.read_bytes()).hexdigest(),
        )
        result = invoke_publisher(assets, env=environment)
        return result, read_fake_calls(log_path)

    def test_existing_tag_source_mismatch_stops_before_remote_mutation(self):
        result, calls = self.run_fake_scenario("existing_wrong_commit")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not release source commit", result.stderr)
        self.assertFalse(any(is_api_call(call, "PATCH", "/releases/101") for call in calls))
        self.assertFalse(any(call["args"][:2] == ["release", "upload"] for call in calls))

    def test_repository_identity_mismatch_stops_before_release_lookup_or_mutation(self):
        result, calls = self.run_fake_scenario("repository_mismatch")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("repository identity mismatch", result.stderr)
        self.assertEqual(len(calls), 1)
        self.assertFalse(any(is_api_call(call, "POST", "/releases") for call in calls))

    def test_existing_asset_conflict_is_detected_before_patch_or_upload(self):
        result, calls = self.run_fake_scenario("existing_conflict")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cannot be proven identical", result.stderr)
        assets_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "GET", "/releases/101/assets")
        )
        self.assertGreater(assets_index, 0)
        self.assertFalse(any(is_api_call(call, "PATCH", "/releases/101") for call in calls))
        self.assertFalse(any(call["args"][:2] == ["release", "upload"] for call in calls))

    def test_existing_release_uploads_only_after_preflight_and_patches_last(self):
        result, calls = self.run_fake_scenario("existing_success")
        self.assertEqual(result.returncode, 0, result.stderr)
        assets_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "GET", "/releases/101/assets")
        )
        upload_index = next(
            index
            for index, call in enumerate(calls)
            if call["args"][:2] == ["release", "upload"]
        )
        patch_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "PATCH", "/releases/101")
        )
        self.assertLess(assets_index, upload_index)
        self.assertLess(upload_index, patch_index)
        self.assertEqual(patch_index, len(calls) - 1)

    def test_existing_identical_asset_is_not_reuploaded(self):
        result, calls = self.run_fake_scenario("existing_identical")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any(call["args"][:2] == ["release", "upload"] for call in calls))
        self.assertTrue(any(is_api_call(call, "PATCH", "/releases/101") for call in calls))

    def test_source_bound_draft_is_resumed_without_creating_a_duplicate(self):
        result, calls = self.run_fake_scenario("resume_draft_success")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any(is_api_call(call, "POST", "/releases") for call in calls))
        assets_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "GET", "/releases/303/assets")
        )
        upload_index = next(
            index
            for index, call in enumerate(calls)
            if call["args"][:2] == ["release", "upload"]
        )
        patch_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "PATCH", "/releases/303")
        )
        self.assertLess(assets_index, upload_index)
        self.assertLess(upload_index, patch_index)

    def test_draft_with_wrong_target_commit_is_rejected_before_mutation(self):
        result, calls = self.run_fake_scenario("resume_draft_wrong_source")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not a source-bound draft", result.stderr)
        self.assertFalse(any(is_api_call(call, "POST", "/releases") for call in calls))
        self.assertFalse(any(is_api_call(call, "PATCH", "/releases/303") for call in calls))
        self.assertFalse(any(call["args"][:2] == ["release", "upload"] for call in calls))

    def test_duplicate_asset_names_across_pages_fail_before_mutation(self):
        result, calls = self.run_fake_scenario("existing_duplicate_pages")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate assets", result.stderr)
        self.assertFalse(any(is_api_call(call, "PATCH", "/releases/101") for call in calls))
        self.assertFalse(any(call["args"][:2] == ["release", "upload"] for call in calls))

    def test_new_release_stays_draft_when_upload_fails(self):
        result, calls = self.run_fake_scenario("new_upload_failure")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("simulated upload failure", result.stderr)
        create_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "POST", "/releases")
        )
        self.assertIs(calls[create_index]["payload"]["draft"], True)
        self.assertFalse(any(is_api_call(call, "PATCH", "/releases/202") for call in calls))

    def test_new_release_is_created_draft_uploaded_then_finalized(self):
        result, calls = self.run_fake_scenario("new_success")
        self.assertEqual(result.returncode, 0, result.stderr)
        create_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "POST", "/releases")
        )
        assets_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "GET", "/releases/202/assets")
        )
        upload_index = next(
            index
            for index, call in enumerate(calls)
            if call["args"][:2] == ["release", "upload"]
        )
        patch_index = next(
            index
            for index, call in enumerate(calls)
            if is_api_call(call, "PATCH", "/releases/202")
        )
        self.assertIs(calls[create_index]["payload"]["draft"], True)
        self.assertIs(calls[patch_index]["payload"]["draft"], False)
        self.assertLess(create_index, assets_index)
        self.assertLess(assets_index, upload_index)
        self.assertLess(upload_index, patch_index)
        self.assertEqual(patch_index, len(calls) - 1)


if __name__ == "__main__":
    unittest.main()
