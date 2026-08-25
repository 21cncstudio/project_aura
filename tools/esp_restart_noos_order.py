"""Pure validation helpers for the ESP32-S3 restart backport."""

from dataclasses import dataclass


class RestartOrderError(ValueError):
    """Raised when the linked restart path does not contain the safe order."""


@dataclass(frozen=True)
class FunctionSymbol:
    """Address range and output section for a linked function."""

    address: int
    size: int
    section: str


def parse_function_symbol(symbol_table: str, symbol: str) -> FunctionSymbol:
    """Parse one exact function from ``objdump -t -C`` output."""

    matches = []
    for line in symbol_table.splitlines():
        fields = line.split(None, 5)
        if len(fields) != 6 or fields[2] != "F" or fields[5] != symbol:
            continue
        try:
            address = int(fields[0], 16)
            size = int(fields[4], 16)
        except ValueError:
            continue
        matches.append(FunctionSymbol(address, size, fields[3]))

    if len(matches) != 1:
        raise RestartOrderError(
            f"expected one linked function {symbol}, found {len(matches)}"
        )
    if matches[0].size <= 0:
        raise RestartOrderError(f"linked function {symbol} has zero size")
    return matches[0]


def validate_restart_order(wrapper_disassembly: str,
                           restart_disassembly: str,
                           panic_disassembly: str,
                           wrapper_section: str) -> None:
    """Require the fixed cross-core ordering and active linker interposition."""

    symbol = "__wrap_esp_restart_noos"
    if f"<{symbol}>:" not in wrapper_disassembly:
        raise RestartOrderError(f"missing linked symbol {symbol}")
    if not wrapper_section.startswith(".iram"):
        raise RestartOrderError(
            f"{symbol} must execute from IRAM, found section {wrapper_section}"
        )

    ordered_targets = (
        "<esp_rom_software_reset_cpu>",
        "<esp_cpu_stall>",
        "<Cache_Disable_ICache>",
        "<Cache_Disable_DCache>",
    )
    positions = []
    for target in ordered_targets:
        position = wrapper_disassembly.find(target)
        if position < 0:
            raise RestartOrderError(f"missing restart target {target}")
        positions.append(position)

    if positions != sorted(positions) or len(set(positions)) != len(positions):
        raise RestartOrderError(
            "unsafe restart order: expected reset -> stall -> I-cache off -> "
            "D-cache off"
        )

    for caller_name, disassembly in (
        ("esp_restart", restart_disassembly),
        ("panic_restart", panic_disassembly),
    ):
        if "<__wrap_esp_restart_noos>" not in disassembly:
            raise RestartOrderError(
                f"{caller_name} is not routed through __wrap_esp_restart_noos"
            )
