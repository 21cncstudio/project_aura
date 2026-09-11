"""Check the pinned vendor input and the complete generated I2C adaptation."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import idf_panel_overlay


class NativePanelProvenanceTests(unittest.TestCase):
    def test_vendor_matches_reviewed_revision_except_utils_constraint(self):
        vendor = ROOT / "components/espressif__esp32_display_panel"
        hashes = json.loads((vendor / "UPSTREAM_SHA256.json").read_text())
        for name, expected in hashes.items():
            with self.subTest(name=name):
                data = (vendor / name).read_bytes()
                if name == "idf_component.yml":
                    self.assertEqual(1, data.count(b'"==0.3.0"'))
                    data = data.replace(b'"==0.3.0"', b'"0.2.*"')
                self.assertEqual(expected, hashlib.sha256(data).hexdigest())

    def test_native_overlay_uses_bus_handles_and_finite_lcd_timeout(self):
        vendor = ROOT / "components/espressif__esp32_display_panel"
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            with patch.object(sys, "argv", ["idf_panel_overlay.py", "--source", str(vendor), "--destination", str(destination)]):
                idf_panel_overlay.main()
            source = (destination / "src/drivers/bus/esp_panel_bus_i2c.cpp").read_text()
            self.assertIn("esp_lcd_new_panel_io_i2c(bus, &panel_config", source)
            self.assertIn("panel_config.transaction_timeout_ms = 50", source)
            self.assertNotIn("esp_lcd_new_panel_io_i2c_v1", source)
            host = (destination / "src/drivers/host/esp_panel_host_i2c.cpp").read_text()
            self.assertIn("aura_i2c_start", host)
            self.assertIn("aura_i2c_stop", host)
            self.assertNotIn("i2c_driver_install", host)
