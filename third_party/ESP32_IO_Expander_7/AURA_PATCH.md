# Project Aura 7-inch CH422G patch

This directory contains the runtime sources from Espressif
`ESP32_IO_Expander` v1.1.0, upstream commit
`e79a63876a1d8a834cf8ec8f8b698ff9d9374579`.

The upstream license is preserved in `license.txt`. The SHA-256 of the
unmodified upstream `src/port/esp_io_expander_ch422g.c` is:

```text
44BD59BFBE6790D3A75F118C9FC0F3D040D6BC44AA191AC4E47B77D7EC164292
```

Only the CH422G reset image and reset order are changed:

- `WR_IO` defaults to `0xD1`, keeping EXIO5 (`USB_SEL`) low so GPIO19/20
  remain connected to the board's native Type_C2 port.
- The output image is written before IO0-7 are enabled.

The resulting initial transaction order is:

```text
WR_OC  address=0x23 data=0x0F
WR_IO  address=0x38 data=0xD1
WR_SET address=0x24 data=0x01
```

There are no retries, recovery actions, forced writes, mutex changes, or
changes to later CH422G operations. The normal 4.3-inch firmware continues to
use the untouched official dependency and never builds this directory.
