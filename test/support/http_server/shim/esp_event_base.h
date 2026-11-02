#pragma once
typedef const char *esp_event_base_t;
#define ESP_EVENT_DECLARE_BASE(id) extern esp_event_base_t const id
#define ESP_EVENT_DEFINE_BASE(id) esp_event_base_t const id = #id
