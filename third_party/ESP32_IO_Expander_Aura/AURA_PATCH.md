# Project Aura native-USB CH422G patch

This directory contains the runtime sources from Espressif
`ESP32_IO_Expander` v1.1.0, upstream commit
`e79a63876a1d8a834cf8ec8f8b698ff9d9374579`.

The upstream license is preserved in `license.txt`. The SHA-256 of the
unmodified upstream `src/port/esp_io_expander_ch422g.c` is:

```text
44BD59BFBE6790D3A75F118C9FC0F3D040D6BC44AA191AC4E47B77D7EC164292
```

Both hardware profiles now use this one copy. The directory was originally
named `ESP32_IO_Expander_7`; the upstream runtime sources were not replaced when
it was renamed. Only the CH422G reset image and reset order are changed:

- `WR_IO` comes from the C-compatible `include/Ch422gBoardPolicy.h`: `0xDB`
  for 4.3-inch and the unchanged `0xD1` for 7-inch. Both keep EXIO5 (`USB_SEL`)
  low so GPIO19/20 remain connected to the board's native USB port.
- The 4.3-inch image clears EXIO5 and EXIO2 relative to upstream `0xFF`.
  EXIO2 now starts low for deferred backlight enable; the other levels are
  unchanged from the native-USB `0xDF` image. It does not inherit the 7-inch
  LCD/touch startup levels. Both profiles keep the backlight off at reset.
- The output image is written before IO0-7 are enabled.

The resulting initial transaction order is:

```text
WR_OC  address=0x23 data=0x0F
WR_IO  address=0x38 data=0xDB (4.3-inch) or 0xD1 (7-inch)
WR_SET address=0x24 data=0x01
```

There are no retries, recovery actions, forced writes, mutex changes, or
changes to later CH422G operations. The optional CH422G probe uses the same
policy header, so it cannot restore the upstream all-HIGH/CAN image.

Both production profiles and both dedicated native-test profiles build this
directory. Native tests verify each exact reset image/order, failure exits,
and USB selection through subsequent masked output changes. CAN is not used
by either production profile. This is a software policy, not a physical lock
on EXIO5, and a different firmware can select CAN again.
