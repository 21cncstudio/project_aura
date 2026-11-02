#pragma once
#include "esp_err.h"
typedef enum { WIFI_PS_NONE = 0, WIFI_PS_MIN_MODEM = 1, WIFI_PS_MAX_MODEM = 2 } wifi_ps_type_t;
esp_err_t esp_wifi_get_ps(wifi_ps_type_t *t);
esp_err_t esp_wifi_set_ps(wifi_ps_type_t t);

#define ESP_ERR_WIFI_NOT_INIT 0x3001
#define ESP_ERR_WIFI_NOT_STARTED 0x3002
esp_err_t esp_wifi_scan_stop();
esp_err_t esp_wifi_clear_ap_list();

#include <stdint.h>
enum { WIFI_SCAN_TYPE_ACTIVE = 0 };
struct wifi_scan_config_t { uint8_t channel = 0;
    int scan_type = 0;
    struct { struct { uint32_t min = 0; uint32_t max = 0; } active; } scan_time;
};
struct wifi_ap_record_t { uint8_t ssid[33]{}; int8_t rssi = 0; int authmode = 0; };
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool block);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count);
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *record);

// Single-channel default keeps ownership tests independent of scan duration.
struct wifi_country_t { uint8_t schan = 1; uint8_t nchan = 1; };
esp_err_t esp_wifi_get_country(wifi_country_t *country);
