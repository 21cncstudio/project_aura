// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardInit.h"

#include <Arduino.h>
#include <atomic>
#include <new>

#include <esp_display_panel.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BoardInitCallbacks.h"
#include "config/AppConfig.h"
#include "core/BoardInitPolicy.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "lvgl_v8_port.h"

namespace BoardInit {
namespace {

using esp_panel::board::Board;
using esp_panel::drivers::BusRGB;

constexpr uint32_t kBeginTimeoutMs = 10000;

enum class BeginResult : uint8_t {
    Success = 0,
    Failed,
    TaskCreateFailed,
    Timeout,
};

struct BeginContext {
    Board *board = nullptr;
    TaskHandle_t waiter = nullptr;
    std::atomic<bool> success{false};
};

BeginContext g_begin_context;
TaskHandle_t g_begin_task = nullptr;
std::atomic<uint8_t> g_stage{static_cast<uint8_t>(Stage::Bus)};

void boardBeginTask(void *) {
    LOGI("Main", "[Core %d] Starting board->begin()...", xPortGetCoreID());
    const bool success = g_begin_context.board != nullptr && g_begin_context.board->begin();
    g_begin_context.success.store(success, std::memory_order_release);
    if (!success) {
        LOGE("Main", "Board begin failed at stage=%s", stageText(static_cast<Stage>(g_stage.load())));
    }
    xTaskNotifyGive(g_begin_context.waiter);

    // Parent owns the task lifetime. This acknowledgement removes the race
    // between timeout deletion and child self-deletion.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelete(nullptr);
}

BeginResult runBoardBeginOnce(Board *board) {
    (void)ulTaskNotifyTake(pdTRUE, 0);
    g_begin_context.board = board;
    g_begin_context.waiter = xTaskGetCurrentTaskHandle();
    g_begin_context.success.store(false, std::memory_order_relaxed);
    g_begin_task = nullptr;

    const BaseType_t created = xTaskCreatePinnedToCore(
        boardBeginTask,
        "board_init",
        8192,
        nullptr,
        1,
        &g_begin_task,
        0);
    if (created != pdPASS || g_begin_task == nullptr) {
        LOGE("Main", "Failed to create board_init task");
        g_begin_task = nullptr;
        return BeginResult::TaskCreateFailed;
    }

    const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kBeginTimeoutMs));
    if (notified == 0) {
        LOGE("Main", "board->begin() timeout at stage=%s", stageText(static_cast<Stage>(g_stage.load())));
        vTaskDelete(g_begin_task);
        g_begin_task = nullptr;
        return BeginResult::Timeout;
    }

    const bool success = g_begin_context.success.load(std::memory_order_acquire);
    xTaskNotifyGive(g_begin_task);
    g_begin_task = nullptr;
    return success ? BeginResult::Success : BeginResult::Failed;
}

bool configureBoard(Board *board) {
    auto lcd = board->getLCD();
    if (lcd == nullptr) {
        LOGE("Main", "Board init returned no LCD");
        return false;
    }

#if LVGL_PORT_AVOID_TEARING_MODE
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus != nullptr && lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    return true;
}

void logMemory(const char *phase) {
    LOGI("Main",
         "Board %s: heap=%u psram=%u",
         phase,
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(ESP.getFreePsram()));
}

I2cBusRecovery::Result observeBus(const I2cBusRecovery::LineState &state) {
    I2cBusRecovery::Result result{};
    result.before = state;
    result.after = state;
    if (state.sda_high && state.scl_high) {
        result.status = I2cBusRecovery::Status::Idle;
    } else if (!state.sda_high && !state.scl_high) {
        result.status = I2cBusRecovery::Status::BothStuckLow;
    } else if (!state.sda_high) {
        result.status = I2cBusRecovery::Status::SdaStuckLow;
    } else {
        result.status = I2cBusRecovery::Status::SclStuckLow;
    }
    return result;
}

} // namespace

void noteStage(Stage stage) {
    g_stage.store(static_cast<uint8_t>(stage), std::memory_order_release);
    LOGI("Main", "Board stage: %s", stageText(stage));
}

const char *failureText(Failure failure) {
    switch (failure) {
        case Failure::None: return "none";
        case Failure::BusStuck: return "bus_stuck";
        case Failure::ExpanderNotReady: return "expander_not_ready";
        case Failure::Allocation: return "allocation";
        case Failure::Init: return "init";
        case Failure::TaskCreate: return "task_create";
        case Failure::Begin: return "begin";
        case Failure::Timeout: return "timeout";
        default: return "unknown";
    }
}

