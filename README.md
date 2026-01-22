# Project Aura

<!-- TODO: Replace badges with the exact set you already shared -->
[![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-ff7f2a?logo=platformio&logoColor=white)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/target-ESP32--S3-323330)](https://www.espressif.com/en/products/socs/esp32-s3)
[![LVGL](https://img.shields.io/badge/ui-LVGL-00b0f0)](https://lvgl.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Support this project: back the crowdfunding to get detailed build instructions and 3D-printable enclosure models at
https://makerworld.com/en/crowdfunding/159-project-aura-make-the-invisible-visible

Compact ESP32-S3 air-quality station with LVGL UI, local web setup, and MQTT + Home Assistant discovery.

![Project Aura device](docs/assets/device-hero.jpg)

## Gallery
![Assembly overview](docs/assets/device-assembly.jpg)
![Internal wiring](docs/assets/device-internals.jpg)
![Boot and live dashboard](docs/assets/device-boot-ui.jpg)

## UI Screens
![Dashboard](docs/assets/ui-dashboard.png)
![Settings](docs/assets/ui-settings.png)
![Theme presets](docs/assets/ui-theme-presets.png)

## Highlights
- Multi-sensor telemetry (temp, humidity, CO2, VOC, NOx, PM, pressure, HCHO).
- LVGL UI with night mode, custom themes, and status indicators.
- Wi-Fi AP onboarding + mDNS portal (`aura.local`) for Wi-Fi/MQTT/theme setup.
- MQTT state publishing and Home Assistant discovery + command topics.
- **Robust Safe Boot:** automatic rollback to last-known-good config after crashes, plus boot diagnostics and I2C recovery.

## Hardware
- Waveshare ESP32-S3-Touch-LCD-4.3 (16MB flash, LittleFS).
- RGB LCD + touch via ESP32_Display_Panel (GT911 supported).
- Sensor drivers included: SEN66, SFA3X, DPS310, BMP580, PCF8523 RTC.

## Pin Configuration (Wiring)
| Component | Pin (ESP32-S3) | Notes |
| :--- | :--- | :--- |
| **3V3** | `3V3` | Power for external I2C sensors |
| **GND** | `GND` | Common ground |
| **I2C SDA** | `GPIO 8` | SEN66, SFA3X, BMP580 (external) |
| **I2C SCL** | `GPIO 9` | |

Display and touch are on the board; no external wiring is needed.

## Firmware Architecture
Data flow and responsibilities are intentionally split into small managers:

```mermaid
graph TD
    subgraph Hardware
        Sensors[Sensors<br/>SEN66, SFA3X, BMP580]
        Touch[Touch<br/>GT911]
        RTC[RTC<br/>PCF8523]
        LCD[LCD + Backlight]
    end

    subgraph Core
        SM[SensorManager]
        NM[NetworkManager]
        MM[MqttManager]
        TM[TimeManager]
        CoreLogic[BootPolicy / Watchdog / MemoryMonitor]
    end

    subgraph Data
        History[PressureHistory]
        Storage[(LittleFS<br/>config/last_good/voc/pressure)]
    end

    subgraph UI
        LVGL[LVGL UI]
        Web[Web Config Portal]
    end

    Sensors -->|I2C| SM
    RTC -->|I2C| TM
    Touch -->|I2C| LVGL
    LCD --> LVGL
    SM -->|Data| LVGL
    SM -->|Data| History
    History <-->|save/load| Storage
    Storage <-->|config| NM
    Storage <-->|config| MM
    NM --> Web
    NM --> MM
    MM -->|Publish| HA[Home Assistant]

    style CoreLogic fill:#f96,stroke:#333,stroke-width:2px
```

Core modules live in `src/core/` and orchestrate startup (`AppInit`, `BoardInit`).
Feature managers are in `src/modules/`, UI in `src/ui/`, and web pages in `src/web/`.

## Build and Flash (PlatformIO)
Prereqs: PlatformIO CLI or VSCode + PlatformIO extension.
Built with Arduino ESP32 core 3.1.1 (ESP-IDF 5.3.x).

```powershell
pio run -e project_aura
pio run -e project_aura -t upload
pio device monitor -b 115200
```

## Configuration and Secrets
Runtime settings are stored on-device in LittleFS:
- `/config.json` (active config)
- `/config.last_good.json` (rollback snapshot)

Wi-Fi, MQTT, and themes can be configured from the device UI or from the web portal:
- AP mode SSID: `ProjectAura-Setup`
- Config portal: `http://192.168.4.1`
- When connected: `http://aura.local/` (Wi-Fi), `/mqtt`, `/theme`

Optional compile-time defaults belong in `include/secrets.h`, which is ignored by git.
Copy and edit:

```text
copy include/secrets.h.example include/secrets.h
```

This keeps Wi-Fi passwords and MQTT credentials out of the repository while still allowing
easy local setup.

## MQTT + Home Assistant
- State topic: `<base>/state`
- Availability topic: `<base>/status`
- Commands: `<base>/command/*` (night_mode, alert_blink, backlight, restart)
- Home Assistant discovery: `homeassistant/*/config`

MQTT stays idle until configured and enabled.

![Home Assistant dashboard](docs/assets/ha-dashboard.jpg)

## Tests
See `TESTING.md` for native host tests and `scripts/run_tests.ps1`.

## Docs
- `CUSTOM_IPC_STACK_BUILD.md` covers custom ESP-IDF/Arduino IPC stack size builds.

## Repo Layout
- `src/core/` boot, init, reliability
- `src/modules/` sensors, storage, network, MQTT, time
- `src/ui/` LVGL screens, assets, controllers
- `src/web/` HTML templates and handlers
- `test/` native tests and mocks

## License
MIT. See `LICENSE`.
