from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))

from eez_ui_postprocess import PostprocessError, postprocess_project


BASE_FONTS_HEADER = """#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

extern const lv_font_t ui_font_noto_sans_sc_reg_14;
extern const lv_font_t ui_font_noto_sans_sc_reg_18;

#endif
"""

BASE_SCREENS = """void generated() {
    // co2_marker_1
    lv_obj_set_style_border_color(obj, lv_color_hex(0x160c09), LV_PART_MAIN | LV_STATE_DEFAULT);
    // label_co2_title_1

    // chip_delta_4
    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    // chip_delta_25
    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    // label_delta_4

    // label_firmware_trust
    lv_label_set_text_static(obj, "UNVERIFIED FW");
    tick_screen_page_settings();

    // label_optional_gas_text
    lv_label_set_text_static(obj, "Optional DFRobot electrochemical gas module for NH3, O3, SO2, NO2, H2S, or ambient O2. Units and reference bands depend on the installed sensor. Use as an air-quality indicator, not as a certified safety monitor.");
    // optional_gas_info_thresholds
}
"""


class EezUiPostprocessTests(unittest.TestCase):
    def make_project(
        self,
        fonts_header: str = BASE_FONTS_HEADER,
        screens: str = BASE_SCREENS,
    ) -> Path:
        temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(temp_dir.cleanup)
        project = Path(temp_dir.name)
        ui_dir = project / "src" / "ui"
        ui_dir.mkdir(parents=True)
        (ui_dir / "fonts.h").write_text(fonts_header, encoding="utf-8", newline="\n")
        (ui_dir / "screens.c").write_text(screens, encoding="utf-8", newline="\n")
        (ui_dir / "ui_font_noto_sans_jp_reg_14.c").write_text("font14\n", encoding="utf-8")
        (ui_dir / "ui_font_noto_sans_jp_reg_18.c").write_text("font18\n", encoding="utf-8")
        return project

    def test_apply_adds_managed_external_font_block(self) -> None:
        project = self.make_project()

        changed = postprocess_project(project)
        text = (project / "src/ui/fonts.h").read_text(encoding="utf-8")

        self.assertEqual((Path("src/ui/fonts.h"),), changed)
        self.assertIn("PROJECT_AURA_MANAGED_BEGIN: external-japanese-fonts", text)
        self.assertEqual(1, text.count("ui_font_noto_sans_jp_reg_14"))
        self.assertEqual(1, text.count("ui_font_noto_sans_jp_reg_18"))

    def test_apply_is_idempotent(self) -> None:
        project = self.make_project()
        postprocess_project(project)
        target = project / "src/ui/fonts.h"
        first = target.read_bytes()

        changed = postprocess_project(project)

        self.assertEqual((), changed)
        self.assertEqual(first, target.read_bytes())

    def test_apply_normalizes_existing_unmanaged_declarations(self) -> None:
        project = self.make_project(
            BASE_FONTS_HEADER.replace(
                "\n#endif",
                "\nextern const lv_font_t ui_font_noto_sans_jp_reg_14;"
                "\nextern const lv_font_t ui_font_noto_sans_jp_reg_18;\n#endif",
            )
        )

        postprocess_project(project)
        text = (project / "src/ui/fonts.h").read_text(encoding="utf-8")

        self.assertEqual(1, text.count("ui_font_noto_sans_jp_reg_14"))
        self.assertEqual(1, text.count("ui_font_noto_sans_jp_reg_18"))
        self.assertIn("PROJECT_AURA_MANAGED_BEGIN", text)

    def test_check_reports_required_postprocess_without_writing(self) -> None:
        project = self.make_project()
        target = project / "src/ui/fonts.h"
        original = target.read_bytes()

        with self.assertRaisesRegex(PostprocessError, "post-process required"):
            postprocess_project(project, check=True)

        self.assertEqual(original, target.read_bytes())

    def test_check_passes_after_apply(self) -> None:
        project = self.make_project()
        postprocess_project(project)

        self.assertEqual((), postprocess_project(project, check=True))

    def test_missing_anchor_fails_without_modifying_file(self) -> None:
        project = self.make_project("#ifndef FONTS_H\n#define FONTS_H\n#endif\n")
        target = project / "src/ui/fonts.h"
        original = target.read_bytes()

        with self.assertRaisesRegex(PostprocessError, "expected exactly one anchor"):
            postprocess_project(project)

        self.assertEqual(original, target.read_bytes())

    def test_missing_external_font_source_fails(self) -> None:
        project = self.make_project()
        (project / "src/ui/ui_font_noto_sans_jp_reg_18.c").unlink()

        with self.assertRaisesRegex(PostprocessError, "required external UI source missing"):
            postprocess_project(project)

    def test_apply_restores_generated_screen_invariants(self) -> None:
        stale_screens = (
            BASE_SCREENS.replace("0x160c09", "0x130b08")
            .replace("border_width(obj, 2", "border_width(obj, 1")
            .replace("UNVERIFIED FW", "OFFICIAL FW")
            .replace(
                "H2S, or ambient O2. Units and reference bands depend on the installed sensor.",
                "or H2S. Higher ppm means higher gas concentration. Bands depend on the installed sensor.",
            )
        )
        project = self.make_project(screens=stale_screens)
        postprocess_project(project)
        postprocess_project(project)
        screens_path = project / "src/ui/screens.c"

        text = screens_path.read_text(encoding="utf-8")

        self.assertIn("lv_color_hex(0x160c09)", text)
        self.assertEqual(2, text.count("border_width(obj, 2"))
        self.assertIn('lv_label_set_text_static(obj, "UNVERIFIED FW")', text)
        self.assertIn("H2S, or ambient O2", text)
        self.assertEqual((), postprocess_project(project, check=True))

    def test_unknown_screen_shape_fails_without_modifying_file(self) -> None:
        project = self.make_project(
            screens=BASE_SCREENS.replace("border_width(obj, 2", "border_width(obj, 7", 1)
        )
        target = project / "src/ui/screens.c"
        fonts_target = project / "src/ui/fonts.h"
        original = target.read_bytes()
        original_fonts = fonts_target.read_bytes()

        with self.assertRaisesRegex(PostprocessError, "supported property"):
            postprocess_project(project)

        self.assertEqual(original, target.read_bytes())
        self.assertEqual(original_fonts, fonts_target.read_bytes())

    def test_optional_firmware_trust_block_may_be_absent(self) -> None:
        start = BASE_SCREENS.index("    // label_firmware_trust")
        end = BASE_SCREENS.index("\n\n    // label_optional_gas_text", start)
        screens = BASE_SCREENS[:start] + BASE_SCREENS[end + 2 :]
        project = self.make_project(screens=screens)

        postprocess_project(project)

        self.assertEqual((), postprocess_project(project, check=True))


if __name__ == "__main__":
    unittest.main()
