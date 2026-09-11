"""Execute the actual native transport against a fault-injecting IDF API fake."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativeI2cTransportTests(unittest.TestCase):
    def test_transport_lifecycle_and_wire_contract(self):
        compiler = shutil.which("g++") or str(Path("C:/msys64/mingw64/bin/g++.exe"))
        if not Path(compiler).is_file():
            self.skipTest("C++ host compiler unavailable")
        fixture = ROOT / "tools/tests/native_i2c"
        with tempfile.TemporaryDirectory() as temp:
            executable = Path(temp) / "i2c_test.exe"
            env = dict(os.environ)
            env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DAURA_NATIVE_IDF=1",
                            "-I" + str(fixture), "-I" + str(ROOT / "include"),
                            str(ROOT / "components/aura_i2c/aura_i2c.cpp"), str(fixture / "transport_test.cpp"),
                            "-o", str(executable)], check=True, env=env, capture_output=True, text=True)
            result = subprocess.run([str(executable)], check=True, env=env, capture_output=True, text=True)
            self.assertIn("failure cleanup and restart passed", result.stdout)
