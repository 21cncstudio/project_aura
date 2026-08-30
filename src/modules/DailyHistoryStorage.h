// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

enum class DailyStorageFailureKind : uint8_t {
    None = 0,
    Busy,
    Missing,
    Io,
    NoSpace,
    ReadOnly,
    Invalid,
    Unknown,
};

class DailyHistoryStorage {
public:
    virtual ~DailyHistoryStorage() = default;

    virtual bool isReady() const = 0;
    virtual bool fileInfo(const char *path, bool &exists, size_t &out_size) const = 0;
    virtual bool fileExists(const char *path) const = 0;
    virtual bool fileSize(const char *path, size_t &out_size) const = 0;
    virtual bool appendText(const char *path, const char *text) = 0;
    virtual bool appendUniqueTextBlockAtomic(const char *path,
                                             const char *unique_line_prefix,
                                             const char *header,
                                             const char *block) = 0;
    virtual bool removeDailyCsvRowsOnOrAfterAtomic(const char *path,
                                                    const char *cutoff_iso_day) = 0;
    virtual bool readBinary(const char *path, void *out, size_t len, size_t &out_len) const = 0;
    virtual bool writeBinaryAtomic(const char *path, const void *data, size_t len) = 0;
    virtual bool removeFile(const char *path) = 0;
    virtual DailyStorageFailureKind lastFailureKind() const = 0;
};
