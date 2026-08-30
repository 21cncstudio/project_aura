"""Fail firmware builds if an external-sensor driver uses the panel I2C constants."""

from pathlib import Path
import re

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))
sensor_sources = sorted((project_dir / "src" / "drivers").glob("*.cpp"))
sensor_sources.append(project_dir / "src" / "core" / "I2CHelper.cpp")
forbidden = re.compile(
    r"Config::I2C_(?:PORT|SDA_PIN|SCL_PIN|FREQ_HZ|TIMEOUT_MS)\b"
)
violations = []

for source in sensor_sources:
    text = source.read_text(encoding="utf-8")
    for line_number, line in enumerate(text.splitlines(), start=1):
        match = forbidden.search(line)
        if match is not None:
            violations.append(
                f"{source.relative_to(project_dir)}:{line_number}: {match.group(0)}"
            )

if violations:
    details = "\n".join(f"  {item}" for item in violations)
    raise RuntimeError(
        "External-sensor I2C code must use Config::SENSOR_I2C_*:\n" + details
    )

print(
    f"[sensor-i2c-routing] checked {len(sensor_sources)} source files; routing is clean"
)
