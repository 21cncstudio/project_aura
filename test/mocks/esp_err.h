#pragma once

typedef int esp_err_t;

#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_ERR_INVALID_ARG
#define ESP_ERR_INVALID_ARG -2
#endif
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM -3
#endif
#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT -4
#endif

inline const char *esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "ESP_ERR";
}
