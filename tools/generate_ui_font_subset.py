#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
# SPDX-License-Identifier: GPL-3.0-or-later

"""Generate LVGL font subsets from Project Aura UI translation files."""

from __future__ import annotations

import argparse
import ast
from pathlib import Path
import re
import shlex
import subprocess
import sys


DEFAULT_EXTRA_SYMBOLS = "\u65e5\u672c\u8a9e\u00a9\u00b0\u00b3\u00b5\u221e\u2248\u2264\uff1f"
STRING_LITERAL_RE = re.compile(r'"(?:[^"\\]|\\.)*"')


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def read_c_string_literals(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    strings: list[str] = []
    for match in STRING_LITERAL_RE.finditer(text):
        try:
            strings.append(ast.literal_eval(match.group(0)))
        except (SyntaxError, ValueError) as exc:
            raise RuntimeError(f"failed to parse string literal in {path}: {match.group(0)}") from exc
    return strings


def collect_symbols(strings_path: Path, extra_symbols: str) -> str:
    strings = read_c_string_literals(strings_path)
    chars = {
        char
        for value in strings
        for char in value
        if ord(char) > 126
    }
    chars.update(extra_symbols)
    return "".join(sorted(chars))


def normalize_text_file(path: Path) -> None:
    data = path.read_bytes()
    path.write_bytes(data.rstrip(b"\r\n") + b"\n")


def run_font_conv(args: argparse.Namespace, size: int, symbols: str) -> None:
    font_name = f"{args.lv_font_name_prefix}_{size}"
    output = args.output_dir / f"{font_name}.c"
    command = [
        args.npx,
        "lv_font_conv",
        "--bpp",
        str(args.bpp),
        "--size",
        str(size),
        "--no-compress",
        "--font",
        str(args.font),
        "--symbols",
        symbols,
        "--range",
        args.ascii_range,
        "--format",
        "lvgl",
        "--lv-font-name",
        font_name,
        "-o",
        str(output),
    ]

    print(shlex.join(command))
    if not args.dry_run:
        subprocess.run(command, cwd=args.repo_root, check=True)
        normalize_text_file(output)


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description="Generate LVGL font subsets from UI translation strings."
    )
    parser.add_argument(
        "--font",
        type=Path,
        required=True,
        help="Path to the source TTF/OTF font, for example NotoSansJP-Regular.ttf.",
    )
    parser.add_argument(
        "--strings",
        type=Path,
        default=root / "src/ui/strings/UiStrings.ja.inc",
        help="Translation .inc file to scan for non-ASCII glyphs.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=root / "src/ui",
        help="Directory where generated LVGL C font files are written.",
    )
    parser.add_argument(
        "--lv-font-name-prefix",
        default="ui_font_noto_sans_jp_reg",
        help="LVGL font symbol prefix; size is appended automatically.",
    )
    parser.add_argument(
        "--sizes",
        type=int,
        nargs="+",
        default=[14, 18],
        help="Font sizes to generate.",
    )
    parser.add_argument(
        "--extra-symbols",
        default=DEFAULT_EXTRA_SYMBOLS,
        help="Symbols not necessarily present in the translation file but required by UI labels.",
    )
    parser.add_argument("--bpp", type=int, default=4, help="Bits per pixel for lv_font_conv.")
    parser.add_argument("--ascii-range", default="32-126", help="ASCII range passed to lv_font_conv.")
    parser.add_argument("--npx", default="npx", help="npx executable used to run lv_font_conv.")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without running them.")
    args = parser.parse_args()
    args.repo_root = root
    args.font = args.font.resolve()
    args.strings = args.strings.resolve()
    args.output_dir = args.output_dir.resolve()
    return args


def main() -> int:
    args = parse_args()
    if not args.font.is_file():
        print(f"font file not found: {args.font}", file=sys.stderr)
        return 2
    if not args.strings.is_file():
        print(f"strings file not found: {args.strings}", file=sys.stderr)
        return 2
    args.output_dir.mkdir(parents=True, exist_ok=True)

    symbols = collect_symbols(args.strings, args.extra_symbols)
    print(f"symbols: {len(symbols)}")
    for size in args.sizes:
        run_font_conv(args, size, symbols)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
