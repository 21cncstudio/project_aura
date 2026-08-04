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
already satisfied. A normal PlatformIO firmware build also invokes the same
post-processor before compilation as a final safety net.

The post-processor currently preserves:

- declarations for the externally generated Japanese 14 px and 18 px fonts;
- the CO2 marker border that remains visible after its color changes;
- the two pressure delta chip borders;
- the fail-safe `UNVERIFIED FW` initial trust label when that settings block is
  present in the generated UI;
- the ambient O2 description on the optional gas information screen.

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
