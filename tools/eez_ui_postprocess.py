#!/usr/bin/env python3
"""Restore Project Aura invariants after EEZ Studio regenerates ``src/ui``.

The operation is deterministic and idempotent. Run it directly after an EEZ
Build, or use ``--check`` in reviews and CI. The firmware PlatformIO build also
invokes it through ``scripts/postprocess_eez_ui.py``.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Match, Sequence


class PostprocessError(RuntimeError):
    """Raised when generated UI output cannot be repaired safely."""


@dataclass(frozen=True)
class ManagedHeaderBlock:
    relative_path: Path
    anchor: str
    name: str
    lines: tuple[str, ...]

    @property
    def begin_marker(self) -> str:
        return f"// PROJECT_AURA_MANAGED_BEGIN: {self.name}"

    @property
    def end_marker(self) -> str:
        return f"// PROJECT_AURA_MANAGED_END: {self.name}"

    def render(self) -> str:
        return "\n".join((self.begin_marker, *self.lines, self.end_marker))


@dataclass(frozen=True)
class ManagedScreenOverride:
    name: str
    anchor: str
    end_anchor: str
    pattern: re.Pattern[str]
    replacement: str | Callable[[Match[str]], str]
    optional: bool = False


MANAGED_HEADER_BLOCKS: tuple[ManagedHeaderBlock, ...] = (
    ManagedHeaderBlock(
        relative_path=Path("src/ui/fonts.h"),
        anchor="extern const lv_font_t ui_font_noto_sans_sc_reg_18;",
        name="external-japanese-fonts",
        lines=(
            "extern const lv_font_t ui_font_noto_sans_jp_reg_14;",
            "extern const lv_font_t ui_font_noto_sans_jp_reg_18;",
        ),
    ),
)

REQUIRED_GENERATED_SOURCES: tuple[Path, ...] = (
    Path("src/ui/ui_font_noto_sans_jp_reg_14.c"),
    Path("src/ui/ui_font_noto_sans_jp_reg_18.c"),
)

SCREENS_PATH = Path("src/ui/screens.c")

OPTIONAL_GAS_TEXT = (
    "Optional DFRobot electrochemical gas module for NH3, O3, SO2, NO2, H2S, "
    "or ambient O2. Units and reference bands depend on the installed sensor. "
    "Use as an air-quality indicator, not as a certified safety monitor."
)


def _style_line(property_name: str, value_pattern: str) -> re.Pattern[str]:
    return re.compile(
        rf"(?m)^(?P<indent>[ \t]*)lv_obj_set_style_{property_name}"
        rf"\(obj, {value_pattern}, LV_PART_MAIN \| LV_STATE_DEFAULT\);$"
    )


def _restore_co2_marker_border(match: Match[str]) -> str:
    if match.group("color").lower() == "160c09":
        return match.group(0)
    return (
        f'{match.group("indent")}lv_obj_set_style_border_color(obj, '
        "lv_color_hex(0x160c09), LV_PART_MAIN | LV_STATE_DEFAULT);"
    )


SCREEN_OVERRIDES: tuple[ManagedScreenOverride, ...] = (
    ManagedScreenOverride(
        name="CO2 marker border color",
        anchor="// co2_marker_1",
        end_anchor="// label_co2_title_1",
        pattern=_style_line(
            "border_color",
            r"lv_color_hex\(0x(?:ff)?(?P<color>130b08|160c09)\)",
        ),
        replacement=_restore_co2_marker_border,
    ),
    ManagedScreenOverride(
        name="3-hour pressure delta chip border",
        anchor="// chip_delta_4",
        end_anchor="// chip_delta_25",
        pattern=_style_line("border_width", r"[12]"),
        replacement=(
            r"\g<indent>lv_obj_set_style_border_width(obj, 2, "
            r"LV_PART_MAIN | LV_STATE_DEFAULT);"
        ),
    ),
    ManagedScreenOverride(
        name="24-hour pressure delta chip border",
        anchor="// chip_delta_25",
        end_anchor="// label_delta_4",
        pattern=_style_line("border_width", r"[12]"),
        replacement=(
            r"\g<indent>lv_obj_set_style_border_width(obj, 2, "
            r"LV_PART_MAIN | LV_STATE_DEFAULT);"
        ),
    ),
    ManagedScreenOverride(
        name="fail-safe firmware trust label",
        anchor="// label_firmware_trust",
        end_anchor="tick_screen_page_settings();",
        pattern=re.compile(
            r'(?P<setter>lv_label_set_text(?:_static)?)'
            r'\(obj, "(?:OFFICIAL|UNVERIFIED) FW"\);'
        ),
        replacement=r'\g<setter>(obj, "UNVERIFIED FW");',
        optional=True,
    ),
    ManagedScreenOverride(
        name="ambient O2 optional gas description",
        anchor="// label_optional_gas_text",
        end_anchor="// optional_gas_info_thresholds",
        pattern=re.compile(
            r'(?P<setter>lv_label_set_text(?:_static)?)'
            r'\(obj, "Optional DFRobot electrochemical gas module for [^"\n]*"\);'
        ),
        replacement=lambda match: (
            f'{match.group("setter")}(obj, "{OPTIONAL_GAS_TEXT}");'
        ),
    ),
)


def _without_managed_block(text: str, block: ManagedHeaderBlock) -> str:
    pattern = re.compile(
        rf"(?m)^[ \t]*{re.escape(block.begin_marker)}[ \t]*\n"
        rf".*?"
        rf"^[ \t]*{re.escape(block.end_marker)}[ \t]*(?:\n|$)",
        re.DOTALL,
    )
    return pattern.sub("", text)


def _without_standalone_lines(text: str, lines: Sequence[str]) -> str:
    for line in lines:
        text = re.sub(
            rf"(?m)^[ \t]*{re.escape(line)}[ \t]*(?:\n|$)",
            "",
            text,
        )
    return text


def _apply_header_block(text: str, block: ManagedHeaderBlock) -> str:
    normalized = text.replace("\r\n", "\n")
    normalized = _without_managed_block(normalized, block)
    normalized = _without_standalone_lines(normalized, block.lines)

    anchor_matches = list(
        re.finditer(rf"(?m)^[ \t]*{re.escape(block.anchor)}[ \t]*$", normalized)
    )
    if len(anchor_matches) != 1:
        raise PostprocessError(
            f"{block.relative_path}: expected exactly one anchor "
            f"{block.anchor!r}, found {len(anchor_matches)}"
        )

    match = anchor_matches[0]
    return normalized[: match.end()] + "\n" + block.render() + normalized[match.end() :]


def _apply_screen_override(text: str, override: ManagedScreenOverride) -> str:
    anchor_count = text.count(override.anchor)
    if anchor_count == 0 and override.optional:
        return text
    if anchor_count != 1:
        raise PostprocessError(
            f"{SCREENS_PATH}: {override.name}: expected exactly one anchor "
            f"{override.anchor!r}, found {anchor_count}"
        )

    start = text.index(override.anchor)
    end = text.find(override.end_anchor, start + len(override.anchor))
    if end < 0:
        raise PostprocessError(
            f"{SCREENS_PATH}: {override.name}: end anchor "
            f"{override.end_anchor!r} not found"
        )

    window = text[start:end]
    matches = list(override.pattern.finditer(window))
    if len(matches) != 1:
        raise PostprocessError(
            f"{SCREENS_PATH}: {override.name}: expected exactly one supported "
            f"property, found {len(matches)}"
        )

    patched = override.pattern.sub(override.replacement, window, count=1)
    return text[:start] + patched + text[end:]


def _apply_screen_overrides(text: str) -> str:
    normalized = text.replace("\r\n", "\n")
    for override in SCREEN_OVERRIDES:
        normalized = _apply_screen_override(normalized, override)
    return normalized


def _encode_with_original_newlines(original: bytes, text: str) -> bytes:
    newline = "\r\n" if b"\r\n" in original else "\n"
    if newline == "\r\n":
        text = text.replace("\n", "\r\n")
    return text.encode("utf-8")


def postprocess_project(project_dir: Path, *, check: bool = False) -> tuple[Path, ...]:
    """Apply or validate all Project Aura additions to EEZ-generated files."""

    project_dir = project_dir.resolve()
    missing_sources = [
        path for path in REQUIRED_GENERATED_SOURCES if not (project_dir / path).is_file()
    ]
    if missing_sources:
        joined = ", ".join(str(path) for path in missing_sources)
        raise PostprocessError(f"required external UI source missing: {joined}")

    changed: list[Path] = []
    pending_writes: list[tuple[Path, bytes]] = []
    for block in MANAGED_HEADER_BLOCKS:
        target = project_dir / block.relative_path
        if not target.is_file():
            raise PostprocessError(f"EEZ-generated file missing: {block.relative_path}")

        original = target.read_bytes()
        try:
            text = original.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise PostprocessError(f"{block.relative_path}: expected UTF-8 text") from exc

        patched_text = _apply_header_block(text, block)
        patched = _encode_with_original_newlines(original, patched_text)
        if patched == original:
            continue

        changed.append(block.relative_path)
        pending_writes.append((target, patched))

    screens_target = project_dir / SCREENS_PATH
    if not screens_target.is_file():
        raise PostprocessError(f"EEZ-generated file missing: {SCREENS_PATH}")

    screens_original = screens_target.read_bytes()
    try:
        screens_text = screens_original.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise PostprocessError(f"{SCREENS_PATH}: expected UTF-8 text") from exc

    patched_screens_text = _apply_screen_overrides(screens_text)
    patched_screens = _encode_with_original_newlines(
        screens_original, patched_screens_text
    )
    if patched_screens != screens_original:
        changed.append(SCREENS_PATH)
        pending_writes.append((screens_target, patched_screens))

    if check and changed:
        joined = ", ".join(str(path) for path in changed)
        raise PostprocessError(
            f"EEZ post-process required for: {joined}; "
            "run 'python tools/eez_ui_postprocess.py'"
        )

    for target, patched in pending_writes:
        target.write_bytes(patched)

    return tuple(changed)


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Restore Project Aura additions after EEZ Studio generation."
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Project root (defaults to the parent of tools/).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Validate generated files without modifying them.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        changed = postprocess_project(args.project_dir, check=args.check)
    except PostprocessError as exc:
        print(f"[eez-postprocess] ERROR: {exc}", file=sys.stderr)
        return 1

    if args.check:
        print("[eez-postprocess] check passed")
    elif changed:
        print("[eez-postprocess] updated: " + ", ".join(str(path) for path in changed))
    else:
        print("[eez-postprocess] up-to-date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
