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
#include "modules/SdCardPolicy.h"

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
        SdCardPolicy::State state = SdCardPolicy::State::NotAttempted;
        bool mounted = false;
        bool attempted = false;
        uint64_t card_size_bytes = 0;
        const char *mount_point = "/sdcard";
        const char *last_error = "";
        int32_t last_error_code = 0;
        bool runtime_healthy = true;
        bool write_fault = false;
        DailyStorageFailureKind runtime_failure_kind = DailyStorageFailureKind::None;
        int32_t runtime_error_code = 0;
        char last_operation[32] = {};
        char last_stage[32] = {};
        char runtime_error[96] = {};
        uint32_t last_failure_ms = 0;
        uint32_t last_write_success_ms = 0;
        uint32_t consecutive_failures = 0;
        uint32_t total_failures = 0;
        uint32_t busy_count = 0;
        uint64_t filesystem_total_bytes = 0;
        uint64_t filesystem_free_bytes = 0;
    };

    SdCardManager();
    ~SdCardManager();

    bool begin(esp_panel::board::Board *board);
    bool end();
    Status status() const;
    String fullPath(const char *relative_path) const;
    FILE *openRead(const char *relative_path) const;
    FILE *openReadLocked(const char *relative_path) const;
    bool fileInfo(const char *path, bool &exists, size_t &out_size) const override;
    bool fileInfoLocked(const char *path, bool &exists, size_t &out_size) const;
    bool acquireFileAccess(uint32_t timeout_ms = 1000) const;
    void releaseFileAccess() const;
    void noteStreamReadFailure(const char *stage, int error_code) const;
    void noteStreamReadSuccess() const;

    bool isReady() const override { return mounted_; }
    bool fileExists(const char *path) const override;
    bool fileSize(const char *path, size_t &out_size) const override;
    bool appendText(const char *path, const char *text) override;
    bool appendUniqueTextBlockAtomic(const char *path,
                                     const char *unique_line_prefix,
                                     const char *header,
                                     const char *block) override;
    bool readBinary(const char *path, void *out, size_t len, size_t &out_len) const override;
    bool writeBinaryAtomic(const char *path, const void *data, size_t len) override;
    bool removeFile(const char *path) override;
    DailyStorageFailureKind lastFailureKind() const override;

private:
    bool setCardSelect(bool selected);
    bool ensureParentDirsLocked(const char *relative_path, const char *operation) const;
    bool fileInfoUnlocked(const char *path, bool &exists, size_t &out_size) const;
    FILE *openReadUnlocked(const char *relative_path) const;
    bool recoverAtomicBackupLocked(const String &final_path, const char *operation) const;
    bool replaceWithTempLocked(const String &final_path,
                               const String &tmp_path,
                               const char *operation) const;
    void updateSpaceInfoLocked();
    void recordFailure(const char *operation,
                       const char *stage,
                       int error_code,
                       DailyStorageFailureKind kind,
                       bool write_fault) const;
    void recordSuccess(const char *operation, bool write_success) const;
    static DailyStorageFailureKind classifyErrno(int error_code);
    bool lock(uint32_t timeout_ms = 1000) const;
    void unlock() const;
    void setState(SdCardPolicy::State state, const char *error = "", int32_t error_code = 0);

    static constexpr const char *kMountPoint = "/sdcard";
    static constexpr uint8_t kSdMosiPin = 11;
    static constexpr uint8_t kSdSckPin = 12;
    static constexpr uint8_t kSdMisoPin = 13;
    static constexpr uint8_t kSdCsExio = 4;

    bool mounted_ = false;
    bool attempted_ = false;
    SdCardPolicy::State state_ = SdCardPolicy::State::NotAttempted;
    const char *last_error_ = "";
    int32_t last_error_code_ = 0;
    mutable bool runtime_healthy_ = true;
    mutable bool write_fault_ = false;
    mutable DailyStorageFailureKind last_failure_kind_ = DailyStorageFailureKind::None;
    mutable int32_t runtime_error_code_ = 0;
    mutable char last_operation_[32] = {};
    mutable char last_stage_[32] = {};
    mutable char runtime_error_[96] = {};
    mutable uint32_t last_failure_ms_ = 0;
    mutable uint32_t last_write_success_ms_ = 0;
    mutable uint32_t consecutive_failures_ = 0;
    mutable uint32_t total_failures_ = 0;
    mutable uint32_t busy_count_ = 0;
    uint64_t filesystem_total_bytes_ = 0;
    uint64_t filesystem_free_bytes_ = 0;

#ifndef UNIT_TEST
    esp_panel::board::Board *board_ = nullptr;
    sdmmc_card_t *card_ = nullptr;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    mutable SemaphoreHandle_t status_mutex_ = nullptr;
    bool spi_bus_owned_ = false;
#endif
};
