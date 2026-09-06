# Testing

## Native tests (host)

Prerequisites:

- MSYS2 with `mingw-w64-x86_64-toolchain`.
- PlatformIO installed in `%USERPROFILE%\.platformio\penv`.

Recommended entry point, from the repository root:

```powershell
.\scripts\run_tests.ps1
```

The helper runs all ten native Unity environments: the common suite, four
dedicated sensor-driver suites, the 4.3-inch and 7-inch I2C topology/routing
suites, CH422G reset/probe suites for both profiles, and
`native_test_startup_probe_policy`.
It uses the canonical PlatformIO Python with isolated Python startup, records
the observed Python/PlatformIO/compiler identity, and separates build caches by
runtime and selected suites. It does not require one hard-coded Python or
PlatformIO version. Supply `-CompilerDir` if the MSYS2 compiler is elsewhere.

For a targeted iteration:

```powershell
.\scripts\run_tests.ps1 -Environment native_test -Filter test_web_events_utils -Verbosity 1
```

Keep the run's `.pio/native-tests/reports/<run-id>/launcher.json` and PIO JSON
reports. Acceptance requires `launcher.json` status `PASSED`, zero subprocess
exits, and verified Unity cases from every selected suite's real source files.
A suite heading or a `PASSED` summary alone is insufficient: a shared PIO cache
can replay another suite's executable after changing runtimes or filters.
The launcher rejects mismatched case sources and missing selected suites.

The low-level command remains available for diagnosis, but does not provide
the launcher's identity, cache-isolation, and case-source checks:

```powershell
& $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe test -e native_test
```

The launcher only starts host-side PlatformIO tests. It does not open serial
ports, flash a controller, or interact with connected I2C hardware.
