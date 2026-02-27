// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/PrometheusExporter.h"

#include <math.h>
#include <stdio.h>

#ifndef UNIT_TEST
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include "config/AppData.h"
#include "core/MathUtils.h"

#ifndef UNIT_TEST
#include "modules/FanControl.h"
#include "web/WebHandlers.h"
#include "core/BootState.h"
#endif

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

namespace {

WebHandlerContext *g_prom_ctx = nullptr;

#ifndef UNIT_TEST
uint32_t g_cached_flash_size = 0;
uint32_t g_cached_sketch_size = 0;
uint32_t g_cached_sketch_free = 0;
#endif

// ---------------------------------------------------------------------------
// Prometheus text format helpers
// ---------------------------------------------------------------------------

void prom_gauge_header(String &out, const char *name, const char *help) {
    out += "# HELP ";
    out += name;
    out += ' ';
    out += help;
    out += '\n';
    out += "# TYPE ";
    out += name;
    out += " gauge\n";
}

void prom_gauge_float(String &out, const char *name, const char *help,
                      float value, int decimals) {
    prom_gauge_header(out, name, help);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, static_cast<double>(value));
    out += name;
    out += ' ';
    out += buf;
    out += '\n';
}

void prom_gauge_int(String &out, const char *name, const char *help,
                    int32_t value) {
    prom_gauge_header(out, name, help);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    out += name;
    out += ' ';
    out += buf;
    out += '\n';
}

void prom_gauge_uint(String &out, const char *name, const char *help,
                     uint32_t value) {
    prom_gauge_header(out, name, help);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    out += name;
    out += ' ';
    out += buf;
    out += '\n';
}

void prom_gauge_float_if(String &out, const char *name, const char *help,
                         bool valid, float value, int decimals) {
    if (!valid || !isfinite(value)) return;
    prom_gauge_float(out, name, help, value, decimals);
}

void prom_gauge_int_if(String &out, const char *name, const char *help,
                       bool valid, int32_t value) {
    if (!valid) return;
    prom_gauge_int(out, name, help, value);
}

void prom_gauge_bool(String &out, const char *name, const char *help,
                     bool value) {
    prom_gauge_header(out, name, help);
    out += name;
    out += value ? " 1\n" : " 0\n";
}

void prom_escape_label(String &out, const String &value) {
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c == '\\')      out += "\\\\";
        else if (c == '"')  out += "\\\"";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Section builders (outside anonymous namespace for testability)
// ---------------------------------------------------------------------------

void prom_append_sensor_metrics(String &out, const SensorData &data) {
    prom_gauge_float_if(out, "aura_temperature_celsius",
        "Ambient temperature in degrees Celsius",
        data.temp_valid, data.temperature, 1);
    prom_gauge_float_if(out, "aura_humidity_percent",
        "Relative humidity in percent",
        data.hum_valid, data.humidity, 1);
    prom_gauge_float_if(out, "aura_pressure_hpa",
        "Atmospheric pressure in hectopascals",
        data.pressure_valid, data.pressure, 1);
    prom_gauge_float_if(out, "aura_pm05_count_per_cm3",
        "PM0.5 particle count per cubic centimeter",
        data.pm05_valid, data.pm05, 1);
    prom_gauge_float_if(out, "aura_pm1_ugm3",
        "PM1.0 mass concentration in micrograms per cubic meter",
        data.pm1_valid, data.pm1, 1);
    prom_gauge_float_if(out, "aura_pm25_ugm3",
        "PM2.5 mass concentration in micrograms per cubic meter",
        data.pm25_valid, data.pm25, 1);
    prom_gauge_float_if(out, "aura_pm4_ugm3",
        "PM4.0 mass concentration in micrograms per cubic meter",
        data.pm4_valid, data.pm4, 1);
    prom_gauge_float_if(out, "aura_pm10_ugm3",
        "PM10 mass concentration in micrograms per cubic meter",
        data.pm10_valid, data.pm10, 1);
    prom_gauge_int_if(out, "aura_co2_ppm",
        "CO2 concentration in parts per million",
        data.co2_valid, data.co2);
    prom_gauge_int_if(out, "aura_voc_index",
        "VOC index (1-500 Sensirion algorithm)",
        data.voc_valid, data.voc_index);
    prom_gauge_int_if(out, "aura_nox_index",
        "NOx index (1-500 Sensirion algorithm)",
        data.nox_valid, data.nox_index);
    prom_gauge_float_if(out, "aura_hcho_ppb",
        "Formaldehyde concentration in parts per billion",
        data.hcho_valid, data.hcho, 1);
    prom_gauge_float_if(out, "aura_co_ppm",
        "Carbon monoxide concentration in parts per million",
        data.co_valid && data.co_sensor_present, data.co_ppm, 1);
    prom_gauge_bool(out, "aura_co_sensor_present",
        "Whether the CO sensor module is physically installed",
        data.co_sensor_present);
    prom_gauge_bool(out, "aura_co_sensor_warmup",
        "Whether the CO sensor is in warmup phase",
        data.co_warmup);
}

