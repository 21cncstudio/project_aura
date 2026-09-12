"""Exercise native-IDF release preparation without touching real signing keys."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_release_identity import init_repository, ps_quote, run_powershell


PROJECT = Path(__file__).resolve().parents[2]


class NativeReleaseAssetsTests(unittest.TestCase):
    def prepare(self, root, profile):
        environment = "project_aura" if profile == "4_3" else "project_aura_7"
        target = "aura-aq-v1" if profile == "4_3" else "aura-aq-7-v1"
        scripts = root / "scripts"
        scripts.mkdir()
        for name in ("release_identity.ps1", "release_layout.ps1", "prepare_release_assets.ps1"):
            shutil.copyfile(PROJECT / "scripts" / name, scripts / name)
        (root / ".gitignore").write_text("build-idf*/\nrelease-assets/\n")
        (root / "platformio.ini").write_text('-DAPP_VERSION="1.2.1-beta"\n')
        shutil.copyfile(PROJECT / "partitions_16MB_littlefs.csv", root / "partitions.csv")
        init_repository(root)
        subprocess.run(["git", "-C", str(root), "add", "."], check=True)
        subprocess.run(["git", "-C", str(root), "commit", "-qm", "release inputs"], check=True)
        commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
        build = root / f"build-idf-{profile}"
        (build / "generated").mkdir(parents=True)
        identity = {
            "schema": "project-aura.build-identity.v1", "environment": environment,
            "source_commit": commit, "build_id": commit[:7] + ("-7-dual-i2c" if profile != "4_3" else ""),
            "hardware_target": target, "hardware_profile": profile, "partitions_file": "partitions.csv",
        }
        (build / "generated/build-identity.json").write_text(json.dumps(identity))
        paths = {
            "bootloader.bin": "bootloader/bootloader.bin",
            "partitions.bin": "partition_table/partition-table.bin",
            "boot_app0.bin": "ota_data_initial.bin",
            "firmware.bin": "aura_aq.bin", "littlefs.bin": "littlefs.bin",
        }
        for name, path in paths.items():
            destination = build / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(f"{profile}:{name}".encode())
        setup = (
            f". {ps_quote(scripts / 'release_identity.ps1')}; "
            f"$layout = Get-AuraReleaseBuildLayout -RepositoryRoot {ps_quote(root)} -Environment '{environment}'; "
            f"$identity = Read-AuraBuildIdentity -IdentityPath $layout.IdentityPath -Environment '{environment}' -RepositoryRoot {ps_quote(root)}; "
            "Write-AuraReleaseArtifactStamp -StampPath $layout.StampPath -Identity $identity -ArtifactInputs $layout.ArtifactInputs"
        )
        result = run_powershell(setup)
        self.assertEqual(result.returncode, 0, result.stderr)
        command = f"& {ps_quote(scripts / 'prepare_release_assets.ps1')} -Env '{environment}' -Version '1.2.1-beta' -SkipBuild -SkipWebInstallerSync"
        return command, build, root / "release-assets/v1.2.1-beta" / target, paths

    def test_native_outputs_keep_the_canonical_installer_names_for_both_profiles(self):
        for profile in ("4_3", "7_dual_i2c"):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as temp:
                command, build, output, paths = self.prepare(Path(temp), profile)
                result = run_powershell(command)
                self.assertEqual(result.returncode, 0, result.stderr)
                for name, path in paths.items():
                    self.assertEqual((output / name).read_bytes(), (build / path).read_bytes())
                manifest = json.loads((output / "manifest.json").read_text())
                self.assertEqual([p["path"] for p in manifest["builds"][0]["parts"]], list(paths))
                self.assertEqual(manifest["hardware_profile"], profile)
                retry = run_powershell(command)
                self.assertNotEqual(retry.returncode, 0)
                self.assertIn("already exists", retry.stderr)

    def test_skip_build_rejects_missing_stamp_and_any_changed_native_asset(self):
        for changed in (None, "aura_aq.bin", "ota_data_initial.bin", "littlefs.bin", "bootloader/bootloader.bin", "partition_table/partition-table.bin"):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temp:
                command, build, output, _ = self.prepare(Path(temp), "4_3")
                if changed is None:
                    (build / "generated/release-artifacts.json").unlink()
                else:
                    with (build / changed).open("ab") as stream:
                        stream.write(b"tampered")
                result = run_powershell(command)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("stamp is missing" if changed is None else "changed after", result.stderr)
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
