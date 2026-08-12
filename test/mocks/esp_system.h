#pragma once

typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON = 1,
    ESP_RST_SW = 3,
    ESP_RST_BROWNOUT = 9,
} esp_reset_reason_t;
