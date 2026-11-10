#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Daniel Abramowitz
# SPDX-License-Identifier: GPL-3.0-or-later

# Linux counterpart of scripts/run_tests.ps1: runs the native Unity suites
# through scripts/run_native_tests.py with the canonical PlatformIO penv Python.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/run_tests.sh [--environment ENV]... [--filter PATTERN]...
                            [--compiler-dir DIR] [--verbosity 0-3]

  --environment ENV   PlatformIO environment; repeat for several (default: all ten).
  --filter PATTERN    Unity suite filter; repeat for several.
  --compiler-dir DIR  Directory holding gcc and g++ (default: /usr/bin, set by
                      run_native_tests.py).
  --verbosity N       PlatformIO verbosity, 0-3 (default: 0).
EOF
}

fail() {
    echo "run_tests.sh: $*" >&2
    exit 2
}

need_value() {
    [[ $# -ge 2 && -n "$2" ]] || fail "$1 needs a value"
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
python_exe="$HOME/.platformio/penv/bin/python"

launcher_args=()
verbosity=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --environment) need_value "$@"; launcher_args+=(--environment "$2"); shift 2 ;;
        --filter) need_value "$@"; launcher_args+=(--filter "$2"); shift 2 ;;
        --compiler-dir) need_value "$@"; launcher_args+=(--compiler-dir "$2"); shift 2 ;;
        --verbosity)
            need_value "$@"
            [[ "$2" =~ ^[0-3]$ ]] || fail "--verbosity must be 0, 1, 2 or 3 (got '$2')"
            verbosity="$2"
            shift 2
            ;;
        -h | --help) usage; exit 0 ;;
        *) usage >&2; fail "unknown argument '$1'" ;;
    esac
done

if [[ ! -f "$python_exe" || ! -x "$python_exe" ]]; then
    fail "PlatformIO penv Python not found at $python_exe. This launcher does not fall back to a PATH installation."
fi

if [[ "$verbosity" -gt 0 ]]; then
    launcher_args+=("-$(printf 'v%.0s' $(seq 1 "$verbosity"))")
fi

exec "$python_exe" -I "$script_dir/run_native_tests.py" "${launcher_args[@]}"
