"""PlatformIO pre-build adapter for the standalone EEZ UI post-processor."""

from pathlib import Path
import sys

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
TOOLS_DIR = PROJECT_DIR / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from eez_ui_postprocess import PostprocessError, postprocess_project


try:
    changed = postprocess_project(PROJECT_DIR)
except PostprocessError as exc:
    print(f"[eez-postprocess] ERROR: {exc}")
    raise

if changed:
    print("[eez-postprocess] updated: " + ", ".join(str(path) for path in changed))
else:
    print("[eez-postprocess] up-to-date")
