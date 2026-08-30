"""Check native-USB build wiring without claiming physical USB validation.

The paired native CH422G suites exercise the real driver and probe. These
checks guard the production dependency selection and shared policy wiring.
"""

import configparser
from pathlib import Path
import re
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SHARED_DRIVER = "ESP32_IO_Expander=symlink://third_party/ESP32_IO_Expander_Aura"


def effective_option(config, section, option):
    """Resolve the single-parent inheritance used by these PlatformIO envs."""
    value = config.get(section, option, fallback=None)
    if value is None:
        parent = config.get(section, "extends", fallback=None)
        return effective_option(config, parent, option) if parent else ""

    def interpolate(match):
        source_section, source_option = match.group(1).rsplit(".", 1)
        return effective_option(config, source_section, source_option)

    return re.sub(r"\$\{([^}]+)\}", interpolate, value)


class NativeUsbContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = configparser.ConfigParser(interpolation=None)
        cls.config.read(PROJECT_ROOT / "platformio.ini", encoding="utf-8")

    def test_both_production_profiles_enable_native_cdc(self):
        for environment in ("project_aura", "project_aura_7"):
            with self.subTest(environment=environment):
                flags = effective_option(
                    self.config, f"env:{environment}", "build_flags"
                ).split()
                self.assertEqual(
                    ["-DARDUINO_USB_CDC_ON_BOOT=1"],
                    [flag for flag in flags if flag.startswith("-DARDUINO_USB_CDC_ON_BOOT")],
                )

    def test_both_production_profiles_select_only_shared_expander_dependency(self):
        for environment in ("project_aura", "project_aura_7"):
            with self.subTest(environment=environment):
                dependencies = effective_option(
                    self.config, f"env:{environment}", "lib_deps"
                ).splitlines()
                self.assertEqual(
                    [SHARED_DRIVER],
                    [line.strip() for line in dependencies if "ESP32_IO_Expander" in line],
                )
        self.assertTrue(
            (PROJECT_ROOT / "third_party/ESP32_IO_Expander_Aura/src/port/esp_io_expander_ch422g.c").is_file()
        )
        self.assertFalse(
            (PROJECT_ROOT / "third_party/ESP32_IO_Expander_7/src/port/esp_io_expander_ch422g.c").is_file()
        )

    def test_driver_and_diagnostic_probe_consume_shared_initial_image(self):
        driver = (
            PROJECT_ROOT / "third_party/ESP32_IO_Expander_Aura/src/port/esp_io_expander_ch422g.c"
        ).read_text(encoding="utf-8")
        probe = (PROJECT_ROOT / "src/core/Ch422gReadyProbe.h").read_text(encoding="utf-8")
        for source in (driver, probe):
            self.assertIn('#include "Ch422gBoardPolicy.h"', source)
        self.assertRegex(
            driver,
            r"#define\s+REG_WR_IO_DEFAULT_VAL\s+\(?AURA_CH422G_INITIAL_IO_VALUE\)?",
        )
        self.assertRegex(
            probe,
            r"kWriteIoSafeValue\s*=\s*AURA_CH422G_INITIAL_IO_VALUE\s*;",
        )

    def test_both_native_profile_envs_cover_reset_and_probe(self):
        for profile in ("4_3", "7"):
            with self.subTest(profile=profile):
                section = f"env:native_test_ch422g_{profile}_profile"
                self.assertEqual(
                    {"test_ch422g_reset", "test_ch422g_ready_probe"},
                    set(effective_option(self.config, section, "test_filter").split()),
                )
                self.assertEqual("true", effective_option(self.config, section, "test_build_src"))
                flags = effective_option(self.config, section, "build_flags").split()
                profile_flags = [flag for flag in flags if flag.startswith("-DAURA_HARDWARE_PROFILE_7")]
                if profile == "7":
                    self.assertEqual(["-DAURA_HARDWARE_PROFILE_7=1"], profile_flags)
                else:
                    self.assertIn(profile_flags, ([], ["-DAURA_HARDWARE_PROFILE_7=0"]))


if __name__ == "__main__":
    unittest.main()
