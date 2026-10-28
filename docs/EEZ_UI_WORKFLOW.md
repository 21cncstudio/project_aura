# EEZ Studio UI workflow

EEZ Studio generates the LVGL sources in `src/ui`, but it does not know about
several Project Aura firmware additions and safe defaults. Regenerating the UI
can therefore produce valid C code that either fails to compile or silently
restores stale UI values.

After every **Build** in EEZ Studio, run from the repository root:

```powershell
python tools/eez_ui_postprocess.py
python tools/eez_ui_postprocess.py --check
```

The first command applies the Project Aura generated-UI contract. The second
command is a non-mutating verification and must report that the contract is
already satisfied. The native IDF build also invokes the same post-processor before compilation.
Firmware on `main` is built with `scripts/build_idf.ps1`.
The canonical project is `ui/aura-lvgl.eez-project`, using LVGL 9.5.0.
The obsolete firmware-trust chip and its label have been removed from the source
and generated UI. Release-package signature verification remains separate.

The post-processor currently preserves:

- declarations for the externally generated Japanese 14 px and 18 px fonts;
- the CO2 marker border that remains visible after its color changes;
- the two pressure delta chip borders;
- the ambient O2 description on the optional gas information screen.
- the initial hidden state of `label_co2_warmup`; `hiddenInEditor` alone does
  not hide a widget at runtime. Its visibility follows `SensorData.co2_warmup`
  independently of the VOC/NOx and HCHO warmups.

The script is intentionally strict. If EEZ changes an expected object name or
emits an unknown value, it exits with an error instead of editing a possibly
unrelated object. Update the contract and its tests when adding another
firmware-owned override.

Run the post-processor tests with:

```powershell
python -m unittest discover -s tools/tests -p "test_*.py"
```

Do not commit `src/ui/.eez-project-build`; it is EEZ build metadata. Review the
remaining generated diff before committing because the post-processor only
protects known Project Aura invariants.

## LVGL 9 source and backup

The canonical project is `ui/aura-lvgl.eez-project`, targeting LVGL 9.5.0 with
output `../src/ui`. Source TTFs and licenses are in `ui/fonts`; bitmaps and
fonts are embedded as well. The installed EEZ Studio 0.27.1 can generate this
version. Always retain and check the independently maintained font C subsets,
including Japanese, after generation. The headless builder emits screens,
styles and images but not the font C files.

The original file in `release-assets/EEZ` was updated after a verified full
backup. Exact paths, hashes and migration changes are documented in
[IDF61_REMAINING_UPDATES_20260911.md](IDF61_REMAINING_UPDATES_20260911.md).

For an offline rendering check after native dependencies are resolved, with
CMake, Ninja and a host GCC/G++ on PATH:

```powershell
cmake -S tools/tests/lvgl9_ui -B .pio/lvgl9-ui -G Ninja
cmake --build .pio/lvgl9-ui -j 6
.pio/lvgl9-ui/aura_ui_check.exe .pio/lvgl9-ui/rendered
python tools/check_ui_font_coverage.py
```

The renderer checks the actual generated C resources and writes 15 PPM images.
It is a resource/API check, not a substitute for testing firmware runtime UI,
touch, rotation or sleep/wake on both physical profiles.

## Localized buttons and Polish

All 186 `btn_*` containers in the EEZ source use Flex centering on both axes.
Keep label height set to content so one-line and multiline translations stay
centered. `btn_language` uses `ROW_REVERSE` because its serialized children are
value, then title. The confirmation button contains three alternative labels;
firmware shows only the label for the current action. Theme buttons center their
preview card without changing its internal layout.

The 14 px and 18 px JetBrains fonts include all Polish upper- and lowercase
letters. When editing glyph sets, update the canonical source and the user's
`release-assets/EEZ/aura-lvgl.eez-project` after a verified backup. Regenerate the
font C subsets as well: the headless EEZ build does not emit those files.
`tools/check_ui_font_coverage.py` includes Polish and checks every Latin-language
string against the 18 px font, in addition to the existing status-font checks.

See [POLISH_LOCALIZATION_20260916.md](POLISH_LOCALIZATION_20260916.md) for the local
implementation, rendered layout checks and build evidence.

## RGB565 boot logo (2026-10-28)

The canonical `ui/aura-lvgl.eez-project` stores `page_boot_logo_new` as an
opaque 386 x 354 embedded PNG with native EEZ RGB565 format ID `bpp=15`.
The image remains at (207,47), the threshold-zero bounding box of the original
800 x 480 image. The boot screen explicitly paints opaque black across the
whole screen; the version label stays at (275,422). Do not stretch the image
or restore the full ARGB8888 canvas. Both firmware profiles use this resource.

This is the exact bitmap from Hub commit `0e9371c9`, transferred without Hub
UI/runtime changes. Its ordered 8 x 8 dithering comes from the pinned LVGL
converter, with exact black preserved. EEZ 9.x ignores the legacy `lvglDither`
checkbox, so quantization is already encoded in the embedded PNG. Nonzero
channels were increased by one before embedding to compensate the EEZ
`color_pre_multiply` divide-by-256 truncation at alpha 255. Do not apply
dithering again or replace this PNG with a visually similar source.

Use the normal EEZ Build, then the two postprocess commands above. No image
postprocess or manual edit of generated C is needed. For a headless build,
`EEZ Studio.exe --build-project <project> --user-data-dir=<temporary-profile>`
can build an immutable, self-contained copy of the canonical project into a
staging `src/ui`; keep embedded fonts/bitmaps and review the generated diff.
The builder does not regenerate the separately maintained font C subsets.

The generated `img_page_boot_logo_new_map` must contain 273,288 bytes of
RGB565, SHA-256
`07d6dd0775640996ccff7f9f3b366387e8e486093d7756747a34adf3d1ab3479`.
The header is RGB565, 386 x 354, stride 772. The old resource used 1,536,000
bytes: the pixel data saving is 1,262,712 bytes. Check the actual LVGL boot
render in both profiles: outside the crop it must be identical to the original
frame (black except the unchanged version label); the crop must equal the
RGB565 resource. Maximum channel error against the original ARGB pixels is
[13, 7, 14], caused by quantization/dithering.

Measured firmware sizes and reproduction evidence are in
[MAIN_LOGO_RGB565_20261028.md](MAIN_LOGO_RGB565_20261028.md).
