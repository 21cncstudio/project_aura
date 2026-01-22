#include "core/BoardInit.h"

#include <assert.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_display_panel.hpp>

#include "lvgl_v8_port.h"
#include "core/Logger.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

namespace {

Board *g_board_ptr = nullptr;
volatile bool g_board_begin_done = false;
volatile bool g_board_begin_success = false;

void board_begin_task(void *) {
    LOGI("Main", "[Core %d] Starting board->begin()...", xPortGetCoreID());
    g_board_begin_success = g_board_ptr->begin();
    if (!g_board_begin_success) {
        LOGE("Main", "Board begin failed!");
    }
    g_board_begin_done = true;
    vTaskDelete(nullptr);
}

} // namespace

esp_panel::board::Board *BoardInit::initBoard() {
    LOGI("Main", "Initializing board");
    Board *board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    // When avoid tearing function is enabled, the frame buffer number should be set in the board driver
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
     * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
     * "bounce buffer" functionality to enhance the RGB data bandwidth.
     * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
     */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif

    // Run board->begin() on Core 0 to avoid IPC stack overflow.
    LOGI("Main", "Running on core: %d", xPortGetCoreID());
    g_board_ptr = board;
    g_board_begin_done = false;
    g_board_begin_success = false;
    xTaskCreatePinnedToCore(
        board_begin_task,
        "board_init",
        8192,
        nullptr,
        1,
        nullptr,
        0  // Core 0
    );
    while (!g_board_begin_done) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    assert(g_board_begin_success);
    return board;
}