void prom_append_derived_metrics(String &out, const SensorData &data) {
    const bool climate_valid = data.temp_valid && data.hum_valid;
    if (climate_valid) {
        const float dew_point = MathUtils::compute_dew_point_c(
            data.temperature, data.humidity);
        prom_gauge_float_if(out, "aura_dew_point_celsius",
            "Dew point temperature in degrees Celsius",
            true, dew_point, 1);

        const float abs_humidity = MathUtils::compute_absolute_humidity_gm3(
            data.temperature, data.humidity);
        prom_gauge_float_if(out, "aura_absolute_humidity_gm3",
            "Absolute humidity in grams per cubic meter",
            true, abs_humidity, 1);

        const int mold_risk = MathUtils::compute_mold_risk_index(
            data.temperature, data.humidity);
        if (mold_risk >= 0) {
            prom_gauge_int(out, "aura_mold_risk_index",
                "Mold risk index (0-10)", mold_risk);
        }
    }
}

void prom_append_pressure_trend_metrics(String &out, const SensorData &data) {
    prom_gauge_float_if(out, "aura_pressure_delta_3h_hpa",
        "Pressure change over last 3 hours in hectopascals",
        data.pressure_delta_3h_valid, data.pressure_delta_3h, 1);
    prom_gauge_float_if(out, "aura_pressure_delta_24h_hpa",
        "Pressure change over last 24 hours in hectopascals",
        data.pressure_delta_24h_valid, data.pressure_delta_24h, 1);
}

#ifndef UNIT_TEST

