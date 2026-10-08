#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef int gpio_num_t;
typedef void (*gpio_isr_t)(void *);
#define GPIO_NUM_NC -1
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_OUTPUT_OD 2
#define GPIO_MODE_INPUT 3
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_POSEDGE 1
#define GPIO_INTR_NEGEDGE 2
typedef struct { int mode; uint64_t pin_bit_mask; int intr_type, pull_down_en, pull_up_en; } gpio_config_t;
static inline esp_err_t gpio_config(const gpio_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t gpio_set_level(int p, int l) { (void)p; (void)l; return ESP_OK; }
static inline esp_err_t gpio_reset_pin(int p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_install_isr_service(int f) { (void)f; return ESP_OK; }
static inline esp_err_t gpio_intr_enable(int p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_intr_disable(int p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_isr_handler_remove(int p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_isr_handler_add(int p, gpio_isr_t fn, void *a) { (void)p; (void)fn; (void)a; return ESP_OK; }
