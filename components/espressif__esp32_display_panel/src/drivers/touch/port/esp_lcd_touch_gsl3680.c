/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../esp_panel_touch_conf_internal.h"
#if ESP_PANEL_DRIVERS_TOUCH_ENABLE_GSL3680

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gsl3680.h"
#include "utils/esp_panel_utils_log.h"
#include "esp_lcd_touch_gsl3680_fw.h"

static const char *TAG = "GSL3680";

/* GSL3680 registers */
#define ESP_LCD_TOUCH_GSL3680_READ_XY_REG     (0x80)

//#define ESP_LCD_TOUCH_GSL3680_READ_KEY_REG    (0x8093)
#define ESP_LCD_TOUCH_GSL3680_CONFIG_REG      (0xF0)
//#define ESP_LCD_TOUCH_GSL3680_PRODUCT_ID_REG  (0x8140)
#define ESP_LCD_TOUCH_GSL3680_ENTER_SLEEP    (0x05)

/* GSL3680 support key num */
#define ESP_GSL3680_TOUCH_MAX_BUTTONS         (9)
//#define ESP_GSL3680_TOUCH_MAX_BUTTONS    CONFIG_ESP_LCD_TOUCH_MAX_POINTS

unsigned int gsl_config_data_id[] = {
    0xccb69a,
    0x200,
    0, 0,
    0,
    0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0x1cc86fd6,


    0x40000d00, 0xa, 0xe001a, 0xe001a, 0x3200500, 0, 0x5100, 0x8e00,
    0, 0x320014, 0, 0x14, 0, 0, 0, 0,
    0x8, 0x4000, 0x1000, 0x10170002, 0x10110000, 0, 0, 0x4040404,
    0x1b6db688, 0x64, 0xb3000f, 0xad0019, 0xa60023, 0xa0002d, 0xb3000f, 0xad0019,
    0xa60023, 0xa0002d, 0xb3000f, 0xad0019, 0xa60023, 0xa0002d, 0xb3000f, 0xad0019,
    0xa60023, 0xa0002d, 0x804000, 0x90040, 0x90001, 0, 0, 0,
    0, 0, 0, 0x14012c, 0xa003c, 0xa0078, 0x400, 0x1081,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0,//key_map
    0x3200384, 0x64, 0x503e8, //0
    0, 0, 0, //1
    0, 0, 0, //2
    0, 0, 0, //3
    0, 0, 0, //4
    0, 0, 0, //5
    0, 0, 0, //6
    0, 0, 0, //7

    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,


    0x220,
    0, 0, 0, 0, 0, 0, 0, 0,
    0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f, 0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
    0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f, 0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f,
    0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f, 0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
    0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f, 0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f,

    0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f, 0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
    0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f, 0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f, 0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
    0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f, 0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,


    0x3,
    0x101, 0, 0x100, 0,
    0x20, 0x10, 0x8, 0x4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0x4, 0, 0, 0, 0, 0, 0, 0,
    0x3800680, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
};

/*******************************************************************************
* Function definitions
*******************************************************************************/
static esp_err_t esp_lcd_touch_gsl3680_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_gsl3680_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
static esp_err_t esp_lcd_touch_gsl3680_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state);
#endif
static esp_err_t esp_lcd_touch_gsl3680_del(esp_lcd_touch_handle_t tp);

/* I2C read/write */
static esp_err_t touch_gsl3680_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t touch_gsl3680_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);

/* GSL3680 reset */
static esp_err_t touch_gsl3680_reset(esp_lcd_touch_handle_t tp);
/* Read status and config register */
static esp_err_t touch_gsl3680_read_cfg(esp_lcd_touch_handle_t tp);

/* GSL3680 enter/exit sleep mode */
static esp_err_t esp_lcd_touch_gsl3680_enter_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_exit_sleep(esp_lcd_touch_handle_t tp);

