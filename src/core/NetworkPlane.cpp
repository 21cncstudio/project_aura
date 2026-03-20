// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/NetworkPlane.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Logger.h"
#include "core/NetworkCommandQueue.h"
#include "core/ConnectivityRuntime.h"
#include "core/MqttRuntimeState.h"
#include "core/Watchdog.h"
#include "modules/MqttManager.h"
#include "modules/NetworkManager.h"
#include "web/WebRuntime.h"
#include "web/WebUiBridge.h"

namespace {

constexpr uint32_t kNetworkTaskStackSize = 8192;
constexpr UBaseType_t kNetworkTaskPriority = 1;
constexpr BaseType_t kNetworkTaskCore = 0;
constexpr uint32_t kNetworkTaskDelayNormalMs = 10;

TaskHandle_t g_network_task_handle = nullptr;

void network_plane_task(void *arg) {
    auto *ctx = static_cast<NetworkPlane::Context *>(arg);
    if (!Watchdog::subscribeCurrentTask()) {
        LOGW("Main", "network task is not subscribed to Task WDT");
    }
    LOGI("Main", "Network task running on core: %d", xPortGetCoreID());
    for (;;) {
        Watchdog::kick();
        ctx->networkCommandQueue.processAll(ctx->networkManager,
                                            ctx->mqttManager,
                                            ctx->connectivityRuntime);
        Watchdog::kick();
        ctx->networkManager.poll();
        Watchdog::kick();
        ctx->connectivityRuntime.update(ctx->networkManager, ctx->mqttManager);
        ctx->mqttManager.poll(ctx->mqttRuntimeState);
        Watchdog::kick();
        ctx->connectivityRuntime.update(ctx->networkManager, ctx->mqttManager);
        const TickType_t delay_ticks = WebHandlersIsOtaBusy()
                                           ? 1
                                           : pdMS_TO_TICKS(kNetworkTaskDelayNormalMs);
        Watchdog::kick();
        vTaskDelay(delay_ticks == 0 ? 1 : delay_ticks);
    }
}

} // namespace

bool NetworkPlane::start(Context &ctx) {
    if (g_network_task_handle != nullptr) {
        return true;
    }

    TaskHandle_t created = nullptr;
    const BaseType_t ok = xTaskCreatePinnedToCore(network_plane_task,
                                                  "network",
                                                  kNetworkTaskStackSize,
                                                  &ctx,
                                                  kNetworkTaskPriority,
                                                  &created,
                                                  kNetworkTaskCore);
    if (ok != pdPASS || created == nullptr) {
        LOGE("Main", "failed to start network task");
        return false;
    }

    g_network_task_handle = created;
    ctx.webUiBridge.setDispatchMode(WebUiBridge::DispatchMode::DeferredReply);
    return true;
}

bool NetworkPlane::isRunning() {
    return g_network_task_handle != nullptr;
}
