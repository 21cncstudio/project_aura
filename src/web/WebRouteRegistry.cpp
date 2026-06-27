// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "web/WebRouteRegistry.h"

#include "web/WebHandlers.h"
#include "web/WebTemplates.h"
#include "web/WebTransport.h"

namespace WebRouteRegistry {

void registerRoutes(WebServerBackend &server) {
    server.onGet("/", dashboard_handle_root);
    server.onGet("/dashboard", dashboard_handle_root);
    server.onGet(WebTemplates::kDashboardStylesCssPath, dashboard_handle_styles);
    server.onGet(WebTemplates::kDashboardAppJsPath, dashboard_handle_app);
    server.onGet("/wifi", wifi_handle_root);
    server.onGet("/diag", diag_handle_root);
    server.onGet("/debug/sfa40", sfa40_debug_handle_root);
    server.onPost("/save", wifi_handle_save);
    server.onGet("/mqtt", mqtt_handle_root);
    server.onPost("/mqtt", mqtt_handle_save);
    server.onGet("/theme", theme_handle_root);
    server.onGet(WebTemplates::kThemeStylesCssPath, theme_handle_styles);
    server.onGet(WebTemplates::kThemeAppJsPath, theme_handle_app);
    server.onGet("/theme/state", theme_handle_state);
    server.onPost("/theme/apply", theme_handle_apply);
    server.onGet("/dac", dac_handle_root);
    server.onGet(WebTemplates::kDacStylesCssPath, dac_handle_styles);
    server.onGet(WebTemplates::kDacAppJsPath, dac_handle_app);
    server.onGet("/dac/state", dac_handle_state);
    server.onPost("/dac/action", dac_handle_action);
    server.onPost("/dac/auto", dac_handle_auto);
    server.onGet("/thresholds", thresholds_handle_root);
    server.onGet("/api/thresholds", thresholds_handle_state);
    server.onPost("/api/thresholds", thresholds_handle_update);
    server.onPost("/api/thresholds/reset", thresholds_handle_reset);
    server.onGet("/api/charts", charts_handle_data);
    server.onGet("/api/history/daily/status", daily_history_status_handle);
    server.onGet("/api/history/daily.csv", daily_history_csv_handle);
    server.onGet("/api/state", state_handle_data);
    server.onGet("/api/events", events_handle_data);
    server.onGet("/api/diag", diag_handle_data);
    server.onGet("/api/debug/sfa40", sfa40_debug_handle_data);
    server.onPost("/api/settings", settings_handle_update);
    server.onPost(kOtaPreparePath, ota_handle_prepare);
    server.onPostUpload(kOtaUploadPath, ota_handle_update, ota_handle_upload);
    server.onNotFound(wifi_handle_not_found);
}

}  // namespace WebRouteRegistry
