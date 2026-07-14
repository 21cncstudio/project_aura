// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BoardInit.h"

#include <Arduino.h>
#include <atomic>
#include <new>

#include <esp_display_panel.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/AppConfig.h"
#include "BoardInitCallbacks.h"
#include "core/BoardInitPolicy.h"
#include "core/BootHelpers.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "lvgl_v8_port.h"

namespace BoardInit {
namespace {

using esp_panel::board::Board;
using esp_panel::drivers::BusRGB;

constexpr uint8_t kMaxRounds = 3;
constexpr uint32_t kRetryDelayMs = 300;
constexpr uint32_t kBeginTimeoutMs = 10000;
constexpr uint32_t kColdReleaseWaitMs = 2000;
constexpr uint32_t kColdReleasePollMs = 100;

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

BoardInitPolicy::AttemptOutcome policyOutcome(BeginResult result) {
    switch (result) {
        case BeginResult::Success: return BoardInitPolicy::AttemptOutcome::Success;
        case BeginResult::TaskCreateFailed: return BoardInitPolicy::AttemptOutcome::TaskCreateFailed;
        case BeginResult::Timeout: return BoardInitPolicy::AttemptOutcome::Timeout;
        case BeginResult::Failed:
        default: return BoardInitPolicy::AttemptOutcome::Failed;
    }
}

void logRecovery(uint8_t round, const I2cBusRecovery::Result &result, uint32_t waited_ms) {
    LOGI("Main",
         "I2C round %u: status=%s before=%u/%u after=%u/%u pulses=%u wait=%lu ms",
         static_cast<unsigned>(round),
         I2cBusRecovery::statusText(result.status),
         result.before.sda_high ? 1u : 0u,
         result.before.scl_high ? 1u : 0u,
         result.after.sda_high ? 1u : 0u,
         result.after.scl_high ? 1u : 0u,
         static_cast<unsigned>(result.pulses),
         static_cast<unsigned long>(waited_ms));
}

I2cBusRecovery::Result prepareBus(uint8_t round) {
    const gpio_num_t sda = static_cast<gpio_num_t>(Config::I2C_SDA_PIN);
    const gpio_num_t scl = static_cast<gpio_num_t>(Config::I2C_SCL_PIN);
    I2cBusRecovery::Result result = BootHelpers::recoverI2CBus(sda, scl);
    uint32_t waited_ms = 0;

    if (!result.busReady() && round == 1 && boot_reset_reason == ESP_RST_POWERON) {
        const uint32_t wait_start_ms = millis();
        while (waited_ms < kColdReleaseWaitMs) {
            vTaskDelay(pdMS_TO_TICKS(kColdReleasePollMs));
            waited_ms = millis() - wait_start_ms;
            if (I2cBusRecovery::sample(sda, scl).idle()) {
                break;
            }
        }
        result = BootHelpers::recoverI2CBus(sda, scl);
    }

    logRecovery(round, result, waited_ms);
    boot_i2c_recovered = result.busReady();
    return result;
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

void logMemory(const char *phase, uint8_t round) {
    LOGI("Main",
         "Board round %u %s: heap=%u psram=%u",
         static_cast<unsigned>(round),
         phase,
         static_cast<unsigned>(ESP.getFreeHeap()),
         static_cast<unsigned>(ESP.getFreePsram()));
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

Result initBoard(const I2cBusRecovery::LineState &early_state) {
    Result result{};
    LOGI("Main",
         "Early I2C sample: SDA=%u SCL=%u",
         early_state.sda_high ? 1u : 0u,
         early_state.scl_high ? 1u : 0u);
    LOGI("Main", "Initializing board with %u clean rounds", static_cast<unsigned>(kMaxRounds));

    for (uint8_t round = 1; round <= kMaxRounds; ++round) {
        result.rounds = round;
        result.last_stage = Stage::Bus;
        g_stage.store(static_cast<uint8_t>(Stage::Bus), std::memory_order_relaxed);
        result.last_recovery = prepareBus(round);

        if (!result.last_recovery.busReady()) {
            result.failure = Failure::BusStuck;
            LOGW("Main", "Board round %u/%u skipped: I2C bus not idle",
                 static_cast<unsigned>(round), static_cast<unsigned>(kMaxRounds));
            if (round < kMaxRounds) {
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
            }
            continue;
        }

        logMemory("before allocation", round);
        Board *board = new (std::nothrow) Board();
        if (board == nullptr) {
            result.failure = Failure::Allocation;
            LOGE("Main", "Board allocation failed in round %u", static_cast<unsigned>(round));
            if (round < kMaxRounds) {
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
            }
            continue;
        }
        LOGI("Main", "Board generation %u created @%p", static_cast<unsigned>(round), board);

        if (!board->init() || !configureBoard(board)) {
            result.failure = Failure::Init;
            LOGE("Main", "Board init failed in round %u", static_cast<unsigned>(round));
            LOGI("Main", "Board generation %u deleting @%p", static_cast<unsigned>(round), board);
            delete board;
            logMemory("after init cleanup", round);
            if (round < kMaxRounds) {
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
            }
            continue;
        }

        ++result.begin_attempts;
        const BeginResult begin_result = runBoardBeginOnce(board);
        result.last_stage = static_cast<Stage>(g_stage.load(std::memory_order_acquire));
        const BoardInitPolicy::Action action = BoardInitPolicy::decide(policyOutcome(begin_result), round, kMaxRounds);

        if (action == BoardInitPolicy::Action::ReturnSuccess) {
            noteStage(Stage::Complete);
            result.board = board;
            result.failure = Failure::None;
            result.last_stage = Stage::Complete;
            LOGI("Main", "Board initialized in round %u", static_cast<unsigned>(round));
            return result;
        }

        if (begin_result == BeginResult::Timeout) {
            // The task was deleted inside vendor code. Destructing the partially
            // active object could touch inconsistent locks or device handles.
            result.failure = Failure::Timeout;
            LOGE("Main", "Board timeout: retaining unsafe generation @%p until restart", board);
            return result;
        }

        result.failure = (begin_result == BeginResult::TaskCreateFailed) ? Failure::TaskCreate : Failure::Begin;
        LOGW("Main",
             "Board round %u/%u failed at stage=%s; destroying generation before recovery",
             static_cast<unsigned>(round),
             static_cast<unsigned>(kMaxRounds),
             stageText(result.last_stage));
        LOGI("Main", "Board generation %u deleting @%p", static_cast<unsigned>(round), board);
        delete board;
        logMemory("after cleanup", round);

        if (action == BoardInitPolicy::Action::RetryFresh) {
            vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
        }
    }

    LOGE("Main",
         "Board init failed: reason=%s stage=%s rounds=%u begin_attempts=%u",
         failureText(result.failure),
         stageText(result.last_stage),
         static_cast<unsigned>(result.rounds),
         static_cast<unsigned>(result.begin_attempts));
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
