import unittest

from tools.esp_restart_noos_order import (
    FunctionSymbol,
    RestartOrderError,
    parse_function_symbol,
    validate_restart_order,
)


def wrapper_with(*targets: str, section: str = ".iram0.text") -> str:
    body = "\n".join(f"  call {target}" for target in targets)
    return (
        f"Disassembly of section {section}:\n"
        "40370000 <__wrap_esp_restart_noos>:\n"
        f"{body}\n"
    )


SAFE_TARGETS = (
    "<esp_rom_software_reset_cpu>",
    "<esp_cpu_stall>",
    "<Cache_Disable_ICache>",
    "<Cache_Disable_DCache>",
)


class EspRestartNoosOrderTests(unittest.TestCase):
    def test_parses_exact_function_range_and_section(self):
        table = (
            "4037572c g     F .iram0.text 00000187 "
            "__wrap_esp_restart_noos\n"
            "420f3d30 g     F .flash.text 00000022 esp_restart\n"
        )
        self.assertEqual(
            FunctionSymbol(0x4037572C, 0x187, ".iram0.text"),
            parse_function_symbol(table, "__wrap_esp_restart_noos"),
        )

    def test_rejects_missing_function_symbol(self):
        with self.assertRaisesRegex(RestartOrderError, "found 0"):
            parse_function_symbol("", "__wrap_esp_restart_noos")

    def test_accepts_safe_iram_wrapper_and_linker_routing(self):
        validate_restart_order(
            wrapper_with(*SAFE_TARGETS),
            "40371000 <esp_restart>:\n  call <__wrap_esp_restart_noos>\n",
            "40372000 <panic_restart>:\n  call <__wrap_esp_restart_noos>\n",
            ".iram0.text",
        )

    def test_rejects_the_idf_5_3_2_cache_first_order(self):
        unsafe = (
            "<Cache_Disable_ICache>",
            "<Cache_Disable_DCache>",
            "<esp_rom_software_reset_cpu>",
            "<esp_cpu_stall>",
        )
        with self.assertRaisesRegex(RestartOrderError, "unsafe restart order"):
            validate_restart_order(
                wrapper_with(*unsafe),
                "call <__wrap_esp_restart_noos>",
                "call <__wrap_esp_restart_noos>",
                ".iram0.text",
            )

    def test_rejects_flash_resident_wrapper(self):
        with self.assertRaisesRegex(RestartOrderError, "must execute from IRAM"):
            validate_restart_order(
                wrapper_with(*SAFE_TARGETS),
                "call <__wrap_esp_restart_noos>",
                "call <__wrap_esp_restart_noos>",
                ".flash.text",
            )

    def test_rejects_missing_normal_restart_routing(self):
        with self.assertRaisesRegex(
            RestartOrderError, "esp_restart is not routed"
        ):
            validate_restart_order(
                wrapper_with(*SAFE_TARGETS),
                "40371000 <esp_restart>:\n  call <esp_restart_noos>\n",
                "call <__wrap_esp_restart_noos>",
                ".iram0.text",
            )

    def test_rejects_missing_panic_restart_routing(self):
        with self.assertRaisesRegex(
            RestartOrderError, "panic_restart is not routed"
        ):
            validate_restart_order(
                wrapper_with(*SAFE_TARGETS),
                "call <__wrap_esp_restart_noos>",
                "40372000 <panic_restart>:\n  call <esp_restart_noos>\n",
                ".iram0.text",
            )


if __name__ == "__main__":
    unittest.main()
