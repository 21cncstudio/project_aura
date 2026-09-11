"""Keep the transport backport byte-equivalent to the reviewed Espressif source."""

import hashlib
from pathlib import Path
import unittest


class LcdLegacyAdapterProvenanceTests(unittest.TestCase):
    def test_transport_source_matches_upstream_5_5_5(self):
        path = Path(__file__).resolve().parents[2] / "components/aura_lcd_i2c_legacy/esp_lcd_panel_io_i2c_v1.c"
        # Git's Windows checkout can change line endings; no other change is allowed.
        source = path.read_text(encoding="utf-8").encode("utf-8")
        self.assertEqual("fb009474a40c26d8fd7a47bc089d0b46dec31dd8459bc415adbaaee6ed66476a",
                         hashlib.sha256(source).hexdigest())
