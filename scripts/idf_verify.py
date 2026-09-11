"""Apply Aura's existing binary gates to a native-IDF firmware build."""

import argparse
from pathlib import Path
import subprocess

from idf_build_env import BuildEnv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--size-tool", default="xtensa-esp-elf-size")
    args = parser.parse_args()
    env = BuildEnv(args.profile, args.build_dir, args.size_tool)
    env.run("check_rtc_noinit_abi.py")
    env.run("check_ota_image_identity.py")
    for target, action in env.actions:
        action([], [target], env)

    # Reuse the existing disassembly validator against the real upstream path.
    helpers = env.run("check_esp_restart_noos.py")
    objdump = helpers["_find_objdump"](env)
    elf = args.build_dir / "aura_aq.elf"
    table = subprocess.check_output([objdump, "-t", "-C", str(elf)], text=True)
    if "__wrap_esp_restart_noos" in table:
        raise RuntimeError("The IDF 5.3.2 restart backport leaked into the IDF 6.1 build")
    symbol = helpers["_read_symbol"](objdump, elf, "esp_restart_noos")
    disassembly = helpers["_disassemble"](objdump, elf, symbol)
    callers = [helpers["_disassemble"](objdump, elf,
               helpers["_read_symbol"](objdump, elf, name))
               for name in ("esp_restart", "panic_restart")]
    helpers["validate_restart_order"](disassembly, *callers, symbol.section,
                                      symbol="esp_restart_noos")
    print("[restart-noos] verified upstream IDF reset/stall before cache disable")


if __name__ == "__main__":
    main()