namespace {

void append_dac_metrics(String &out, const WebHandlerContext &ctx) {
    if (!ctx.fan_control) return;
    const FanControl &fan = *ctx.fan_control;
    const uint32_t now_ms = millis();

    prom_gauge_bool(out, "aura_dac_available",
        "Whether the DAC/fan hardware is detected", fan.isAvailable());
    prom_gauge_bool(out, "aura_dac_running",
        "Whether the fan output is currently active", fan.isRunning());
    prom_gauge_bool(out, "aura_dac_faulted",
        "Whether a DAC hardware fault has been detected", fan.isFaulted());
    prom_gauge_bool(out, "aura_dac_output_known",
        "Whether the current output level is valid", fan.isOutputKnown());
    prom_gauge_bool(out, "aura_dac_manual_override",
        "Whether manual override is blocking auto mode",
        fan.isManualOverrideActive());
    prom_gauge_bool(out, "aura_dac_auto_resume_blocked",
        "Whether auto resume from manual is blocked",
        fan.isAutoResumeBlocked());
    prom_gauge_int(out, "aura_dac_mode",
        "Fan control mode (0=manual, 1=auto)",
        static_cast<int>(fan.mode()));
    prom_gauge_uint(out, "aura_dac_manual_step",
        "Manual fan speed step (1-100)", fan.manualStep());
    prom_gauge_uint(out, "aura_dac_timer_selected_seconds",
        "Selected timer duration in seconds", fan.selectedTimerSeconds());
    prom_gauge_uint(out, "aura_dac_timer_remaining_seconds",
        "Timer remaining seconds until auto-stop",
        fan.remainingSeconds(now_ms));
    prom_gauge_uint(out, "aura_dac_output_millivolts",
        "Current DAC output in millivolts", fan.outputMillivolts());
    prom_gauge_uint(out, "aura_dac_output_percent",
        "Current DAC output as percentage", fan.outputPercent());
}

void append_network_metrics(String &out, const WebHandlerContext &ctx) {
    const bool wifi_enabled = ctx.wifi_enabled ? *ctx.wifi_enabled : false;
    prom_gauge_bool(out, "aura_wifi_enabled",
        "Whether WiFi is enabled", wifi_enabled);

    const bool sta_connected = ctx.wifi_is_connected && ctx.wifi_is_connected();
    if (sta_connected) {
        const int rssi = WiFi.RSSI();
        if (rssi < 0) {
            prom_gauge_int(out, "aura_wifi_rssi_dbm",
                "WiFi signal strength in dBm", rssi);
        }
    }

    const bool mqtt_enabled = ctx.mqtt_user_enabled ? *ctx.mqtt_user_enabled : false;
    prom_gauge_bool(out, "aura_mqtt_enabled",
        "Whether MQTT is enabled by user", mqtt_enabled);
    prom_gauge_bool(out, "aura_mqtt_connected",
        "Whether MQTT client is currently connected",
        ctx.mqtt_client && ctx.mqtt_client->connected());
}

void append_system_metrics(String &out) {
    // Uptime
    prom_gauge_uint(out, "aura_uptime_seconds",
        "Device uptime in seconds",
        static_cast<uint32_t>(millis() / 1000UL));

    // Heap: general
    prom_gauge_uint(out, "aura_heap_free_bytes",
        "Free heap memory in bytes", ESP.getFreeHeap());
    prom_gauge_uint(out, "aura_heap_min_free_bytes",
        "Minimum free heap since boot in bytes", ESP.getMinFreeHeap());
    prom_gauge_uint(out, "aura_heap_max_alloc_bytes",
        "Largest contiguous free heap block in bytes", ESP.getMaxAllocHeap());

    // Heap: 8-bit capable
    prom_gauge_uint(out, "aura_heap_8bit_free_bytes",
        "Free 8-bit capable heap in bytes",
        heap_caps_get_free_size(MALLOC_CAP_8BIT));
    prom_gauge_uint(out, "aura_heap_8bit_min_free_bytes",
        "Minimum free 8-bit capable heap since boot in bytes",
        heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    prom_gauge_uint(out, "aura_heap_8bit_largest_block_bytes",
        "Largest free 8-bit capable block in bytes",
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Heap: internal SRAM
    prom_gauge_uint(out, "aura_heap_internal_free_bytes",
        "Free internal SRAM in bytes",
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    prom_gauge_uint(out, "aura_heap_internal_min_free_bytes",
        "Minimum free internal SRAM since boot in bytes",
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    prom_gauge_uint(out, "aura_heap_internal_largest_block_bytes",
        "Largest free internal SRAM block in bytes",
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    // Heap: PSRAM
    prom_gauge_uint(out, "aura_psram_free_bytes",
        "Free PSRAM in bytes", ESP.getFreePsram());
    prom_gauge_uint(out, "aura_psram_min_free_bytes",
        "Minimum free PSRAM since boot in bytes", ESP.getMinFreePsram());
    prom_gauge_uint(out, "aura_psram_max_alloc_bytes",
        "Largest contiguous free PSRAM block in bytes", ESP.getMaxAllocPsram());

    // CPU
    prom_gauge_uint(out, "aura_cpu_frequency_mhz",
        "CPU clock frequency in MHz", getCpuFrequencyMhz());

    // Chip temperature (ESP32-S3 internal sensor)
    float chip_temp = temperatureRead();
    if (isfinite(chip_temp)) {
        prom_gauge_float(out, "aura_chip_temperature_celsius",
            "Internal chip temperature in degrees Celsius", chip_temp, 1);
    }

    // Flash / sketch (cached at init)
    prom_gauge_uint(out, "aura_flash_size_bytes",
        "Total flash chip size in bytes", g_cached_flash_size);
    prom_gauge_uint(out, "aura_sketch_size_bytes",
        "Firmware sketch size in bytes", g_cached_sketch_size);
    prom_gauge_uint(out, "aura_sketch_free_bytes",
        "Free sketch space for OTA in bytes", g_cached_sketch_free);

    // Boot state
    prom_gauge_uint(out, "aura_boot_count",
        "Number of boots since counter reset", boot_count);
    prom_gauge_int(out, "aura_reset_reason",
        "ESP reset reason code from last boot",
        static_cast<int32_t>(boot_reset_reason));

    // FreeRTOS task count (lightweight, no interrupt disable)
    prom_gauge_uint(out, "aura_freertos_task_count",
        "Number of FreeRTOS tasks", uxTaskGetNumberOfTasks());
}

void append_info_metric(String &out, const WebHandlerContext &ctx) {
    out += "# HELP aura_build_info Firmware build information\n";
    out += "# TYPE aura_build_info gauge\n";
    out += "aura_build_info{firmware=\"";
    out += APP_VERSION;
    out += "\",build_date=\"";
    out += __DATE__;
    out += "\",build_time=\"";
    out += __TIME__;
    out += "\",hostname=\"";
    if (ctx.hostname) {
        prom_escape_label(out, *ctx.hostname);
    } else {
        out += "aura";
    }
    out += "\",device_name=\"";
    if (ctx.mqtt_device_name) {
        prom_escape_label(out, *ctx.mqtt_device_name);
    } else {
        out += "Project Aura";
    }
    out += "\"} 1\n";
}

} // namespace

#endif // UNIT_TEST

void PrometheusExporterInit(WebHandlerContext *context) {
    g_prom_ctx = context;
#ifndef UNIT_TEST
    g_cached_flash_size = ESP.getFlashChipSize();
    g_cached_sketch_size = ESP.getSketchSize();
    g_cached_sketch_free = ESP.getFreeSketchSpace();
#endif
}

#ifndef UNIT_TEST

void prometheus_handle_metrics() {
    WebHandlerContext *ctx = g_prom_ctx;
    if (!ctx || !ctx->server || !ctx->sensor_data) {
        return;
    }
    if (WebHandlersIsOtaBusy()) {
        ctx->server->send(503, "text/plain",
                          "# OTA upload in progress\n");
        return;
    }

    const SensorData &data = *ctx->sensor_data;
    WebServer &server = *ctx->server;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/plain; version=0.0.4; charset=utf-8", "");

    String buf;
    buf.reserve(1536);

    // Sensor readings
    prom_append_sensor_metrics(buf, data);
    server.sendContent(buf);
    buf = "";

    // Derived metrics
    prom_append_derived_metrics(buf, data);
    prom_append_pressure_trend_metrics(buf, data);
    server.sendContent(buf);
    buf = "";

    // DAC / fan control
    append_dac_metrics(buf, *ctx);
    server.sendContent(buf);
    buf = "";

    // Network
    append_network_metrics(buf, *ctx);
    server.sendContent(buf);
    buf = "";

    // System metrics (heap, CPU, flash, boot, tasks)
    append_system_metrics(buf);
    server.sendContent(buf);
    buf = "";

    // Build info label metric
    append_info_metric(buf, *ctx);
    server.sendContent(buf);

    // End chunked transfer
    server.sendContent("");
}

#endif // UNIT_TEST
