#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
# SPDX-License-Identifier: GPL-3.0-or-later

"""Verify glyph coverage for localized status messages in every body font."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import sys
import unicodedata

from generate_ui_font_subset import read_c_string_literals


ARRAY_RE = re.compile(
    r"static const (?:u?int(?:8|16|32)_t)\s+(?P<name>\w+)\[\]\s*=\s*"
    r"\{(?P<body>.*?)\};",
    re.DOTALL,
)
CMAP_RE = re.compile(
    r"\{\s*"
    r"\.range_start\s*=\s*(?P<start>\d+)\s*,\s*"
    r"\.range_length\s*=\s*(?P<length>\d+)\s*,\s*"
    r"\.glyph_id_start\s*=\s*\d+\s*,\s*"
    r"\.unicode_list\s*=\s*(?P<unicode>\w+|NULL)\s*,\s*"
    r"\.glyph_id_ofs_list\s*=\s*(?P<offsets>\w+|NULL)\s*,\s*"
    r"\.list_length\s*=\s*(?P<list_length>\d+)\s*,\s*"
    r"\.type\s*=\s*(?P<type>\w+)\s*"
    r"\}",
    re.DOTALL,
)
NUMBER_RE = re.compile(r"0[xX][0-9a-fA-F]+|\d+")
KEY_RE = re.compile(r"UI_STR_ID\((?P<name>\w+)\)")


@dataclass(frozen=True)
class FontCheck:
    font_path: Path
    strings_paths: tuple[Path, ...]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_number_array(body: str) -> list[int]:
    return [int(token, 0) for token in NUMBER_RE.findall(body)]


def font_codepoints(path: Path) -> set[int]:
    text = path.read_text(encoding="utf-8")
    arrays = {
        match.group("name"): parse_number_array(match.group("body"))
        for match in ARRAY_RE.finditer(text)
    }
    codepoints: set[int] = set()

    for match in CMAP_RE.finditer(text):
        start = int(match.group("start"))
        length = int(match.group("length"))
        list_length = int(match.group("list_length"))
        cmap_type = match.group("type")
        unicode_name = match.group("unicode")
        offsets_name = match.group("offsets")

        if cmap_type == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            codepoints.update(range(start, start + length))
        elif cmap_type == "LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL":
            offsets = arrays[offsets_name]
            # lv_font_conv uses zero both for the first glyph and for gaps.
            codepoints.update(
                start + relative
                for relative, glyph_offset in enumerate(offsets[:length])
                if relative == 0 or glyph_offset != 0
            )
        elif cmap_type in {
            "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY",
            "LV_FONT_FMT_TXT_CMAP_SPARSE_FULL",
        }:
            unicode_offsets = arrays[unicode_name]
            codepoints.update(start + offset for offset in unicode_offsets[:list_length])
        else:
            raise RuntimeError(f"unsupported cmap type in {path}: {cmap_type}")

    if not codepoints:
        raise RuntimeError(f"no LVGL cmaps found in {path}")
    return codepoints


def status_indices(root: Path) -> list[int]:
    keys_path = root / "src/ui/strings/UiStrings.keys.inc"
    keys = [match.group("name") for match in KEY_RE.finditer(keys_path.read_text(encoding="utf-8"))]
    if not keys:
        raise RuntimeError(f"no UI string keys found in {keys_path}")
    return [index for index, key in enumerate(keys) if key.startswith("Msg")]


def required_codepoints(paths: tuple[Path, ...], indices: list[int]) -> set[int]:
    values: list[str] = []
    for path in paths:
        strings = read_c_string_literals(path)
        if not indices or max(indices) >= len(strings):
            raise RuntimeError(f"translation table is shorter than the key table: {path}")
        values.extend(strings[index] for index in indices)
    return {
        ord(char)
        for value in values
        for char in value
        if char.isprintable() and not char.isspace()
    }


def describe(codepoint: int) -> str:
    char = chr(codepoint)
    name = unicodedata.name(char, "UNKNOWN")
    return f"U+{codepoint:04X} {ascii(char)} {name}"


def checks(root: Path) -> list[FontCheck]:
    strings = root / "src/ui/strings"
    fonts = root / "src/ui"
    latin_strings = tuple(
        strings / f"UiStrings.{locale}.inc"
        for locale in ("en", "de", "es", "fr", "it", "ptbr", "nl")
    )
    return [
        FontCheck(fonts / f"ui_font_jet_reg_{size}.c", latin_strings)
        for size in (14, 18)
    ] + [
        FontCheck(
            fonts / f"ui_font_noto_sans_sc_reg_{size}.c",
            (strings / "UiStrings.zh.inc",),
        )
        for size in (14, 18)
    ] + [
        FontCheck(
            fonts / f"ui_font_noto_sans_jp_reg_{size}.c",
            (strings / "UiStrings.ja.inc",),
        )
        for size in (14, 18)
    ]


def main() -> int:
    failed = False
    indices = status_indices(repo_root())
    for check in checks(repo_root()):
        required = required_codepoints(check.strings_paths, indices)
        available = font_codepoints(check.font_path)
        missing = sorted(required - available)
        if missing:
            failed = True
            print(f"MISSING {check.font_path.name}:")
            for codepoint in missing:
                print(f"  {describe(codepoint)}")
        else:
            print(f"OK {check.font_path.name}: {len(required)} required status glyphs")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
