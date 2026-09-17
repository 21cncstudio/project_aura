from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from idf_i2c_linkage import REQUIRED, FORBIDDEN, validate_i2c_linkage


def table(names):
    return "\n".join(f"42001000 g     F .flash.text 00000100 {name}" for name in names)


class IdfI2cLinkageTests(unittest.TestCase):
    def test_master_driver_without_legacy_is_valid(self):
        validate_i2c_linkage(table(REQUIRED) + "\n00000000 w *UND* 00000000 i2c_acquire_bus_handle")

    def test_each_legacy_driver_entry_is_rejected(self):
        for name in FORBIDDEN:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "legacy-driver symbols"):
                validate_i2c_linkage(table(REQUIRED | {name}))

    def test_missing_master_or_lcd_entry_is_rejected(self):
        for name in REQUIRED:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "missing"):
                validate_i2c_linkage(table(REQUIRED - {name}))
