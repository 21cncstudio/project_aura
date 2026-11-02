#pragma once
#include "esp_err.h"
typedef void esp_netif_t;
esp_err_t esp_netif_set_hostname(esp_netif_t *, const char *);
esp_err_t esp_netif_get_hostname(esp_netif_t *, const char **);
