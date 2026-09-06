#pragma once

#include "esp_err.h"

#define ESP_RETURN_ON_FALSE(condition, error, tag, ...) \
    do { if (!(condition)) { return (error); } } while (0)
#define ESP_RETURN_ON_ERROR(expression, tag, ...) \
    do { const esp_err_t check_error = (expression); \
         if (check_error != ESP_OK) { return check_error; } } while (0)
#define ESP_GOTO_ON_ERROR(expression, label, tag, ...) \
    do { ret = (expression); if (ret != ESP_OK) { goto label; } } while (0)
