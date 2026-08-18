"""Fail the firmware build if retained RTC ABI symbols move.

Firmware 7c3f8e6 wrote wake diagnostics and boot-state words at these exact
addresses. New firmware must keep that layout so an OTA reboot can decode the
old RTC_NOINIT contents before writing any extended evidence after it.
"""

from pathlib import Path
import subprocess

Import("env")


EXPECTED_SYMBOLS = {
    "BacklightWakeBreadcrumbs::(anonymous namespace)::g_retained": (
        0x50000208,
        0x78,
    ),
    "(anonymous namespace)::boot_board_power_settled_magic": (
        0x50000280,
        0x04,
    ),
    "(anonymous namespace)::boot_board_auto_recovery_magic": (
        0x50000284,
        0x04,
    ),
    "(anonymous namespace)::boot_ui_auto_recovery_magic": (
        0x50000288,
        0x04,
    ),
    "boot_backlight_wake_evidence_words": (0x5000028C, 0x28),
}


def _find_nm(build_env):
    # The ESP32 PlatformIO builder defines SIZETOOL but not NM. Resolve the
    # adjacent binary from SCons' toolchain PATH instead of relying on the host
    # to provide a generic `nm` command.
    size_tool = build_env.WhereIs(build_env.subst("$SIZETOOL"))
    if not size_tool:
        raise RuntimeError("RTC_NOINIT ABI check could not locate SIZETOOL")
    size_path = Path(size_tool)
    nm_path = size_path.with_name(size_path.name.replace("-size", "-nm"))
    if not nm_path.is_file():
        raise RuntimeError(
            f"RTC_NOINIT ABI check could not locate nm beside {size_path}"
        )
    return str(nm_path)


def _read_symbols(elf_path, build_env):
    nm = _find_nm(build_env)
    output = subprocess.check_output(
        [nm, "-S", "-n", "-C", str(elf_path)],
        text=True,
        errors="replace",
    )

    symbols = {}
    for line in output.splitlines():
        fields = line.split(None, 3)
        if len(fields) != 4:
            continue
        address_text, size_text, _symbol_type, name = fields
        try:
            symbols[name] = (int(address_text, 16), int(size_text, 16))
        except ValueError:
            continue
    return symbols


def check_rtc_noinit_abi(source, target, env):
    elf_path = target[0]
    symbols = _read_symbols(elf_path, env)
    errors = []

    for name, expected in EXPECTED_SYMBOLS.items():
        actual = symbols.get(name)
        if actual is None:
            errors.append(f"missing symbol: {name}")
        elif actual != expected:
            errors.append(
                f"{name}: expected address/size 0x{expected[0]:08x}/0x{expected[1]:x}, "
                f"got 0x{actual[0]:08x}/0x{actual[1]:x}"
            )

    if errors:
        details = "\n  - ".join(errors)
        raise RuntimeError(f"RTC_NOINIT ABI regression:\n  - {details}")

    print("[rtc-noinit-abi] verified retained layout from firmware 7c3f8e6")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", check_rtc_noinit_abi)
