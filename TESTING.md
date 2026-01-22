# Testing

## Native tests (host)
Prereqs:
- MSYS2 with `mingw-w64-x86_64-toolchain` (gcc available in PATH)
- PlatformIO CLI (`pio` on PATH or `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`)

Run from repo root:
```powershell
& $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe test -e native_test
```

Or use the helper script:
```powershell
.\scripts\run_tests.ps1
```
