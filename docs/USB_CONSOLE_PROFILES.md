# USB console profiles

Date: 2026-08-30

Both production profiles select native USB CDC for `Serial`. The 4.3-inch
transition needs a new physical test; the existing `fac6e30` UART capture does
not validate it. This document defines the checks and authorizes no port open,
logger stop, reset, flash, cable move, or publication.

## Profile contract

| Property | 4.3-inch | 7-inch |
| --- | --- | --- |
| Environment | `project_aura` | `project_aura_7` |
| Hardware profile | `4_3` | `7_dual_i2c_scl6` |
| Hardware target | `aura-aq-v1` | `aura-aq-7-v1` |
| Console | Native USB CDC, USB Type-C connector | Native USB CDC, Type_C2 |
| `ARDUINO_USB_CDC_ON_BOOT` | `1` | `1`, inherited |
| CH422G initial `WR_IO` | `0xDF` | `0xD1`, unchanged |
| Panel I2C | I2C0, SDA8/SCL9, 100 kHz | I2C0, SDA8/SCL9, 100 kHz |
| Sensor I2C | Shared I2C0, SDA8/SCL9, 100 kHz | I2C1, SDA44/SCL6, 100 kHz |

Waveshare documents separate USB and USB-to-UART connectors on both boards.
Native USB uses GPIO19/20; EXIO5 (`USB_SEL`) low selects USB and high selects
CAN. These are reference-board connections, not a measurement of either test
unit. Sources checked 2026-08-30: [4.3-inch hardware documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3)
and [7-inch hardware documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7).

CAN remains unused. The migration changes neither model identity nor I2C
routing, frequency, pull-ups, panel timing, or sensor configuration. On the
7-inch board, keep GPIO43/H3 EX_TXD open from sensor SCL, use GPIO6 for SCL,
and retain SW1 at UART2 while H3 EX_RXD carries SDA44.

## CH422G startup policy

Both environments use `third_party/ESP32_IO_Expander_Aura`, the renamed shared
copy of the reviewed upstream v1.1.0 fork. See its
[patch record](../third_party/ESP32_IO_Expander_Aura/AURA_PATCH.md).
`include/Ch422gBoardPolicy.h` supplies the initial image to the driver and the
disabled legacy probe.

The 4.3-inch image is `0xFF & ~0x20 = 0xDF`: only USB_SEL changes from the
former upstream image. It does not copy the 7-inch LCD/touch/backlight levels.
The 7-inch image remains `0xD1`. Both use this startup order:

```text
WR_OC  0x23 <- 0x0F
WR_IO  0x38 <- 0xDF (4.3-inch) or 0xD1 (7-inch)
WR_SET 0x24 <- 0x01
```

Preload the output image before enabling IO0-7. Stop on a failed write without
retry. This migration adds no recovery actions, forced runtime writes, waits
for a host monitor, or changes to `Logger` behavior. Native tests and builds
check software policy; they do not establish USB enumeration or board health.

## Controlled migration and qualification

The operator must authorize physical actions at the relevant stage. Keep the
7-inch unit and its existing COM10 capture untouched until the 4.3-inch basic
check passes. COM8 remains outside scope.

1. **Preserve evidence and identify the candidate.** Archive a bounded copy of
   each existing capture before an agreed logger handoff. Record source commit,
   build ID, profile, target, BIN SHA-256, fitted sensors, board identity, and
   power/cable arrangement. Keep the historical `fac6e30` evidence separate.
2. **First flash through the existing 4.3-inch UART path.** After flash and
   logger-handoff approval, reverify the physical 4.3-inch unit and its current
   USB-to-UART identity, recorded as COM5 in the earlier capture. Write the
   matching application through that path. Preserve NVS/LittleFS; do not use
   erase-all. Label the resulting restart as upload/reset evidence. The new
   application routes `Serial` to native USB, so UART silence alone does not
   determine success or failure.
3. **Move the cable with power removed.** On instruction, the user removes
   every power source, including any battery or external supply. Leave the USB
   cable disconnected from its host/supply, then move its board end from
   USB-to-UART to the native USB connector. Wait for the user's confirmation
   before directing power-on. Keep sensor wiring unchanged.
   Identify the new native port from the physical unit and before/after USB
   enumeration, including VID/PID and serial identity. Do not guess its COM
   number or select the 7-inch port because it has the same VID/PID.
4. **Pass the 4.3-inch basic check.** Start with serial monitors closed and
   auto-connect disabled. Confirm the first frame, screen, touch/navigation,
   and readings expected from the fitted sensors. Verify the running
   build/profile from device evidence; if a late-open capture lacks startup
   identity, use a separate approved status check. After an agreed capture
   step, use one receive-only open of the discovered native connection without
   intentional reset or automatic reconnect. Record any effect of opening the
   monitor. An absent optional probe and a failed fitted sensor need different
   conclusions.
5. **Test native reflashing and cold boots as separate cases.** With approval,
   reflash the matching 4.3-inch BIN through native USB and record reset and
   re-enumeration behavior. Discover the port again after enumeration changes.
   Then run controlled physical OFF/ON cases, including a monitor-absent boot,
   and record screen/touch/sensor results. Do not count an upload reset,
   software restart, EN/RTS event, or USB reconnect as a cold boot.
6. **Proceed to 7-inch testing after the 4.3-inch basic pass.** Obtain the
   separate device/flash approval, preserve its capture, and reverify its
   native identity before using the port recorded as COM10. Keep the existing
   SDA44/SCL6 wiring and `0xD1` policy. Apply the native reflash, reset, and
   monitor-absent checks, then complete the
   [7-inch qualification record](releases/SEVEN_INCH_QUALIFICATION.md).
7. **Verify built-in OTA on both models.** On guarded firmware, submit the
   opposite-model BIN and an unlabelled legacy BIN. For each rejection record
   the inline error, `written:0`, `rebooting:false`, continuous uptime, unchanged build,
   and restored UI. Follow with a successful matching-model OTA and verify the
   new running build after its expected restart. Repeat in the other model
   direction. Use the [OTA guard contract](OTA_HARDWARE_TARGET_GUARD.md);
   local tests do not replace these authorized device checks.

## Recovery and evidence limits

Keep the USB-to-UART recovery path available. A rollback to older firmware can
select CAN or UART logging again and require moving back to USB-to-UART. On
7-inch hardware, UART switch/sensor-routing changes need their own power-off
instruction; do not change SW1 while treating the sensor setup as unchanged.
Preserve failure evidence before any approved recovery. Use a matching
application recovery image without erase-all.

Record exact test boundaries and close/hash each capture after an agreed stop.
`capture_active state=connected` describes the logger connection, not a live
screen/touch/sensor assessment. Capture headers naming an expected build are
operator metadata, not a received firmware identity. Neither an open capture
nor a successful local build qualifies a public release. Keep the existing
installer, Aura Link, and 7-inch publication gates closed.
