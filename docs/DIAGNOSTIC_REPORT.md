# Diagnostics page and report

`/diag` provides **Download report**, a local JSON export of fresh responses from
`GET /api/diag` and `GET /api/events`. It does not upload data, clear logs, scan
I2C, change settings, or restart the device. Review network names, addresses,
hostnames and log text before sharing the file.

## Report format and limits

The report uses `schema: "aura-diag-report"`, `schema_version: 1`. Each section
has its endpoint, browser-clock start/finish timestamps and `ok` flag. Successful
sections retain the complete API payload under `data`; failed sections have an
`error`. The APIs are requested sequentially, with an eight-second timeout per
request including its body. An ordinary page poll already in progress finishes
before fresh report capture begins. Polling pauses during capture.

If one API fails, the report is marked `complete: false`, includes warnings and
uses `_partial.json` in its filename. If both fail, no file is offered. A decrease
in device uptime between responses also marks the report partial. `complete`
means both sections were received without an observed uptime decrease; it does
not prove the sections belong to one boot. There is no atomic capture or unique
boot identity in this format. Browser download success still depends on the
browser accepting the download; the page says "Download started", not "Saved".

These APIs expose finite, filtered recent buffers, not complete serial output:
currently at most 48 events and 12 alerts. `error_count` is the returned alert
count, not a lifetime counter. Soft sensor warnings and low-level driver output
may be absent. A quiet report is not a physical screen/touch or runtime-health
PASS. Keep the two API payloads separate; do not treat repeated entries in both
as two independent incidents.

## Startup I2C interpretation

The existing `boot.i2c_status`, `boot.sda_high` and `boot.scl_high` keys retain
their original names and raw values. They describe the saved panel-bus sample
before board initialization. In the current configuration, pre-init bus
recovery is disabled; a single LOW sample can produce the legacy string
`sda_stuck_low`. It does not establish a persistent fault during later operation.
Do not rewrite that value as `ok` merely because the device later initialized.

`boot.i2c_snapshot` explains those fields with `phase: "before_board_init"`,
`bus: "panel"`, `live: false`, and the port/SDA/SCL GPIO numbers.
`i2c_buses` reports compile-time routing only, not live pin measurements:

| Profile | Panel bus | Sensor bus | Shared |
| --- | --- | --- | --- |
| `4_3` | I2C0, SDA8/SCL9 | I2C0, SDA8/SCL9 | Yes |
| `7_dual_i2c_scl6` | I2C0, SDA8/SCL9 | I2C1, SDA44/SCL6 | No |

`device` includes `firmware`, `build_id`, `hardware_profile` and `hardware_target`
from the existing AppVersion accessors. Build IDs that contain `dirty` are not
unique artifact identities; retain the BIN SHA256 with hardware test records.

## Local checks

- Native: `python -m platformio test -e native_test -f test_web_diag_api_utils`.
- Frontend: `node --test tools/tests/test_diag_report.mjs`. These tests execute
  the actual embedded page script with simulated DOM, network and download APIs.
- Compile both normal environments: `project_aura` and `project_aura_7`.
- Browser fixtures and native tests do not qualify an installed firmware image.

The later split-profile decision keeps `project_aura` at GT911 `0x14` and selects
`0x5D` for normal `project_aura_7`. The diagnostic 7-inch environment uses the
same address with additional bounded startup reads. Diagnostics do not change
polling, clocks, power or routing.

### Browser file verification, 2026-08-31

The actual embedded page was served on localhost with synthetic responses.
The in-app browser saved a complete 7-inch report and a partial 4.3-inch report
with an intentional events HTTP 503. Both downloaded JSON files were found on
disk and validated for schema, sections, routing, flags and preserved text.
The download-event observer still timed out on the complete report; the actual
file establishes saving independently of that observer. This closes the earlier
local file-saving verification gap, not hardware HTTP or optical acceptance.

Files, hashes and assertions are preserved under
`D:\21cncstudio\project_aura\logs\beta_local_completion_20260831T182528Z\browser-downloads`.
No report from a real device was fetched or shared in this check.

## Display and GT911 startup fields, 2026-09-01

`display.available` now describes whether the LVGL port created a display
diagnostic lifecycle. It is independent of `boot.lvgl_ready`: if the display
started and later fail-stopped during UI startup, its counters remain available
for the report. It is false before display initialization and after a failed
partial initialization was cleaned up.

The display object includes refresh-callback semantics/count/age/max gap,
framebuffer hand-off and wait-timeout counts, the latched sync-fault flag,
runtime lock failures, startup-logo lock misses, touch read/offline state,
configured 180-degree flip, active rotation pipeline, rotated copy count,
framebuffer ownership violations and pause state. With the current RGB/bounce
configuration, `refresh_callback_semantics` is not a physical VSYNC measurement.
Zero counters and a quiet report do not prove optical output.

`boot.gt911_startup` is a bounded structured snapshot. Production builds set
`captured: false` because they intentionally omit the additional ID/config and
alternate-address reads. The diagnostic 7-inch build records those reads without
turning a successful configured `0x5D` result into a warning. `captured: false`
does not mean that the compile-time address is unknown.
