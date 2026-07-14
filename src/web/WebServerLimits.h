// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

namespace WebServerLimits {

constexpr size_t kHttpServerMaxUriHandlers = 48;
constexpr size_t kMaxRequestBodyBytes = 64UL * 1024UL;
constexpr size_t kMaxMultipartBoundaryBytes = 128;
constexpr size_t kMaxMultipartHeaderLineBytes = 1024;
constexpr size_t kMaxMultipartFieldBytes = 4096;
constexpr size_t kMaxMultipartParts = 16;

constexpr bool requestBodySizeAllowed(size_t size) {
    return size <= kMaxRequestBodyBytes;
}

constexpr bool multipartBoundarySizeAllowed(size_t size) {
    return size > 0 && size <= kMaxMultipartBoundaryBytes;
}

constexpr bool multipartHeaderLineSizeAllowed(size_t size) {
    return size <= kMaxMultipartHeaderLineBytes;
}

constexpr bool multipartFieldAppendAllowed(size_t current_size, size_t append_size) {
    return current_size <= kMaxMultipartFieldBytes &&
           append_size <= (kMaxMultipartFieldBytes - current_size);
}

}  // namespace WebServerLimits
