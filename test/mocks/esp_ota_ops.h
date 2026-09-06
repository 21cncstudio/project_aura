#pragma once

#include <stdint.h>

typedef struct {
    uint32_t size;
} esp_partition_t;

const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *start_from);
