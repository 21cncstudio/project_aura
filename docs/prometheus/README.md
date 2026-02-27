# Prometheus Metrics Exporter

Project Aura exposes a Prometheus-compatible metrics endpoint at `GET /metrics`.
This allows scraping by Prometheus, VictoriaMetrics, Grafana Alloy, or any
OpenMetrics-compatible collector.

## Quick Start

Once your device is connected to Wi-Fi, metrics are available at:

```
http://aura.local/metrics
```

No configuration is needed on the device side. The endpoint is always active
when the web server is running.

## Prometheus Scrape Configuration

Add a scrape job to your `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: "project_aura"
    scrape_interval: 30s
    static_configs:
      - targets: ["aura.local:80"]
        labels:
          location: "living_room"
```

If you have multiple Aura devices with custom hostnames, list them as separate
targets:

```yaml
scrape_configs:
  - job_name: "project_aura"
    scrape_interval: 30s
    static_configs:
      - targets:
          - "aura-living.local:80"
          - "aura-bedroom.local:80"
          - "aura-office.local:80"
```

You can also use IP addresses instead of mDNS hostnames:

```yaml
      - targets: ["192.168.1.42:80"]
```

### Recommended Scrape Interval

30 seconds is a good default. The endpoint responds in under 10 ms and uses
chunked transfer to keep memory usage low (~1.5 KB peak). Intervals as low as
15 seconds work fine without impacting device performance.

## Available Metrics