const char *stageText(Stage stage) {
    switch (stage) {
        case Stage::Bus: return "bus";
        case Stage::Expander: return "expander";
        case Stage::Lcd: return "lcd";
        case Stage::Touch: return "touch";
        case Stage::Backlight: return "backlight";
        case Stage::Complete: return "complete";
        default: return "unknown";
    }
}

Result initBoard(const I2cBusRecovery::LineState &early_state,
                 const I2cBusRecovery::LineState &pre_init_state,
                 bool auto_recovery_boot) {
    Result result{};
    result.rounds = 1;
    result.cold_power_start = boot_board_cold_start;
    result.last_recovery = observeBus(pre_init_state);
    LOGI("Main",
         "Early I2C sample: SDA=%u SCL=%u",
         early_state.sda_high ? 1u : 0u,
         early_state.scl_high ? 1u : 0u);
    LOGI("Main",
         "Pre-init I2C sample: SDA=%u SCL=%u",
         pre_init_state.sda_high ? 1u : 0u,
         pre_init_state.scl_high ? 1u : 0u);
    const BoardInitPolicy::PreInitI2cSamples recovery_samples{
        early_state.sda_high,
        early_state.scl_high,
        pre_init_state.sda_high,
        pre_init_state.scl_high,
    };
    if (BoardInitPolicy::shouldRecoverI2cBeforeInit(
            auto_recovery_boot,
            recovery_samples)) {
        LOGW("Main", "Marked recovery boot found stuck I2C lines; applying one pre-init recovery");
        result.last_recovery = I2cBusRecovery::recover(
            static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::I2C_SCL_PIN));
        LOGW("Main",
             "Pre-init I2C recovery: status=%s pulses=%u SDA=%u SCL=%u",
             I2cBusRecovery::statusText(result.last_recovery.status),
             static_cast<unsigned>(result.last_recovery.pulses),
             result.last_recovery.after.sda_high ? 1u : 0u,
             result.last_recovery.after.scl_high ? 1u : 0u);
        if (!result.last_recovery.busReady()) {
            result.failure = Failure::BusStuck;
            LOGE("Main", "I2C lines remain stuck after bounded recovery");
            return result;
        }
        delay(5);
    }
    if (result.cold_power_start) {
        boot_mark_board_power_settle_complete();
        LOGI("Main", "Cold power start: using vendor board init without an I2C pre-probe");
    }
    LOGI("Main", "Initializing board with one vendor attempt and no I2C pre-probe");

    result.last_stage = Stage::Bus;
    g_stage.store(static_cast<uint8_t>(Stage::Bus), std::memory_order_relaxed);
    logMemory("before allocation");
    Board *board = new (std::nothrow) Board();
    if (board == nullptr) {
        result.failure = Failure::Allocation;
        LOGE("Main", "Board allocation failed");
        return result;
    }
    LOGI("Main", "Board created @%p", board);

    if (!board->init() || !configureBoard(board)) {
        result.failure = Failure::Init;
        LOGE("Main", "Board init failed");
        LOGI("Main", "Deleting board @%p", board);
        delete board;
        logMemory("after init cleanup");
        return result;
    }

    ++result.begin_attempts;
    const BeginResult begin_result = runBoardBeginOnce(board);
    result.last_stage = static_cast<Stage>(g_stage.load(std::memory_order_acquire));
    if (begin_result == BeginResult::Success) {
        noteStage(Stage::Complete);
        result.board = board;
        result.failure = Failure::None;
        result.last_stage = Stage::Complete;
        boot_i2c_recovered = true;
        LOGI("Main", "Board initialized by vendor path");
        return result;
    }

    if (begin_result == BeginResult::Timeout) {
        // The task was deleted inside vendor code. Destructing the partially
        // active object could touch inconsistent locks or device handles.
        result.failure = Failure::Timeout;
        LOGE("Main", "Board timeout: retaining unsafe object @%p until restart", board);
        return result;
    }

    result.failure = (begin_result == BeginResult::TaskCreateFailed) ? Failure::TaskCreate : Failure::Begin;
    LOGW("Main", "Vendor board begin failed at stage=%s", stageText(result.last_stage));
    LOGI("Main", "Deleting board @%p", board);
    delete board;
    logMemory("after cleanup");
    return result;
}

} // namespace BoardInit

void auraBoardInitNoteStage(AuraBoardInitStage stage) {
    BoardInit::noteStage(static_cast<BoardInit::Stage>(stage));
}

bool auraBoardInitStageResult(AuraBoardInitStage stage, bool success) {
    const auto board_stage = static_cast<BoardInit::Stage>(stage);
    if (success) {
        LOGI("Main", "Board stage complete: %s", BoardInit::stageText(board_stage));
    } else {
        LOGE("Main", "Board stage failed: %s", BoardInit::stageText(board_stage));
    }
    return success;
}
