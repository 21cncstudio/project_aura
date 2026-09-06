from pathlib import Path
import subprocess
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HELPER_PATH = PROJECT_ROOT / "scripts" / "release_layout.ps1"
PREPARE_PATH = PROJECT_ROOT / "scripts" / "prepare_release_assets.ps1"


def invoke_helper(command: str):
    return subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            f". '{HELPER_PATH}'; {command}",
        ],
        text=True,
        capture_output=True,
        encoding="utf-8",
    )


class ReleaseLayoutTests(unittest.TestCase):
    def test_normalizes_canonical_hex_and_decimal_offsets(self):
        app = invoke_helper(
            "Assert-AuraCanonicalFlashOffset -Value '65536' "
            "-ExpectedValue 0x10000 -PartitionName 'app0'"
        )
        filesystem = invoke_helper(
            "Assert-AuraCanonicalFlashOffset -Value '0xc90000' "
            "-ExpectedValue 0xC90000 -PartitionName 'littlefs'"
        )
        self.assertEqual(app.returncode, 0, app.stderr)
        self.assertEqual(app.stdout.strip(), "0x10000")
        self.assertEqual(filesystem.returncode, 0, filesystem.stderr)
        self.assertEqual(filesystem.stdout.strip(), "0xC90000")

    def test_rejects_noncanonical_and_malformed_offsets(self):
        wrong = invoke_helper(
            "Assert-AuraCanonicalFlashOffset -Value '0x20000' "
            "-ExpectedValue 0x10000 -PartitionName 'app0'"
        )
        malformed = invoke_helper(
            "Assert-AuraCanonicalFlashOffset -Value '0x10000junk' "
            "-ExpectedValue 0x10000 -PartitionName 'app0'"
        )
        self.assertNotEqual(wrong.returncode, 0)
        self.assertIn("non-canonical offset", wrong.stderr)
        self.assertNotEqual(malformed.returncode, 0)
        self.assertIn("invalid offset", malformed.stderr)

    def test_guards_run_before_release_directory_creation(self):
        source = PREPARE_PATH.read_text(encoding="utf-8")
        app_guard = source.index(
            '$app0Offset = Assert-AuraCanonicalFlashOffset'
        )
        filesystem_guard = source.index(
            '$littlefsOffset = Assert-AuraCanonicalFlashOffset'
        )
        output_creation = source.index(
            'New-Item -ItemType Directory -Path $outDir'
        )
        artifact_stamp_creation = source.index('Write-AuraReleaseArtifactStamp')
        self.assertLess(app_guard, output_creation)
        self.assertLess(filesystem_guard, output_creation)
        self.assertLess(app_guard, artifact_stamp_creation)
        self.assertLess(filesystem_guard, artifact_stamp_creation)


if __name__ == "__main__":
    unittest.main()
