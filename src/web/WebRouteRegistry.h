// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

class WebServerBackend;

namespace WebRouteRegistry {

constexpr size_t kUriRouteCount = 38;
constexpr const char *kOtaPreparePath = "/api/ota/prepare";
constexpr const char *kOtaUploadPath = "/api/ota";

void registerRoutes(WebServerBackend &server);

}  // namespace WebRouteRegistry
