#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_FALSE(cond, code, tag, ...) do { (void)(tag); if (!(cond)) return (code); } while (0)
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { (void)(tag); esp_err_t e_ = (expr); if (e_ != ESP_OK) return e_; } while (0)
#define ESP_GOTO_ON_FALSE(cond, code, label, tag, ...) do { (void)(tag); if (!(cond)) { ret = (code); goto label; } } while (0)
#define ESP_GOTO_ON_ERROR(expr, label, tag, ...) do { (void)(tag); ret = (expr); if (ret != ESP_OK) goto label; } while (0)
