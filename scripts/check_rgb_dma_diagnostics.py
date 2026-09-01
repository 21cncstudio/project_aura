"""Pin and verify the private RGB-driver diagnostic hook.

The firmware observes two private esp_rgb_panel_t fields once at callback
registration. Refuse to build if the precompiled archive changes, then prove
that both linker wrappers are wired to the audited callers and that the ISR
wrapper is in IRAM.
"""

from pathlib import Path
import hashlib
import json
import re
import subprocess

Import("env")


EXPECTED_PACKAGE_VERSION = "5.3.2+sha.cfea4f7c98"
EXPECTED_ARCHIVE_SHA256 = (
    "94babdbccb36109be9de3a9fdd758e8422ec49c91a363eb0f87e475105bb26b4"
)


def _sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _framework_paths(build_env):
    package_dir = build_env.PioPlatform().get_package_dir(
        "framework-arduinoespressif32-libs"
    )
    if not package_dir:
        raise RuntimeError(
            "RGB diagnostics could not locate framework-arduinoespressif32-libs"
        )
    package_dir = Path(package_dir)
    return (
        package_dir / "package.json",
        package_dir / "esp32s3" / "lib" / "libesp_lcd.a",
    )


def _validate_driver_abi(build_env):
    package_json_path, archive_path = _framework_paths(build_env)
    if not package_json_path.is_file() or not archive_path.is_file():
        raise RuntimeError(
            "RGB diagnostics require the audited ESP32-S3 framework package"
        )
    package = json.loads(package_json_path.read_text(encoding="utf-8"))
    version = package.get("version")
    if version != EXPECTED_PACKAGE_VERSION:
        raise RuntimeError(
            "RGB diagnostics private ABI mismatch: expected framework libs "
            f"{EXPECTED_PACKAGE_VERSION}, found {version!r}"
        )
    archive_hash = _sha256(archive_path)
    if archive_hash.lower() != EXPECTED_ARCHIVE_SHA256:
        raise RuntimeError(
            "RGB diagnostics private ABI mismatch: libesp_lcd.a SHA256 is "
            f"{archive_hash}, expected {EXPECTED_ARCHIVE_SHA256}"
        )
    print(
        "[rgb-dma-diag] verified ESP-IDF 5.3.2 libesp_lcd.a private ABI "
        f"({archive_hash[:12]})"
    )


def _find_objdump(build_env):
    size_tool = build_env.WhereIs(build_env.subst("$SIZETOOL"))
    if not size_tool:
        raise RuntimeError("RGB diagnostics could not locate SIZETOOL")
    size_path = Path(size_tool)
    objdump_path = size_path.with_name(
        size_path.name.replace("-size", "-objdump")
    )
    if not objdump_path.is_file():
        raise RuntimeError(
            f"RGB diagnostics could not locate objdump beside {size_path}"
        )
    return str(objdump_path)


def _find_function(symbol_table, name):
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]+)\s+\S+\s+F\s+(\S+)\s+"
        r"([0-9a-fA-F]+)\s+" + re.escape(name) + r"\s*$"
    )
    matches = []
    for line in symbol_table.splitlines():
        match = pattern.match(line)
        if match:
            matches.append(
                (
                    int(match.group(1), 16),
                    match.group(2),
                    int(match.group(3), 16),
                )
            )
    matches = [match for match in matches if match[2] > 0]
    if len(matches) != 1:
        raise RuntimeError(
            f"RGB diagnostics expected one function {name!r}, found {len(matches)}"
        )
    return matches[0]


def _require_map_reference(map_text, symbol, expected_object):
    marker = "Cross Reference Table\n\nSymbol"
    marker_at = map_text.find(marker)
    if marker_at < 0:
        raise RuntimeError(
            "RGB diagnostics linker map has no cross-reference table"
        )
    lines = map_text[marker_at:].splitlines()
    starts = [
        index
        for index, line in enumerate(lines)
        if re.match(r"^" + re.escape(symbol) + r"\s+", line)
    ]
    if len(starts) != 1:
        raise RuntimeError(
            f"RGB diagnostics expected one map entry for {symbol!r}, "
            f"found {len(starts)}"
        )
    block = [lines[starts[0]]]
    for line in lines[starts[0] + 1 :]:
        if line and not line[0].isspace():
            break
        block.append(line)
    normalized = "\n".join(block).replace("\\", "/")
    if expected_object not in normalized:
        raise RuntimeError(
            f"RGB diagnostics {symbol} is not referenced by "
            f"{expected_object}"
        )


def _check_final_elf(source, target, env):
    elf_path = Path(str(target[0]))
    objdump = _find_objdump(env)
    symbol_table = subprocess.check_output(
        [objdump, "-t", "-C", str(elf_path)],
        text=True,
        errors="replace",
    )

    gdma_wrapper = _find_function(symbol_table, "__wrap_gdma_start")
    register_wrapper = _find_function(
        symbol_table,
        "__wrap_esp_lcd_rgb_panel_register_event_callbacks",
    )

    if not gdma_wrapper[1].startswith(".iram"):
        raise RuntimeError(
            "RGB diagnostics ISR function __wrap_gdma_start is in "
            f"{gdma_wrapper[1]}, not IRAM"
        )

    map_path = elf_path.with_suffix(".map")
    if not map_path.is_file():
        raise RuntimeError(
            f"RGB diagnostics could not locate linker map {map_path}"
        )
    map_text = map_path.read_text(encoding="utf-8", errors="replace")
    _require_map_reference(
        map_text,
        "__wrap_esp_lcd_rgb_panel_register_event_callbacks",
        "libESP32_Display_Panel.a(esp_panel_lcd.cpp.o)",
    )
    _require_map_reference(
        map_text,
        "__wrap_gdma_start",
        "libesp_lcd.a(esp_lcd_panel_rgb.c.obj)",
    )

    print(
        "[rgb-dma-diag] verified RGB registration hook, same-channel ISR "
        "start wrapper, and IRAM placement"
    )


_validate_driver_abi(env)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", _check_final_elf)