static esp_err_t esp_lcd_touch_gsl3680_startup_chip(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_load_fw(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_clear_reg(esp_lcd_touch_handle_t tp);
static esp_err_t esp_lcd_touch_gsl3680_init(esp_lcd_touch_handle_t tp);

/*******************************************************************************
* Public API functions
*******************************************************************************/

esp_err_t esp_lcd_touch_new_i2c_gsl3680(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch)
{
    ESP_LOGI(TAG, "GSL3680 driver version: %d.%d.%d", ESP_LCD_TOUCH_GSL3680_VER_MAJOR, ESP_LCD_TOUCH_GSL3680_VER_MINOR,
             ESP_LCD_TOUCH_GSL3680_VER_PATCH);
    esp_err_t ret = ESP_OK;

    assert(io != NULL);
    assert(config != NULL);
    assert(out_touch != NULL);

    /* Prepare main structure */
    // Use `calloc` instead of `heap_caps_calloc` for MicroPython compatibility
    esp_lcd_touch_handle_t esp_lcd_touch_gsl3680 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(esp_lcd_touch_gsl3680, ESP_ERR_NO_MEM, err, TAG, "no mem for GSL3680 controller");

    /* Communication interface */
    esp_lcd_touch_gsl3680->io = io;

    /* Only supported callbacks are set */
    esp_lcd_touch_gsl3680->read_data = esp_lcd_touch_gsl3680_read_data;
    esp_lcd_touch_gsl3680->get_xy = esp_lcd_touch_gsl3680_get_xy;
#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
    esp_lcd_touch_gsl3680->get_button_state = esp_lcd_touch_gsl3680_get_button_state;
#endif
    esp_lcd_touch_gsl3680->del = esp_lcd_touch_gsl3680_del;
    esp_lcd_touch_gsl3680->enter_sleep = esp_lcd_touch_gsl3680_enter_sleep;
    esp_lcd_touch_gsl3680->exit_sleep = esp_lcd_touch_gsl3680_exit_sleep;

    /* Mutex */
    esp_lcd_touch_gsl3680->data.lock.owner = portMUX_FREE_VAL;

    /* Save config */
    memcpy(&esp_lcd_touch_gsl3680->config, config, sizeof(esp_lcd_touch_config_t));
    esp_lcd_touch_io_gsl3680_config_t *gsl3680_config = (esp_lcd_touch_io_gsl3680_config_t *)esp_lcd_touch_gsl3680->config.driver_data;

    /* Prepare pin for touch controller reset */
    if (esp_lcd_touch_gsl3680->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.rst_gpio_num)
        };
        ret = gpio_config(&rst_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");
    }

    if (gsl3680_config && esp_lcd_touch_gsl3680->config.rst_gpio_num != GPIO_NUM_NC && esp_lcd_touch_gsl3680->config.int_gpio_num != GPIO_NUM_NC) {
        /* Prepare pin for touch controller int */
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
            .pull_down_en = 0,
            .pull_up_en = 1,
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.int_gpio_num),
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.rst_gpio_num, esp_lcd_touch_gsl3680->config.levels.reset), TAG, "GPIO set level error!");
        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.int_gpio_num, 0), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Select I2C addr, set output high or low */
        uint32_t gpio_level = 0;

        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.int_gpio_num, gpio_level), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(1));

        ESP_RETURN_ON_ERROR(gpio_set_level(esp_lcd_touch_gsl3680->config.rst_gpio_num, !esp_lcd_touch_gsl3680->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(10));

        vTaskDelay(pdMS_TO_TICKS(50));
    } else {
        ESP_LOGW(TAG, "Unable to initialize the I2C address");
        /* Reset controller */
        ret = touch_gsl3680_reset(esp_lcd_touch_gsl3680);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GSL3680 reset failed");
    }

    /* Read status and config info */
    ESP_LOGI(TAG, "init GSL3680");
    touch_gsl3680_read_cfg(esp_lcd_touch_gsl3680);
    esp_lcd_touch_gsl3680_init(esp_lcd_touch_gsl3680);
    ret = esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_gsl3680);

    /* Prepare pin for touch interrupt */
    if (esp_lcd_touch_gsl3680->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (esp_lcd_touch_gsl3680->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(esp_lcd_touch_gsl3680->config.int_gpio_num)
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        /* Register interrupt callback */
        if (esp_lcd_touch_gsl3680->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_gsl3680, esp_lcd_touch_gsl3680->config.interrupt_callback);
        }
    }

    /* Read status and config info */
    ret = touch_gsl3680_read_cfg(esp_lcd_touch_gsl3680);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "GSL3680 read config failed");

    *out_touch = esp_lcd_touch_gsl3680;

err:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (0x%x)! Touch controller GSL3680 initialization failed!", ret);
        if (esp_lcd_touch_gsl3680) {
            esp_lcd_touch_gsl3680_del(esp_lcd_touch_gsl3680);
        }
    }

    return ret;
}

static esp_err_t esp_lcd_touch_gsl3680_enter_sleep(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 0), TAG, "Enter Sleep failed!");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3680_exit_sleep(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, 1), TAG, "High GPIO config failed!");
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

//TODO: Need implement this function
#define RAW_MAX_X 1650
#define RAW_MAX_Y 890

static esp_err_t esp_lcd_touch_gsl3680_read_data(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    esp_err_t err;
    uint8_t touch_data[24];
    uint8_t touch_cnt = 0;

    err = touch_gsl3680_i2c_read(tp, ESP_LCD_TOUCH_GSL3680_READ_XY_REG, touch_data, sizeof(touch_data));
    ESP_RETURN_ON_ERROR(err, TAG, "I2C read error!");

    touch_cnt = touch_data[0] & 0x0F;
    if (touch_cnt == 0 || touch_cnt > CONFIG_ESP_LCD_TOUCH_MAX_POINTS) {
        return ESP_OK;
    }
    /*
        uint16_t x_point = ((touch_data[7]&0x0f)<<8 )|touch_data[6];
        uint16_t y_point = (touch_data[5]<<8)|touch_data[4];
        uint16_t x2_point = ((touch_data[11]&0x0f)<<8 )|touch_data[10];
        uint16_t y2_point = (touch_data[9]<<8)|touch_data[8];

        portENTER_CRITICAL(&tp->data.lock);
        tp->data.points = touch_cnt;

        tp->data.coords[0].x = x_point;
        tp->data.coords[0].y = y_point;
        tp->data.coords[1].strength = 0;

        tp->data.coords[1].x = x2_point;
        tp->data.coords[1].y = y2_point;
        tp->data.coords[1].strength = 0;

        portEXIT_CRITICAL(&tp->data.lock);

        uint8_t buf[4] = {0};
        uint8 addr = 0xf0;
        buf[0]=0xa;buf[1]=0;buf[2]=0;buf[3]=0;
        touch_gsl3680_i2c_write(tp,addr, buf, 4);
        addr = 0x8;
        buf[0]=(uint8)(0xffffffff & 0xff);
        buf[1]=(uint8)((0xffffffff>>8) & 0xff);
        buf[2]=(uint8)((0xffffffff>>16) & 0xff);
        buf[3]=(uint8)((0xffffffff>>24) & 0xff);
        //SCI_TRACE_LOW("tmp1=%08x,buf[0]=%02x,buf[1]=%02x,buf[2]=%02x,buf[3]=%02x\n", tmp1,buf[0],buf[1],buf[2],buf[3]);
        touch_gsl3680_i2c_write(tp,addr, buf, 4);

        return ESP_OK;
    */


    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = touch_cnt;

    for (int i = 0; i < touch_cnt; i++) {
        size_t offset = 4 + (i * 4);

        uint16_t raw_x = ((uint16_t)(touch_data[offset + 3] & 0x0F) << 8) | touch_data[offset + 2];
        uint16_t raw_y = ((uint16_t)(touch_data[offset + 1]) << 8) | touch_data[offset + 0];

        // Scale to screen resolution
        uint16_t x = (raw_x * tp->config.y_max) / RAW_MAX_X;
        uint16_t y = (raw_y * tp->config.x_max) / RAW_MAX_Y;

        // Clamp to avoid overflow
        if (x >= tp->config.y_max) {
            x = tp->config.y_max - 1;
        }
        if (y >= tp->config.x_max) {
            y = tp->config.x_max - 1;
        }

        tp->data.coords[i].x = x;
        tp->data.coords[i].y = y;
        tp->data.coords[i].strength = 0;
    }

    portEXIT_CRITICAL(&tp->data.lock);
    return ESP_OK;
}

