# Shared five-minute Aura history

Implemented locally on main, 2026-09-14. Device installation, power-loss qualification,
Hub consumption, and cloud ingestion are separate integration steps.

## Data path

SensorManager marks new acquisitions, including successive equal readings. ChartsHistory
accumulates each metric once per new acquisition in UTC intervals [start, start + 300).
Its completed record is the source for the local web chart and the bounded history export.
Current values, their display filtering, and live alert evaluation keep their existing cadence.

CO2 and pressure enter history before their firmware display-smoothing filters, with the
existing calibration, validity, range, and warm-up rules. Other metrics use the acquired
values from their driver. Sensor-internal processing is unchanged. Counts refer to valid
acquisitions, not main-loop passes or seconds. The mean is an acquisition mean, not a
duration-weighted exposure metric. Hours must combine means weighted by each metric's count.

Records contain mean/min/max/count plus first/last valid sample offsets in seconds per
metric. Coverage is not a fabricated percentage: counts, offsets, missing intervals, and
the metric's acquisition cadence must be interpreted together. Missing pressure is no
longer interpolated into measured history. A failed or warming sensor contributes no
valid measurement. An unfinished interval is kept in RAM and may be lost on reset.

## Storage and time

The ring holds 288 completed records. Twenty-five rotating hourly blocks provide overlap
for the rolling day; each block contains at most twelve records and is statically bounded
to 4 KiB. A completed record causes one block to be replaced via StorageManager's existing
atomic file mechanism. Failed saves retain dirty records in RAM and retry after ten seconds.
Export does not advance past an unsaved record. Checksums reject malformed blocks at load.

The current layout is 264 bytes per entry and 3,184 bytes per hourly block on the tested
toolchains: approximately 80 KB of block payload at full retention, excluding filesystem
overhead and temporary replacement files. History and the runtime reader each allocate
76,032 bytes for entries; each simultaneous web snapshot allocates another 76,032 bytes.
These heap allocations and JSON serialization are not included in the build's static RAM
report. Hardware heap, latency, and Flash endurance still require measurement.

Legacy /charts.bin version-2 snapshots remain readable with their actual timestamps and
snapshot kind. They are not converted into invented five-minute statistics. New blocks
use /charts3_00.bin through /charts3_24.bin. Clear history/factory clear removes both forms.
The old file is preserved for migration/rollback; older firmware will not see new v3 data.

Without trusted time, a new session can show RAM history with null timestamps; it is not
exportable as absolute-time history. Restored records are not extended using untrusted time.
Backward clock corrections hold sampling until the previous observed clock catches up and
discard the affected unfinished accumulator. Completed records are not rewritten. Forward
gaps are represented as gaps, within the retained day. Acquiring trusted time starts a new
anchored series rather than assigning guessed timestamps to unanchored samples.

## Chart API

Existing /api/charts?group=core&window=24h requests keep the values array used by the UI.
Values now contain completed five-minute means. schema_version is 3; record_kind labels
summary, snapshot, or gap per slot. Actual timestamps are used for migrated snapshots.
Use stats=1 to request parallel min/max/count/first_second/last_second arrays; the ordinary
dashboard avoids this extra payload. Legacy statistics and missing metrics are null.

## Bounded read-only export

GET /api/charts?format=history&after=0 returns schema aura.history.v3 and at most eight
durably available records. Continue with after=next_after. has_more also stays true if a
record is awaiting a durable save; retry later with the same cursor. This is a read cursor,
not a Hub or cloud delivery acknowledgement. The owning Hub must persist its delivery state.

Each record has start (UTC epoch seconds), end for summaries/gaps, kind, optional_gas_type,
and metrics. Each metric has mean/min/max/count/first_second/last_second for summaries,
or value for legacy snapshots. A gap has no measured metrics. Record identity for transport
is the stable Aura device identity plus schema and start; the device identity comes from
the authenticated/pinned device association, not an arbitrary request field.

| Metric ID | Meaning | Unit |
| --- | --- | --- |
| 0 | CO2 | ppm |
| 1 | Temperature | degrees C |
| 2 | Relative humidity | percent |
| 3 | Pressure | hPa |
| 4 | CO | ppm |
| 5 | VOC | index |
| 6 | NOx | index |
| 7 | HCHO | ppb |
| 8 | PM0.5 number concentration | particles/cm3 |
| 9 | PM1 | micrograms/m3 |
| 10 | PM2.5 | micrograms/m3 |
| 11 | PM4 | micrograms/m3 |
| 12 | PM10 | micrograms/m3 |
| 13 | Optional gas | ppm, except O2 in percent |

Optional gas types are 1 NH3, 2 SO2, 3 NO2, 4 H2S, 5 O3, 6 O2. Historical records retain
their original type. The local chart filters older gas types so unlike units cannot be
drawn as one series. Hub must split serialized records to its MQTT byte ceiling and keep
live readings separate from backfill. This API does not yet wire the Hub or Link consumers.

## Validation

Native tests cover acquisition freshness, unchanged successive values, extrema and means,
per-metric validity, warm-up, interval boundaries, gaps, retention rollover, save failure and
retry, reboot, legacy migration, untrusted/backward time, optional gas identity, concurrent
snapshots, bounded export, and equality between chart values and exported means.
Build both project_aura and project_aura_7. No Flash, reset, or physical test is implied by
a passing host test or successful build.

Local verification on 2026-09-14: 71 native tests passed across history, sensor-manager,
and web-chart API suites. Both production profile builds passed, including image identity
and checksum checks. EEZ postprocess check and Git whitespace check passed. Exact local
source hashes, test/build logs, and retained candidate BINs are recorded in
`C:/Users/user/AppData/Local/Temp/aura-history-20260914/verification.json`. These candidates
have not been installed on hardware.

## Hub relay integration, 2026-09-14

The export also includes device_id (the stable aura_ EFUSE identity). A consumer
must compare it with the paired device before storing a response from its current
LAN address. Optional limit=1..8 bounds the page; the default remains eight.
The Hub requests limit=2 to bound temporary parser memory during backfill.
This metadata does not enable pairing in main: Aura-to-Hub pairing currently
exists on the separate feature/aura-link branch. A combined firmware candidate
is required before a physical end-to-end test; that older branch is not merged
into main by this history integration.
