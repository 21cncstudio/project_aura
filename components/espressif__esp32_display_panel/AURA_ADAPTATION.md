# Aura native Display Panel integration

Runtime source: Espressif ESP32_Display_Panel commit
`92b790ed6d24b0678e2f45b1fc85f0abd2d41b33` (upstream version field 1.0.5).
This is the same pinned revision already used by Aura. The latest published
tag at the 2026-09-11 audit was 1.0.4; this change does not downgrade the source.

`UPSTREAM_SHA256.json` records the original bytes before adapting the dependency
manifest. The upstream Apache-2.0 license and source copyright headers remain.

The local component makes the native dependency on esp-lib-utils **0.3.0**
explicit. Upstream's manifest restricts it to 0.2.x, preventing the requested
coordinated update. No version is misreported to the dependency solver.
The optional utils plugin registry is disabled; Aura does not use it.

The source tree stays unchanged. `scripts/idf_panel_overlay.py` checks hashes
and compiles the existing IDF 6 RGB adaptations plus the new I2C master API
adaptation in the build directory. Any changed patch input fails configuration.
`scripts/idf_i2c_adapt.py` carries the I2C changes. The native LCD IO driver is
provided by ESP-IDF 6.1; the old LCD/legacy-I2C component has been removed.

Keeping the component in the repository allows a fresh checkout to resolve
the reviewed manifest before CMake generates the source overlay. Generated or
downloaded managed-component files are never patched to change dependencies.

Git preserves original vendor bytes, including upstream whitespace. The full
vendor SHA-256 test is the provenance gate; whitespace normalization is not
applied to this directory.
