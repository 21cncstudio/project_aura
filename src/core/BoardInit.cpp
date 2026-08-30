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
#include "core/BoardInitTaskLifecycle.h"
#include "core/BootState.h"
#include "core/Ch422gReadyProbe.h"
#include "core/Logger.h"
#include "lvgl_v8_port.h"

namespace BoardInit {
namespace {

using esp_panel::board::Board;
using esp_panel::drivers::BusRGB;

constexpr uint32_t kBeginTimeoutMs = 10000;
// Prepare CH422G before the display library touches it. The safe sequence
// preloads the output latches, enables the outputs, waits for the attached
// display load to settle, and only then validates the vendor reset writes.
// A two-second bound leaves room for fresh-host retries on a marginal bus
// without allowing startup to loop indefinitely.
constexpr uint32_t kStartupProbeTimeoutMs = 2000;
constexpr uint32_t kStartupProbePollMs = 250;
constexpr uint32_t kStartupProbeLoadSettleMs = 250;
constexpr uint32_t kStartupProbeTransactionTimeoutMs = 25;
constexpr uint32_t kStartupProbeHostHandoffMs = 15;
// This probe runs only after the vendor init has already failed at the
// expander stage. Keep it to one bounded attempt so it records the immediate
// post-failure bus state without turning diagnostics into another recovery
// path.
constexpr uint32_t kPostFailureProbeTimeoutMs = 200;
constexpr uint32_t kPostFailureProbePollMs = 200;
constexpr uint32_t kPostFailureProbeLoadSettleMs = 25;
constexpr uint32_t kPostFailureProbeTransactionTimeoutMs = 25;
// Board::begin() allocates the shared-I2C, touch-GPIO and RGB-panel interrupts
// on the calling core. Keep the board-owned interrupt set on the Arduino/LVGL
// core instead of routing it through the Core 0 network/system workload.
constexpr BaseType_t kBoardInitCore = ARDUINO_RUNNING_CORE;

using BeginResult = BoardInitPolicy::BeginOutcome;

struct BeginContext {
    Board *board = nullptr;
    TaskHandle_t waiter = nullptr;
    std::atomic<bool> success{false};
    BoardInitTaskLifecycle::Lifecycle lifecycle;
};

std::atomic<uint8_t> g_stage{static_cast<uint8_t>(Stage::Bus)};

[[noreturn]] void parkBoardBeginTaskUntilDeleted() {
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void boardBeginTask(void *argument) {
    auto *context = static_cast<BeginContext *>(argument);
    if (context == nullptr) {
        LOGE("Main", "board_init task started without context");
        parkBoardBeginTaskUntilDeleted();
    }

    LOGI("Main", "[Core %d] Starting board->begin()...", xPortGetCoreID());
    const bool success = context->board != nullptr && context->board->begin();
    context->success.store(success, std::memory_order_relaxed);
    if (!success) {
        LOGE("Main", "Board begin failed at stage=%s", stageText(static_cast<Stage>(g_stage.load())));
    }

    // Completion and timeout compete for ownership with one CAS. If timeout
    // won, the parent is already responsible for deleting this task; do not
    // notify it or touch the stack-owned context again.
    if (!context->lifecycle.childPublishCompletion()) {
        parkBoardBeginTaskUntilDeleted();
    }

    xTaskNotifyGive(context->waiter);

    // The parent acknowledges the completion notification before allowing the
    // child to publish DeleteReady. The second notification proves that the
    // child no longer accesses the stack-owned context, so it cannot leak into
    // a later attempt or race parent deletion.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (context->lifecycle.childPublishDeleteReady()) {
        xTaskNotifyGive(context->waiter);
    }
    parkBoardBeginTaskUntilDeleted();
}

BeginResult runBoardBeginOnce(Board *board) {
    BeginContext context;
    TaskHandle_t begin_task = nullptr;

    // No board-init notification is allowed to survive a completed attempt.
    // Clear a defensive pre-existing count before creating this attempt's
    // child; the two-phase delete-ready handshake consumes both notifications
    // generated below before returning.
    (void)ulTaskNotifyTake(pdTRUE, 0);
    context.board = board;
    context.waiter = xTaskGetCurrentTaskHandle();

    const BaseType_t created = xTaskCreatePinnedToCore(
        boardBeginTask,
        "board_init",
        8192,
        &context,
        1,
        &begin_task,
        kBoardInitCore);
    if (created != pdPASS || begin_task == nullptr) {
        LOGE("Main", "Failed to create board_init task");
        return BeginResult::TaskCreateFailed;
    }

    uint32_t notified =
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kBeginTimeoutMs));
    if (notified == 0) {
        const BoardInitTaskLifecycle::TimeoutClaim timeout_claim =
            context.lifecycle.parentClaimTimeout();
        if (timeout_claim ==
            BoardInitTaskLifecycle::TimeoutClaim::CancelOwned) {
            LOGE("Main",
                 "board->begin() timeout at stage=%s",
                 stageText(static_cast<Stage>(g_stage.load())));
            vTaskDelete(begin_task);
            return BeginResult::Timeout;
        }

        if (timeout_claim !=
            BoardInitTaskLifecycle::TimeoutClaim::CompletionOwned) {
            LOGE("Main", "Invalid board_init timeout ownership state");
            for (;;) {
                vTaskDelay(portMAX_DELAY);
            }
        }

        // The child completed exactly on the timeout boundary and won the CAS
        // before publishing its notification. Wait for that notification and
        // classify the real result instead of reporting a false timeout.
        notified = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    if (notified == 0 ||
        !context.lifecycle.parentAcknowledgeCompletion()) {
        LOGE("Main", "Invalid board_init completion handshake");
        // The lifecycle invariant makes this branch unreachable. Do not let
        // the stack-owned context escape while the child could still use it.
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    const bool success = context.success.load(std::memory_order_acquire);
    xTaskNotifyGive(begin_task);
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (!context.lifecycle.parentOwnsDeletion() ||
        context.lifecycle.state() !=
            BoardInitTaskLifecycle::State::DeleteReady) {
        LOGE("Main", "Invalid board_init delete-ready handshake");
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    vTaskDelete(begin_task);
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

Ch422gReadyProbe::Result prepareExpanderForVendorInit() {
    LOGI("Main", "Preparing CH422G outputs before vendor board init");
    const Ch422gReadyProbe::Result probe = Ch422gReadyProbe::wait(
        Config::I2C_PORT,
        static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
        static_cast<gpio_num_t>(Config::I2C_SCL_PIN),
        Config::I2C_FREQ_HZ,
        kStartupProbeTimeoutMs,
        kStartupProbePollMs,
        kStartupProbeLoadSettleMs,
        kStartupProbeTransactionTimeoutMs);
    LOGI("Main",
         "CH422G startup prepare: status=%s phase=%s attempts=%u recoveries=%u wait=%lu ms failed=0x%02X/0x%02X error=%d lines_valid=%u lines=%u/%u recovered=%u/%u pulses=%u",
         Ch422gReadyProbe::statusText(probe.status),
         Ch422gReadyProbe::phaseText(probe.phase),
         static_cast<unsigned>(probe.attempts),
         static_cast<unsigned>(probe.bus_recoveries),
         static_cast<unsigned long>(probe.waited_ms),
         static_cast<unsigned>(probe.failed_address),
         static_cast<unsigned>(probe.failed_value),
         static_cast<int>(probe.last_error),
         probe.failure_lines_valid ? 1u : 0u,
         probe.failure_sda_high ? 1u : 0u,
         probe.failure_scl_high ? 1u : 0u,
         probe.recovery_sda_high ? 1u : 0u,
         probe.recovery_scl_high ? 1u : 0u,
         static_cast<unsigned>(probe.recovery_pulses));
    return probe;
}

Ch422gReadyProbe::Result probeExpanderAfterVendorFailure() {
    // Board destruction releases the vendor-owned legacy I2C host. Give that
    // cleanup a short handoff before installing the probe's temporary host.
    vTaskDelay(pdMS_TO_TICKS(5));
    const Ch422gReadyProbe::Result probe = Ch422gReadyProbe::wait(
        Config::I2C_PORT,
        static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
        static_cast<gpio_num_t>(Config::I2C_SCL_PIN),
        Config::I2C_FREQ_HZ,
        kPostFailureProbeTimeoutMs,
        kPostFailureProbePollMs,
        kPostFailureProbeLoadSettleMs,
        kPostFailureProbeTransactionTimeoutMs);
    LOGW("Main",
         "CH422G post-failure probe: status=%s phase=%s attempts=%u recoveries=%u wait=%lu ms failed=0x%02X/0x%02X error=%d lines_valid=%u lines=%u/%u recovered=%u/%u pulses=%u",
         Ch422gReadyProbe::statusText(probe.status),
         Ch422gReadyProbe::phaseText(probe.phase),
         static_cast<unsigned>(probe.attempts),
         static_cast<unsigned>(probe.bus_recoveries),
         static_cast<unsigned long>(probe.waited_ms),
         static_cast<unsigned>(probe.failed_address),
         static_cast<unsigned>(probe.failed_value),
         static_cast<int>(probe.last_error),
         probe.failure_lines_valid ? 1u : 0u,
         probe.failure_sda_high ? 1u : 0u,
         probe.failure_scl_high ? 1u : 0u,
         probe.recovery_sda_high ? 1u : 0u,
         probe.recovery_scl_high ? 1u : 0u,
         static_cast<unsigned>(probe.recovery_pulses));
    return probe;
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
    const BoardInitPolicy::PreInitAction pre_init_action =
        BoardInitPolicy::preInitAction(auto_recovery_boot, recovery_samples);
    if (pre_init_action == BoardInitPolicy::PreInitAction::RecoverThenVendorInit) {
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
            LOGW("Main",
                 "I2C lines remain stuck after bounded recovery; continuing with one vendor init attempt");
        }
        delay(5);
    }
    if (result.cold_power_start) {
        boot_mark_board_power_settle_complete();
        LOGI("Main", "Cold power start: preparing CH422G before vendor board init");
    }

    result.last_stage = Stage::Expander;
    noteStage(Stage::Expander);
    result.expander_probe = prepareExpanderForVendorInit();
    if (!result.expander_probe.ready()) {
        result.failure = Failure::ExpanderNotReady;
        LOGE("Main",
             "CH422G safe startup preparation failed after %lu ms at phase=%s address=0x%02X error=%d",
             static_cast<unsigned long>(result.expander_probe.waited_ms),
             Ch422gReadyProbe::phaseText(result.expander_probe.phase),
             static_cast<unsigned>(result.expander_probe.failed_address),
             static_cast<int>(result.expander_probe.last_error));
        return result;
    }

    // The readiness probe owns a temporary legacy I2C host and deletes it
    // before returning. Give that teardown a deterministic handoff window
    // before the display library installs its shared host.
    vTaskDelay(pdMS_TO_TICKS(kStartupProbeHostHandoffMs));
    LOGI("Main", "CH422G outputs stable; starting one bounded vendor board attempt");

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
    const BoardInitPolicy::CompletionAction completion_action =
        BoardInitPolicy::completionAction(begin_result);
    if (completion_action == BoardInitPolicy::CompletionAction::UseBoard) {
        noteStage(Stage::Complete);
        result.board = board;
        result.failure = Failure::None;
        result.last_stage = Stage::Complete;
        boot_i2c_recovered = true;
        LOGI("Main", "Board initialized by vendor path");
        return result;
    }

    if (completion_action == BoardInitPolicy::CompletionAction::RetainUntilRestart) {
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
    if (begin_result == BeginResult::Failed && result.last_stage == Stage::Expander) {
        result.expander_probe = probeExpanderAfterVendorFailure();
    }
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
