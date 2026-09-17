"""Prepare the same identity, EEZ invariants and web assets for native IDF."""

import argparse
from pathlib import Path
import re

from idf_build_env import BuildEnv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    env = BuildEnv(args.profile, args.build_dir)
    match = re.search(r'-DAPP_VERSION=\\?"([^"\\]+)', env.GetProjectOption("build_flags"))
    if not match:
        raise ValueError("APP_VERSION is missing in platformio.ini")
    version = match.group(1)
    env.Append(CPPDEFINES=[("APP_VERSION", f'"{version}"')])
    for script in ("postprocess_eez_ui.py", "check_sensor_i2c_routing.py", "set_build_id.py",
                   "generate_dashboard_gzip.py", "generate_dac_gzip.py", "generate_theme_gzip.py"):
        env.run(script)
    definitions = [f"{name}={value}" for name, value in env["CPPDEFINES"]]
    text = f'set(AURA_APP_VERSION "{version}")\nset(AURA_DEFINITIONS\n'
    text += "".join(f"    [=[{value}]=]\n" for value in definitions) + ")\n"
    destination = args.build_dir / "generated" / "aura-build.cmake"
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
