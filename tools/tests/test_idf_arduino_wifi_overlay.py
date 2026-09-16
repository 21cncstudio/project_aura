"""Execute the actual patched Arduino STA event callback with host test doubles."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("wifi_overlay", ROOT / "scripts/idf_arduino_wifi_overlay.py")
overlay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overlay)
UPSTREAM = ROOT / "managed_components/espressif__arduino-esp32/libraries/WiFi/src/STA.cpp"


class WifiOverlayTests(unittest.TestCase):
    def test_upstream_changes_fail_closed(self):
        with self.assertRaises(ValueError):
            overlay.adapt("unrecognized implementation")
        with self.assertRaises(ValueError):
            overlay.adapt(overlay.BEFORE * 2)
        with self.assertRaises(ValueError):
            overlay.adapt(overlay.AFTER)

    def test_actual_callback_honors_disabled_reconnect_and_preserves_enabled_behavior(self):
        source = overlay.adapt(UPSTREAM.read_text(encoding="utf-8"))
        start = source.index("static void _onStaArduinoEvent(")
        end = source.index("\nvoid STAClass::_onStaEvent", start)
        callback = source[start:end]
        fixture = (ROOT / "tools/tests/fixtures/wifi_sta_reconnect.cpp").read_text(encoding="utf-8")
        compiler = shutil.which("g++")
        if not compiler:
            candidate = Path("C:/msys64/mingw64/bin/g++.exe")
            if candidate.exists():
                compiler = str(candidate)
        self.assertIsNotNone(compiler, "A C++ host compiler is required for the callback regression test")
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        with tempfile.TemporaryDirectory(prefix="aura-wifi-") as directory:
            path = Path(directory)
            cpp = path / "callback.cpp"
            exe = path / ("callback.exe" if os.name == "nt" else "callback")
            cpp.write_text(fixture.replace("// CALLBACK", callback), encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(exe)], check=True, env=env)
            for scenario in ("disabled_initial", "disabled_after_ip", "enabled_initial",
                             "enabled_after_ip", "voluntary_enabled", "disable_after_retry"):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(exe), scenario], check=True, env=env)


if __name__ == "__main__":
    unittest.main()
