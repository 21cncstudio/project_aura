// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

struct ConnectivityStatusSnapshot {
    uint32_t sampled_ms = 0;
    int wifi_state = 0;
    int wifi_sta_status = 0;
    int rssi = 0;
    uint8_t wifi_retry_count = 0;
    uint8_t mqtt_retry_stage = 0;
    bool valid = false;
    bool wifi_enabled = false;
    bool wifi_connected = false;
    bool wifi_ap_mode = false;
    bool wifi_scan_in_progress = false;
    bool has_rssi = false;
    bool mqtt_enabled = false;
    bool mqtt_connected = false;
    bool mqtt_tls_waiting_for_time = false;
};
