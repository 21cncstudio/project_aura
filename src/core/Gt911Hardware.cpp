// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Gt911Hardware.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_display_panel.hpp>

#include "core/Gt911AddressSelect.h"
#include "core/Logger.h"
#include "esp_panel_board_custom_conf.h"

namespace Gt911Hardware {
namespace {

struct Context {
    esp_expander::Base *expander = nullptr;
    gpio_num_t int_gpio = GPIO_NUM_NC;
    int reset_exio = -1;
};

bool setIntOutput(void *opaque) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_set_direction(ctx->int_gpio, GPIO_MODE_OUTPUT) == ESP_OK;
}

bool setIntLevel(void *opaque, bool high) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_set_level(ctx->int_gpio, high ? 1 : 0) == ESP_OK;
}

bool setResetLevel(void *opaque, bool high) {
    auto *ctx = static_cast<Context *>(opaque);
    return ctx->expander != nullptr &&
           ctx->expander->digitalWrite(ctx->reset_exio, high ? 1 : 0);
}

bool releaseInt(void *opaque) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_reset_pin(ctx->int_gpio) == ESP_OK;
}

void delayMs(void *, uint32_t delay_ms) {
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

} // namespace

bool selectConfiguredAddress(esp_panel::board::Board *board) {
    if (board == nullptr || board->getIO_Expander() == nullptr) {
        LOGE("GT911", "address select unavailable: board/expander missing");
        return false;
    }

    Context context{
        board->getIO_Expander()->getBase(),
        static_cast<gpio_num_t>(ESP_PANEL_BOARD_TOUCH_INT_IO),
        1,
    };
    if (context.expander == nullptr || context.int_gpio == GPIO_NUM_NC) {
        LOGE("GT911", "address select unavailable: invalid reset/INT path");
        return false;
    }

    const Gt911AddressSelect::Ops ops{
        &context,
        setIntOutput,
        setIntLevel,
        setResetLevel,
        releaseInt,
        delayMs,
    };
    const Gt911AddressSelect::Result result =
        Gt911AddressSelect::selectAddress(ops, ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS);
    if (!result.ok()) {
        LOGE("GT911",
             "address select failed at %s",
             Gt911AddressSelect::failureText(result.failure));
        return false;
    }
    return true;
}

} // namespace Gt911Hardware