static bool esp_lcd_touch_gsl3680_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    assert(tp != NULL);
    assert(x != NULL);
    assert(y != NULL);
    assert(point_num != NULL);
    assert(max_point_num > 0);

    portENTER_CRITICAL(&tp->data.lock);

    /* Count of points */
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);

    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;

        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }

    /* Invalidate */
    tp->data.points = 0;

    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
static esp_err_t esp_lcd_touch_gsl3680_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state)
{
    esp_err_t err = ESP_OK;
    assert(tp != NULL);
    assert(state != NULL);

    *state = 0;

    portENTER_CRITICAL(&tp->data.lock);

    if (n > tp->data.buttons) {
        err = ESP_ERR_INVALID_ARG;
    } else {
        *state = tp->data.button[n].status;
    }

    portEXIT_CRITICAL(&tp->data.lock);

    return err;
}
#endif

static esp_err_t esp_lcd_touch_gsl3680_del(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    /* Reset GPIO pin settings */
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }

    /* Reset GPIO pin settings */
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }

    free(tp);

    return ESP_OK;
}

/*******************************************************************************
* Private API function
*******************************************************************************/

/* Init controller */
static esp_err_t esp_lcd_touch_gsl3680_init(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG, "start init");
    esp_lcd_touch_gsl3680_clear_reg(tp);
    touch_gsl3680_reset(tp);
    esp_lcd_touch_gsl3680_load_fw(tp);
    esp_lcd_touch_gsl3680_startup_chip(tp);
    touch_gsl3680_reset(tp);
    esp_lcd_touch_gsl3680_startup_chip(tp);

    return ESP_OK;
}

/* Reset controller */
static esp_err_t touch_gsl3680_reset(esp_lcd_touch_handle_t tp)
{
    esp_err_t err;
    unsigned char write_buf[4];
    uint8_t addr;
    assert(tp != NULL);

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level error!");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    addr = 0xe0;
    write_buf[0] = 0x88;
    err = touch_gsl3680_i2c_write(tp, addr, write_buf, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "I2C write error!");
    vTaskDelay(pdMS_TO_TICKS(10));

    addr = 0xe4;
    write_buf[0] = 0x04;
    err = touch_gsl3680_i2c_write(tp, addr, write_buf, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "I2C write error!");
    vTaskDelay(pdMS_TO_TICKS(10));

    write_buf[0] = 0x00;
    write_buf[1] = 0x00;
    write_buf[2] = 0x00;
    write_buf[3] = 0x00;
    err = touch_gsl3680_i2c_write(tp, 0xbc, write_buf, 4);
    ESP_RETURN_ON_ERROR(err, TAG, "I2C write error!");

    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

static esp_err_t touch_gsl3680_read_cfg(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[4];
    uint8_t write[4];
    // uint8_t i2c_buffer_read = 0;
    // uint8_t i2c_buffer_write = 0x12;

    write[0] = 0x12;
    write[1] = 0x34;
    write[2] = 0x56;
    assert(tp != NULL);

    ESP_LOGI(TAG, "GSL3680 connect");

    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "I2C read error!");
    ESP_LOGI(TAG, "GSL3680 read reg 0xf0 before is %x %x %x %x", buf[0], buf[1], buf[2], buf[3]);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "GSL3680 writing 0xf0 0x12");
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_write(tp, 0xf0, write, 4), TAG, "I2C write error");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, 0xf0, (uint8_t *)&buf, 4), TAG, "I2C read error!");
    ESP_LOGI(TAG, "GSL3680 read reg 0xf0 after is %x %x %x %x", buf[0], buf[1], buf[2], buf[3]);

    return ESP_OK;

    //TODO: Check this fix
    /*
    if(i2c_buffer_read == i2c_buffer_write)
    {
        ret = ESP_OK;
        ESP_LOGI(TAG, "Read config success");
    }
    else
        ret = ESP_FAIL;*/

    // return ret;
}

