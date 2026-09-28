# SEN0321 reference and Aura integration audit

Retrieved and reviewed: 2026-09-26. Firmware inspected: local main at
`a2891b7443e8f8778a35162021924c6639e6227e`, including its existing working tree.
The local SEN0321 implementation and both firmware profile builds are now complete.
Hardware behavior is unverified; initial testing is planned with community tester
fwa_be. No device was flashed and no external message or release was published.
See [implementation and validation](../../SEN0321_INTEGRATION_20260926.md).

## Official sources retained

- `SEN0321_iic-ozone-sensor_datasheet_v1.pdf`: linked by the official
  [SEN0321 wiki](https://wiki.dfrobot.com/sen0321/).
  [Direct source](https://dfimg.dfrobot.com/wiki/18659/SEN0321_iic-ozone-sensor_datasheet_v1.pdf).
  The actual document is Winsen ZE27-O3, Chinese, version 1.0, dated 2017-11-30,
  six pages. It describes the internal UART module, not the Gravity board's
  external I2C register interface.
- `SEN0321_iic-ozone-sensor_schematics_v1.pdf`: official DFRobot Gravity board
  schematic, V1.0, dated 2020-01-07, one page.
  [Direct source](https://dfimg.dfrobot.com/wiki/18659/SEN0321_iic-ozone-sensor_schematics_v1.pdf).
- `DFRobot_OzoneSensor_reference/`: unmodified reference source and license,
  version 1.0.1, pinned to upstream commit
  `bf073b730981c0e4fc4a6e859173cee4edd8c1fe`.
  This is reference material only, not a compiled dependency.
  `SOURCE.json` records the origin and file paths.
- [Official I2C wiring, address table and example](https://wiki.dfrobot.com/sen0321/docs/18879).

SHA256SUMS.txt records the exact downloaded PDF and library files.
The local PDFs were opened, parsed and visually inspected (datasheet technical
pages 3-6 and the complete schematic).

## Source boundaries

Use the Winsen document for sensor properties, limitations, stabilization and
its internal UART protocol. Use DFRobot's board specification, schematic and
pinned library for the external SEN0321 I2C interface and supply requirements.
Do not send the UART frames directly as SEN0321 I2C commands.
The bare ZE27-O3 voltage specification differs from the assembled DFRobot board.

The datasheet specifies 0-10 ppm range, 0.01 ppm resolution, preheat <=3 minutes,
response/recovery <=90 seconds, and first-use stabilization of 24-48 hours.
It lists alcohol and other gases as interference and cautions about organic
solvents. It does not specify a numerical measurement accuracy or a 20 ppb floor.
The community's 20 ppb floor and 3-6 ppb station comparison remain observations,
not manufacturer guarantees or a calibrated error bound.

## I2C address audit

DFRobot documents four seven-bit addresses: 0x70, 0x71, 0x72 and 0x73.
The board default/example is 0x73 (A0=1, A1=1). Pass this address explicitly:
the current library's begin() default argument is 0x70.

Current Aura addresses from src/config/AppConfig.h and board configuration:

| Device | Address |
| --- | --- |
| SEN66 / SEN69C family | 0x6B |
| SFA30 / SFA40 | 0x5D |
| SEN0466 CO | 0x74 |
| DFR optional gas slot, including SEN0472 | 0x75 |
| PCF8523 / DS3231 RTC alternatives | 0x68 |
| DPS310 / BMP3xx pressure alternatives | 0x76 / 0x77 |
| BMP580 | 0x46 / 0x47 |
| GP8403 DAC default | 0x58 |
| GT911 touch | 0x14 on 4.3-inch, 0x5D on 7-inch panel bus |
| Board expander configured base | 0x20 |

No current Aura device address conflicts with 0x70-0x73. This is a source audit,
not a live scan of fwa_be's wiring or proof about additional custom peripherals.
Use the existing sensor bus: GPIO8/9 for 4.3-inch; GPIO44/6 for 7-inch dual-I2C.
Keep current 100 kHz bus configuration.

The DFRobot schematic connects the connector-side SDA/SCL pull-ups (R1/R2,
10 kohm) to VCC. For direct connection to Aura's 3.3 V bus, power the assembled
SEN0321 board at 3.3 V, within DFRobot's stated 3.3-5.5 V board specification.
Do not transfer the bare internal module's supply requirements to the connector.

## I2C implementation reference

The pinned library uses register 0x03 for automatic/passive mode (0x00/0x01),
register 0x04 for requesting data, and two-byte big-endian ppb readings beginning
at 0x07 (passive) or 0x09 (automatic). It includes 100 ms waits between operations.
An Aura driver should preserve that transaction ordering without blocking the
shared sensor polling loop for those waits. Native readings are in ppb; if the
existing internal ppm model is retained, conversion is /1000.0f and presentation
must convert back to ppb with matching units. User-facing SEN0321 values use ppb.
The current SEN0472 driver uses a different framed protocol; changing only its
address is insufficient.

The reference begin() checks ACK only; it does not establish a unique model ID.
The library does not propagate write errors or require exactly two returned
bytes. A robust adapter must treat failed/short reads as invalid, not zero O3.
Do not copy reference averaging/global state or its validation weaknesses blindly.

## Reuse the existing Aura presentation

The existing data model has one optional-gas slot, separate from CO. SEN0321
will supply that same slot with gas type O3. The user explicitly chose ppb for
this sensor on 2026-09-26: for example, display 20 ppb rather than 0.02 ppm.
This supersedes the initial audit proposal to display SEN0321 in ppm.

Verified consumers that must use consistent values and units:

- SensorManager.cpp: sync_optional_gas_fields and METRIC_OPTIONAL_GAS freshness.
- AppData.h: optional_gas_ppm, type, decimals, present, warmup and valid fields.
- UiControllerSensors.cpp / UiControllerInfo.cpp: existing tile and details.
- UiControllerGraphsGas.cpp / UiOptionalGasProfile.cpp: existing O3 graph/profile.
- ChartsHistory.cpp / DailyExtremaHistory.cpp: optional-gas history and extrema.
- WebStateApiUtils.cpp / WebTemplatesDashboardAp.cpp: API and web dashboard.
- MqttPayloadBuilder.cpp / MqttManager.cpp: existing optional_gas and o3 values
  and Home Assistant discovery entities.

Use ppb consistently for SEN0321's screen values, graph axes and statistics,
threshold labels, web presentation and externally published readings with unit
metadata. Convert threshold numbers together with their units; a unit change
must not change the physical thresholds. Retain the existing units for SEN0472
and other optional-gas modules. Add model/source identity where needed to choose
units; gas type O3 alone cannot distinguish these two models.

The current internal fields and stored history are based on ppm. Retaining that
canonical representation is an implementation option, provided conversion at
presentation/API boundaries is explicit and consistent. Do not reinterpret
existing stored ppm samples as ppb, put ppb numbers into fields that promise ppm,
or change an existing MQTT value's scale while leaving its unit/discovery stale.
Review Home Assistant unit changes and historical-data compatibility together.
The reviewed implementation retains the unitless legacy Optional Gas entity and
its scale via `optional_gas_canonical`; the dedicated O3 entity and display MQTT
fields use ppb for SEN0321. See the integration record for this exception.

SensorManager now uses OptionalGasSensor to select one source and retain the same
present/valid/warmup and fresh-sample paths. Stored type ID 7 identifies SEN0321;
existing type IDs 0-6 and their units are unchanged.

## User-agreed scope, 2026-09-26

- Keep SEN0472 support and reuse the existing UI/history/web/MQTT locations.
- Present SEN0321 in ppb; retain existing units for the other supported modules.
- Retain one optional-gas sensor as the product design, not only for the first
  test: either SEN0321 or the existing optional module at 0x75. There is no
  requirement for concurrent display or separate history for both sensors.
- Use SEN0321's documented default address 0x73 for the initial integration.
- Initial hardware verification will be performed with fwa_be.

## Hardware validation and limitations

- Simultaneous installation of SEN0321 and a module at 0x75 is outside the agreed
  supported configuration. Handle it deterministically and expose the condition
  in diagnostics; do not combine values or alternate the history source on
  transient read failures. No dual-sensor selection UI is required by this scope.
- Keep the observed 20 ppb floor separate from guaranteed specifications; do not
  subtract 20 ppb or interpret a reading of 20 as a guaranteed <=20 result.
- Confirm fwa_be's display profile and present firmware before preparing a
  profile-specific test binary. Device behavior remains unverified locally.
- The user explicitly selected integer ppb (52, 53, 55) without rounding to tens.
  Display precision does not establish 1 ppb measurement resolution or accuracy.
- Local implementation, host tests and both IDF builds passed; see the linked
  integration record and validation-20260926.json for artifact hashes and evidence.
- Hardware behavior, the real I2C output granularity, and Home Assistant unit
  migration/history still need verification on the tester's installation.
