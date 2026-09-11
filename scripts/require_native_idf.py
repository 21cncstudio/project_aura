"""Reject the obsolete LVGL 8 firmware toolchain while retaining profile metadata."""
Import("env")
raise RuntimeError(
    "Aura firmware in this branch requires ESP-IDF 6.1 / LVGL 9.5. "
    "Use scripts/build_idf.ps1 -Profile 4_3 or -Profile 7_dual_i2c. "
    "PlatformIO native tests remain available through scripts/run_tests.ps1."
)
