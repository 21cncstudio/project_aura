# Home Assistant Dashboard for Project Aura

This folder contains a ready-to-use YAML configuration to visualize your Aura data.
It uses only standard Home Assistant cards. No HACS or external dependencies required.

![Dashboard Preview](../assets/preview.png)

## How to Add
You can add this as a new view (tab) to your existing dashboard.

1. Open your Home Assistant Dashboard.
2. Click the Pencil icon (Edit Dashboard) in the top right.
3. Click the 3 dots menu (top right) -> Raw configuration editor.
4. Scroll to the place where you want to insert the code (or replace existing code if starting fresh).
5. Paste the contents of `dashboard.yaml`.
6. Click Save.

## Entity Configuration
Your entity IDs might differ depending on how your MQTT auto-discovery named them (for example,
`sensor.aura_sfa30_voc` vs `sensor.livingroom_voc`).

If you see "Entity not found" warnings:
1. Go to Settings -> Devices & Services -> Entities.
2. Search for "Aura" to see your actual entity IDs.
3. Open the dashboard code and use Find & Replace to swap the IDs with yours.

## Events Entity

Aura automatically registers a `sensor.<device_name>_events` entity via MQTT Discovery.
This sensor mirrors all significant runtime events to the HA logbook in real time —
matching what the built-in web dashboard **Events** tab already shows locally.

The sensor **state** is the human-readable event message, so it appears directly
in the HA logbook and history without any additional configuration.

### Attributes

| Attribute | Example value |
|---|---|
| `event_type` | `co2_bad`, `pm25_critical`, `system_warning`, `system_info` |
| `message` | `CO2 worsened to bad: 1480 ppm` |
| `category` | `SENSORS`, `TIME`, `RTC`, `STORAGE`, `FAN`, `WIFI`, `MQTT`, `PRESSURE`, `CHARTS`, `UI` |
| `value` | `1480.0` (air quality events only) |
| `unit` | `ppm` (air quality events only) |

### Air quality events

Fire on every band transition (good / moderate / bad / critical) for:
CO2, CO, PM0.5, PM1.0, PM2.5, PM4.0, PM10, HCHO, VOC, NOx.

### System events

#### Boot sequence

| Message | Category |
|---|---|
| `WiFi connected, IP: <ip>` | WIFI |
| `mDNS responder started: <hostname>.local` | WIFI |
| `MQTT connected` | MQTT |
| `NTP sync start (tz=<timezone>)` | TIME |
| `NTP sync completed, local time=<timestamp>` | TIME |
| `BMP580/581 OK` / `BMP3xx OK` / `DPS310 OK` | SENSORS |
| `SFA30 OK` | SENSORS |
| `SEN0466 CO OK at 0x<addr>` | SENSORS |
| `Pressure history restored` | PRESSURE |
| `Charts history restored` | CHARTS |
| `boot screens released` | UI |

#### Sensors

| Message | Category |
|---|---|
| `SEN66 not found (x/y)` | SENSORS |
| `SEN66 start attempts exhausted, stop probing until reboot` | SENSORS |
| `SEN66 OK` | SENSORS |
| `SFA30 init failed` | SENSORS |
| `Pressure sensor not found` | SENSORS |

#### Time

| Message | Category |
|---|---|
| `NTP sync timeout after <N> ms` | TIME |
| `RTC init failed` | RTC |
| `RTC read failed repeatedly` | RTC |
| `RTC communication restored` | RTC |
| `RTC battery low` | RTC |
| `RTC battery status OK` | RTC |

#### Storage

| Message | Category |
|---|---|
| `config committed as last known good` | STORAGE |
| `last known good commit failed` | STORAGE |
| `restored last good config` | STORAGE |
| `last good config missing, factory reset` | STORAGE |
| `factory reset requested` | STORAGE |
| `LittleFS mount failed` | STORAGE |

#### Fan / DAC

| Message | Category |
|---|---|
| `DAC ready` | FAN |
| `DAC recovered` | FAN |
| `DAC error: <reason>` | FAN |

#### Network

| Message | Category |
|---|---|
| `WiFi connect failed` | WIFI |
| `WiFi AP failed to start` | WIFI |
| `mDNS start failed` | WIFI |

#### History

| Message | Category |
|---|---|
| `Pressure history: 3h delta ready` | PRESSURE |
| `Pressure history: 24h delta ready` | PRESSURE |
| `Pressure history: filling gap <N>s` | PRESSURE |
| `Pressure history: stale, reset` | PRESSURE |
| `Pressure history: gap <N>s, reset` | PRESSURE |
| `Pressure history: invalid header, reset` | PRESSURE |
| `Pressure history: invalid index/count, reset` | PRESSURE |
| `Charts history: stale, reset` | CHARTS |
| `Charts history: epoch moved backwards, reset` | CHARTS |
| `Charts history: invalid header, reset` | CHARTS |
| `Charts history: invalid index/count, reset` | CHARTS |

### Example automation

```yaml
trigger:
  - platform: state
    entity_id: sensor.aura_events
condition:
  - condition: template
    value_template: >
      {{ trigger.to_state.attributes.get('event_type') in
         ['co2_bad', 'co2_critical', 'pm25_bad', 'pm25_critical'] }}
action:
  - service: notify.mobile_app
    data:
      message: "Aura: {{ trigger.to_state.state }}"
```

### Example logbook card

```yaml
type: logbook
entities:
  - sensor.aura_<device_name>_events
hours_to_show: 24
```