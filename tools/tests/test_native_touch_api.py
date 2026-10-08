"""Execute the pinned official touch core and GT911 driver against mocked I2C."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from idf_touch_adapt import touch_patches

ROOT = Path(__file__).resolve().parents[2]

class NativeTouchApiTests(unittest.TestCase):
    def test_official_driver_with_aura_frame_policy(self):
        cxx = shutil.which("g++") or "C:/msys64/mingw64/bin/g++.exe"
        cc = str(Path(cxx).with_name("gcc.exe" if os.name == "nt" else "gcc"))
        fixture = ROOT / "tools/tests/native_touch"
        core = ROOT / "managed_components/espressif__esp_lcd_touch"
        gt911 = ROOT / "managed_components/espressif__esp_lcd_touch_gt911"
        self.assertTrue((core / "esp_lcd_touch.c").is_file(), "Run IDF reconfigure to resolve pinned components")
        env = dict(os.environ)
        env["PATH"] = str(Path(cxx).parent) + os.pathsep + env.get("PATH", "")
        def run(command):
            result = subprocess.run(command, capture_output=True, text=True, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            return result.stdout
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            flags = ["-Wall", "-Wextra", "-Werror"] + ["-I" + str(p) for p in (fixture, core / "include", gt911 / "include", ROOT / "src")]
            adapted = touch_patches(ROOT / "components/espressif__esp32_display_panel", core, gt911)
            source = adapted["drivers/touch/esp_panel_touch.cpp"]
            start = source.index("bool Touch::readRawDataPoints(")
            end = source.index("\nbool Touch::", start + 1)
            (temp / "wrapper_method.inc").write_text(source[start:end], encoding="utf-8")
            header = adapted["drivers/touch/port/esp_lcd_touch_gt911.h"]
            macro = header[header.index("#define ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG_WITH_ADDR"):]
            (temp / "gt911_address_macro.inc").write_text(macro, encoding="utf-8")
            flags.append("-I" + str(temp))
            objects = []
            for component in (core, gt911):
                name = component.name.removeprefix("espressif__")
                obj = temp / (name + ".o")
                run([cc, "-std=c11", *flags, "-c", str(component / (name + ".c")), "-o", str(obj)])
                objects.append(str(obj))
            exe = temp / "touch_test.exe"
            run([cxx, "-std=c++17", *flags, str(fixture / "touch_test.cpp"), *objects, "-o", str(exe)])
            self.assertIn("41 official touch API scenarios passed", run([str(exe)]))