static esp_err_t esp_lcd_touch_gsl3680_startup_chip(esp_lcd_touch_handle_t tp)
{
    esp_err_t ret = ESP_OK;
    uint8_t write_buf[4];
    uint8_t addr = 0xe0;
    write_buf[0] = 0x00;
    ESP_LOGI(TAG, "enter");
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_write(tp, addr, write_buf, 1), TAG, "I2C write error");
    vTaskDelay(pdMS_TO_TICKS(10));

    //TODO: gsl_DataInit(gsl_config_data_id);
    return ret;
}

static esp_err_t esp_lcd_touch_gsl3680_read_ram_fw(esp_lcd_touch_handle_t tp)
{
    uint8_t read_buf[4];
    uint8_t addr = 0xb0;
    ESP_LOGI(TAG, "enter read_ram_fw");
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_RETURN_ON_ERROR(touch_gsl3680_i2c_read(tp, addr, (uint8_t *)&read_buf, 4), TAG, "I2C read error!");
    ESP_LOGI(TAG, "GSL3680 startup_chip failed read 0xb0 = %x,%x,%x,%x ", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
    if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t touch_gsl3680_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    assert(tp != NULL);
    assert(data != NULL);

    /* Read data */
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

static esp_err_t touch_gsl3680_i2c_write(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    assert(tp != NULL);

    // *INDENT-OFF*
    /* Write data */
    return esp_lcd_panel_io_tx_param(tp->io, reg, data, len);
    // *INDENT-ON*
}

static esp_err_t esp_lcd_touch_gsl3680_load_fw(esp_lcd_touch_handle_t tp)
{
    ESP_LOGI(TAG, "start load fw");
    uint16_t addr;
    unsigned char wrbuf[4];
    uint16_t source_line = 0;
    uint16_t source_len = sizeof(GSLX680_FW) / sizeof(struct fw_data);

    for (source_line = 0; source_line < source_len; source_line++) {
        addr = GSLX680_FW[source_line].offset;
        wrbuf[0] = (uint8_t)(GSLX680_FW[source_line].val & 0x000000ff);
        wrbuf[1] = (uint8_t)((GSLX680_FW[source_line].val & 0x0000ff00) >> 8);
        wrbuf[2] = (uint8_t)((GSLX680_FW[source_line].val & 0x00ff0000) >> 16);
        wrbuf[3] = (uint8_t)((GSLX680_FW[source_line].val & 0xff000000) >> 24);
        if (addr == 0xf0) {
            touch_gsl3680_i2c_write(tp, addr, wrbuf, 1);
        } else {
            touch_gsl3680_i2c_write(tp, addr, wrbuf, 4);
        }

    }
    ESP_LOGI(TAG, "load fw success");
    return ESP_OK;
}

static esp_err_t esp_lcd_touch_gsl3680_clear_reg(esp_lcd_touch_handle_t tp)
{
    uint8_t addr;
    uint8_t wrbuf[4];

    ESP_LOGI(TAG, "clear reg");
    addr = 0xe0;
    wrbuf[0] = 0x88;
    touch_gsl3680_i2c_write(tp, addr, wrbuf, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    addr = 0x88;
    wrbuf[0] = 0x01;
    touch_gsl3680_i2c_write(tp, addr, wrbuf, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe4;
    wrbuf[0] = 0x04;
    touch_gsl3680_i2c_write(tp, addr, wrbuf, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    addr = 0xe0;
    wrbuf[0] = 0x00;
    touch_gsl3680_i2c_write(tp, addr, wrbuf, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

#endif // ESP_PANEL_DRIVERS_TOUCH_ENABLE_GSL3680
