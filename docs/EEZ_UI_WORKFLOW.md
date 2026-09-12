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
Firmware on this migration branch is built with `scripts/build_idf.ps1`.

The post-processor currently preserves:

- declarations for the externally generated Japanese 14 px and 18 px fonts;
- the CO2 marker border that remains visible after its color changes;
- the two pressure delta chip borders;
- the fail-safe `UNVERIFIED FW` initial trust label when that settings block is
  present in the generated UI;
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