All metrics use the `aura_` prefix and follow
[Prometheus naming conventions](https://prometheus.io/docs/practices/naming/).

### Environmental Sensors

| Metric | Unit | Description |
|:---|:---|:---|
| `aura_temperature_celsius` | C | Ambient temperature |
| `aura_humidity_percent` | % | Relative humidity |
| `aura_pressure_hpa` | hPa | Atmospheric pressure |
| `aura_pm05_count_per_cm3` | #/cm3 | PM0.5 particle count |
| `aura_pm1_ugm3` | ug/m3 | PM1.0 mass concentration |
| `aura_pm25_ugm3` | ug/m3 | PM2.5 mass concentration |
| `aura_pm4_ugm3` | ug/m3 | PM4.0 mass concentration |
| `aura_pm10_ugm3` | ug/m3 | PM10 mass concentration |
| `aura_co2_ppm` | ppm | CO2 concentration |
| `aura_voc_index` | index | VOC index (Sensirion, 1-500) |
| `aura_nox_index` | index | NOx index (Sensirion, 1-500) |
| `aura_hcho_ppb` | ppb | Formaldehyde concentration |
| `aura_co_ppm` | ppm | Carbon monoxide (if sensor installed) |
| `aura_co_sensor_present` | bool | CO sensor module installed (0/1) |
| `aura_co_sensor_warmup` | bool | CO sensor in warmup phase (0/1) |

Metrics for sensors that are not connected or not yet valid are **omitted**
(not exported as zero). Prometheus treats absent metrics as stale.

### Derived Metrics

| Metric | Unit | Description |
|:---|:---|:---|
| `aura_dew_point_celsius` | C | Dew point (from temp + humidity) |
| `aura_absolute_humidity_gm3` | g/m3 | Absolute humidity |
| `aura_mold_risk_index` | index | Mold risk (0-10) |
| `aura_pressure_delta_3h_hpa` | hPa | Pressure change over 3 hours |
| `aura_pressure_delta_24h_hpa` | hPa | Pressure change over 24 hours |

### DAC / Fan Control

| Metric | Unit | Description |
|:---|:---|:---|
| `aura_dac_available` | bool | DAC hardware detected (0/1) |
| `aura_dac_running` | bool | Fan output active (0/1) |
| `aura_dac_faulted` | bool | Hardware fault detected (0/1) |
| `aura_dac_output_known` | bool | Output level valid (0/1) |
| `aura_dac_manual_override` | bool | Manual override active (0/1) |
| `aura_dac_auto_resume_blocked` | bool | Auto resume blocked (0/1) |
| `aura_dac_mode` | enum | 0 = manual, 1 = auto |
| `aura_dac_manual_step` | step | Manual speed step (1-100) |
| `aura_dac_timer_selected_seconds` | s | Timer duration |
| `aura_dac_timer_remaining_seconds` | s | Timer remaining |
| `aura_dac_output_millivolts` | mV | DAC output (0-10000) |
| `aura_dac_output_percent` | % | DAC output (0-100) |

### Network

| Metric | Unit | Description |
|:---|:---|:---|
| `aura_wifi_enabled` | bool | WiFi enabled (0/1) |
| `aura_wifi_rssi_dbm` | dBm | WiFi signal strength |
| `aura_mqtt_enabled` | bool | MQTT enabled (0/1) |
| `aura_mqtt_connected` | bool | MQTT connected (0/1) |

### System

| Metric | Unit | Description |
|:---|:---|:---|
| `aura_uptime_seconds` | s | Device uptime |
| `aura_heap_free_bytes` | bytes | Free heap memory |
| `aura_heap_min_free_bytes` | bytes | Minimum free heap since boot |
| `aura_heap_max_alloc_bytes` | bytes | Largest contiguous free block |
| `aura_heap_8bit_free_bytes` | bytes | Free 8-bit capable heap |
| `aura_heap_8bit_min_free_bytes` | bytes | Minimum free 8-bit heap |
| `aura_heap_8bit_largest_block_bytes` | bytes | Largest 8-bit free block |
| `aura_heap_internal_free_bytes` | bytes | Free internal SRAM |
| `aura_heap_internal_min_free_bytes` | bytes | Minimum free internal SRAM |
| `aura_heap_internal_largest_block_bytes` | bytes | Largest internal free block |
| `aura_psram_free_bytes` | bytes | Free PSRAM |
| `aura_psram_min_free_bytes` | bytes | Minimum free PSRAM |
| `aura_psram_max_alloc_bytes` | bytes | Largest PSRAM free block |
| `aura_cpu_frequency_mhz` | MHz | CPU clock frequency |
| `aura_chip_temperature_celsius` | C | Internal chip temperature |
| `aura_flash_size_bytes` | bytes | Total flash size |
| `aura_sketch_size_bytes` | bytes | Firmware image size |
| `aura_sketch_free_bytes` | bytes | Free space for OTA update |
| `aura_boot_count` | count | Boot counter |
| `aura_reset_reason` | enum | ESP reset reason code |
| `aura_freertos_task_count` | count | Number of FreeRTOS tasks |

### Build Info

```
aura_build_info{firmware="1.1.0",build_date="...",build_time="...",hostname="aura",device_name="Project Aura"} 1
```

## Example PromQL Queries

```promql
# Current CO2 level
aura_co2_ppm

# Average temperature over the last hour
avg_over_time(aura_temperature_celsius[1h])

# Free heap memory trend
aura_heap_free_bytes

# PM2.5 above WHO guideline (15 ug/m3)
aura_pm25_ugm3 > 15

# WiFi signal quality
aura_wifi_rssi_dbm

# Alert on low memory (below 20 KB)
aura_heap_internal_free_bytes < 20000
```

## Grafana

Add your Prometheus instance as a data source in Grafana. All `aura_*` metrics
will be available in the metric browser. A recommended dashboard layout:

- **Row 1:** Temperature, Humidity, Pressure, Dew Point
- **Row 2:** CO2, VOC, NOx, HCHO
- **Row 3:** PM0.5, PM1.0, PM2.5, PM4.0, PM10
- **Row 4:** Heap free, PSRAM free, Uptime, WiFi RSSI

## Validating the Output

You can validate the endpoint output with the Prometheus `promtool`:

```bash
curl -s http://aura.local/metrics | promtool check metrics
```

## Notes

- The endpoint returns HTTP 503 during OTA firmware uploads.
- Flash and sketch size metrics are cached at boot (they never change at runtime).
- The internal chip temperature reflects silicon temperature, not ambient. It
  typically reads 4-6 C higher than ambient due to self-heating.
- RSSI is only exported when connected in station mode with a valid signal.
