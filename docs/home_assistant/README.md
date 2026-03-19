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

Additional attributes available for automations:

| Attribute | Example value |
|---|---|
| `event_type` | `co2_bad`, `pm25_critical`, `system_warning`, `system_info` |
| `message` | `CO2 worsened to bad: 1480 ppm` |
| `category` | `SENSORS`, `TIME`, `STORAGE`, `RTC`, `FAN` |
| `value` | `1480.0` (air quality events only) |
| `unit` | `ppm` (air quality events only) |

**Air quality events** fire on every band transition (good / moderate / bad / critical)
for: CO2, CO, PM0.5, PM1.0, PM2.5, PM4.0, PM10, HCHO, VOC, NOx.

**System events** fire for: NTP sync completed/timeout, RTC status changes and battery,
storage lifecycle (last-known-good commit, factory reset, mount failure),
SEN66/SFA30 sensor errors, and DAC/fan control status.

Example automation trigger:

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
