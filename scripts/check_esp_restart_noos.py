"""Fail the firmware build if the ESP32-S3 restart backport is not linked."""

from pathlib import Path
import subprocess
import sys

Import("env")

tools_dir = Path(env["PROJECT_DIR"]) / "tools"
sys.path.insert(0, str(tools_dir))

from esp_restart_noos_order import (
    RestartOrderError,
    parse_function_symbol,
    validate_restart_order,
)


def _find_objdump(build_env):
    size_tool = build_env.WhereIs(build_env.subst("$SIZETOOL"))
    if not size_tool:
        raise RuntimeError("restart-order check could not locate SIZETOOL")
    size_path = Path(size_tool)
    objdump_path = size_path.with_name(
        size_path.name.replace("-size", "-objdump")
    )
    if not objdump_path.is_file():
        raise RuntimeError(
            f"restart-order check could not locate objdump beside {size_path}"
        )
    return str(objdump_path)


def _read_symbol(objdump, elf_path, symbol):
    symbol_table = subprocess.check_output(
        [objdump, "-t", "-C", str(elf_path)],
        text=True,
        errors="replace",
    )
    return parse_function_symbol(symbol_table, symbol)


def _disassemble(objdump, elf_path, function):
    start = function.address
    stop = start + function.size
    return subprocess.check_output(
        [
            objdump,
            "-d",
            "-C",
            f"--start-address=0x{start:x}",
            f"--stop-address=0x{stop:x}",
            str(elf_path),
        ],
        text=True,
        errors="replace",
    )


def check_esp_restart_noos(source, target, env):
    elf_path = target[0]
    objdump = _find_objdump(env)
    try:
        wrapper_symbol = _read_symbol(
            objdump, elf_path, "__wrap_esp_restart_noos"
        )
        restart_symbol = _read_symbol(objdump, elf_path, "esp_restart")
        panic_symbol = _read_symbol(objdump, elf_path, "panic_restart")
        wrapper = _disassemble(objdump, elf_path, wrapper_symbol)
        restart_caller = _disassemble(objdump, elf_path, restart_symbol)
        panic_caller = _disassemble(objdump, elf_path, panic_symbol)
        validate_restart_order(
            wrapper,
            restart_caller,
            panic_caller,
            wrapper_symbol.section,
        )
    except RestartOrderError as error:
        raise RuntimeError(f"ESP32-S3 restart backport regression: {error}")
    print(
        "[restart-noos] verified reset/stall before cache disable "
        "and esp_restart/panic_restart linker routing"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", check_esp_restart_noos)
