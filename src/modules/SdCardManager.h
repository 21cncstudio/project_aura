// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "modules/DailyHistoryStorage.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sdmmc_cmd.h>
#endif

namespace esp_panel {
namespace board {
class Board;
} // namespace board
} // namespace esp_panel

class SdCardManager final : public DailyHistoryStorage {
public:
    struct Status {
        bool mounted = false;
        bool attempted = false;
        uint64_t card_size_bytes = 0;
        const char *mount_point = "/sdcard";
        const char *last_error = "";
    };

    SdCardManager();
    ~SdCardManager();

    bool begin(esp_panel::board::Board *board);
    void end();
    Status status() const;
    String fullPath(const char *relative_path) const;
    FILE *openRead(const char *relative_path) const;
    bool fileInfo(const char *path, bool &exists, size_t &out_size) const override;
    bool acquireFileAccess(uint32_t timeout_ms = 1000) const;
    void releaseFileAccess() const;

    bool isReady() const override { return mounted_; }
    bool fileExists(const char *path) const override;
    bool fileSize(const char *path, size_t &out_size) const override;
    bool appendText(const char *path, const char *text) override;
    bool readBinary(const char *path, void *out, size_t len, size_t &out_len) const override;
    bool writeBinaryAtomic(const char *path, const void *data, size_t len) override;
    bool removeFile(const char *path) override;

private:
    bool setCardSelect(bool selected);
    bool ensureParentDirs(const char *relative_path) const;
    bool lock(uint32_t timeout_ms = 1000) const;
    void unlock() const;
    void setError(const char *error);

    static constexpr const char *kMountPoint = "/sdcard";
    static constexpr uint8_t kSdMosiPin = 11;
    static constexpr uint8_t kSdSckPin = 12;
    static constexpr uint8_t kSdMisoPin = 13;
    static constexpr uint8_t kSdCsExio = 4;

    bool mounted_ = false;
    bool attempted_ = false;
    const char *last_error_ = "";

#ifndef UNIT_TEST
    esp_panel::board::Board *board_ = nullptr;
    sdmmc_card_t *card_ = nullptr;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    bool spi_bus_owned_ = false;
#endif
};
