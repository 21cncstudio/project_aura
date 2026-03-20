// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebHandlers.h"

#include "web/WebChartsApiHandlers.h"
#include "web/WebDacApiHandlers.h"
#include "web/WebHandlersSupport.h"
#include "web/WebMqttHandlers.h"
#include "web/WebOtaHandlers.h"
#include "web/WebPortalHandlers.h"
#include "web/WebSettingsApiHandlers.h"
#include "web/WebShellAssetHandlers.h"
#include "web/WebSystemApiHandlers.h"
#include "web/WebThemeApiHandlers.h"
#include "web/WebWifiHandlers.h"

namespace {

template <typename Fn>
void with_context(Fn &&fn) {
    WebHandlerContext *context = WebHandlersSupport::context();
    if (!context) {
        return;
    }
    fn(*context);
}

template <typename Fn>
void with_response_context(Fn &&fn) {
    with_context([&fn](WebHandlerContext &context) {
        fn(context, WebHandlersSupport::responseContext());
    });
}

template <typename Fn>
void with_ota_busy(Fn &&fn) {
    const bool ota_busy = WebHandlersSupport::isOtaBusy();
    with_context([&fn, ota_busy](WebHandlerContext &context) {
        fn(context, ota_busy);
    });
}

} // namespace

void WebHandlersInit(WebHandlerContext *context) {
    WebHandlersSupport::init(context);
}

bool WebHandlersIsOtaBusy() {
    return WebHandlersSupport::isOtaBusy();
}

bool WebHandlersConsumeRestartRequest() {
    return WebHandlersSupport::consumeRestartRequest();
}

void WebHandlersRequestRestart(uint32_t delay_ms) {
    WebHandlersSupport::requestRestart(delay_ms);
}

void WebHandlersBeginRestartShutdown() {
    WebHandlersSupport::beginRestartShutdown();
}

bool WebHandlersShouldPauseMqttConnect() {
    return WebHandlersSupport::shouldPauseMqttForTransfer();
}

bool WebHandlersShouldPauseMqttPublish() {
    return WebHandlersSupport::shouldPauseMqttForTransfer();
}

void WebHandlersNoteMqttConnectDeferred() {
    WebHandlersSupport::noteMqttConnectDeferred();
}

void WebHandlersNoteMqttPublishDeferred() {
    WebHandlersSupport::noteMqttPublishDeferred();
}

void WebHandlersPollDeferred() {
    WebHandlersSupport::pollDeferred();
}

void wifi_build_scan_items(int count) {
    with_context([count](WebHandlerContext &context) {
        WebPortalHandlers::buildWifiScanItems(context, count);
    });
}

void wifi_handle_root() {
    with_response_context(WebPortalHandlers::handleWifiRoot);
}

void dashboard_handle_root() {
    with_response_context(WebPortalHandlers::handleDashboardRoot);
}

void dashboard_handle_styles() {
    with_response_context(WebPortalHandlers::handleDashboardStyles);
}

void dashboard_handle_app() {
    with_response_context(WebPortalHandlers::handleDashboardApp);
}

void wifi_handle_save() {
    with_context([](WebHandlerContext &context) {
        WebWifiHandlers::handleSave(
            context,
            WebHandlersSupport::isOtaBusy(),
            WebHandlersSupport::deferredActionDelayMs(),
            WebHandlersSupport::deferredActions(),
            WebHandlersSupport::responseContext());
    });
}

void wifi_handle_not_found() {
    with_context([](WebHandlerContext &context) {
        WebPortalHandlers::handleWifiNotFound(context);
    });
}

void diag_handle_root() {
    with_response_context(WebSystemApiHandlers::handleDiagRoot);
}

void diag_handle_data() {
    with_context([](WebHandlerContext &context) {
        WebSystemApiHandlers::handleDiagData(
            context, WebHandlersSupport::isOtaBusy(), WebHandlersSupport::streamSnapshot(millis()));
    });
}

void mqtt_handle_root() {
    with_response_context([](WebHandlerContext &context,
                             const WebResponseUtils::StreamContext &stream_context) {
        if (!context.server || !context.connectivity_runtime) {
            return;
        }
        WebMqttHandlers::handleRoot(context, stream_context);
    });
}

void mqtt_handle_save() {
    with_context([](WebHandlerContext &context) {
        if (!context.server || !context.web_ui_bridge || !context.connectivity_runtime) {
            return;
        }
        WebMqttHandlers::handleSave(
            context,
            WebHandlersSupport::isOtaBusy(),
            WebHandlersSupport::deferredActionDelayMs(),
            WebHandlersSupport::deferredActions(),
            WebHandlersSupport::responseContext());
    });
}

void theme_handle_root() {
    with_response_context(WebShellAssetHandlers::handleThemeRoot);
}

void theme_handle_styles() {
    with_response_context(WebShellAssetHandlers::handleThemeStyles);
}

void theme_handle_app() {
    with_response_context(WebShellAssetHandlers::handleThemeApp);
}

void theme_handle_state() {
    with_context([](WebHandlerContext &context) {
        WebThemeApiHandlers::handleState(context);
    });
}

void theme_handle_apply() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebThemeApiHandlers::handleApply(context, ota_busy, WebHandlersSupport::otaBusyJson());
    });
}

void dac_handle_root() {
    with_response_context(WebShellAssetHandlers::handleDacRoot);
}

void dac_handle_styles() {
    with_response_context(WebShellAssetHandlers::handleDacStyles);
}

void dac_handle_app() {
    with_response_context(WebShellAssetHandlers::handleDacApp);
}

void dac_handle_state() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebDacApiHandlers::handleState(context, ota_busy, WebHandlersSupport::otaBusyJson());
    });
}

void dac_handle_action() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebDacApiHandlers::handleAction(context, ota_busy, WebHandlersSupport::otaBusyJson());
    });
}

void dac_handle_auto() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebDacApiHandlers::handleAuto(context, ota_busy, WebHandlersSupport::otaBusyJson());
    });
}

void charts_handle_data() {
    with_ota_busy(WebChartsApiHandlers::handleData);
}

void state_handle_data() {
    with_ota_busy(WebSystemApiHandlers::handleStateData);
}

void settings_handle_update() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebSettingsApiHandlers::handleUpdate(
            context,
            ota_busy,
            WebHandlersSupport::webDisplayNameMaxLen(),
            WebHandlersSupport::deferredActionDelayMs(),
            WebHandlersSupport::restartController());
    });
}

void ota_handle_upload() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebOtaHandlers::Runtime runtime = WebHandlersSupport::otaRuntime(context);
        WebOtaHandlers::handleUpload(runtime, ota_busy);
    });
}

void ota_handle_update() {
    with_ota_busy([](WebHandlerContext &context, bool ota_busy) {
        WebOtaHandlers::Runtime runtime = WebHandlersSupport::otaRuntime(context);
        WebOtaHandlers::handleUpdate(runtime, ota_busy);
    });
}

void events_handle_data() {
    with_ota_busy(WebSystemApiHandlers::handleEventsData);
}
