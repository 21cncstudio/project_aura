// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/SdCardManager.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/Logger.h"

#ifndef UNIT_TEST
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_display_panel.hpp>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#endif

namespace {

#ifndef UNIT_TEST
constexpr spi_host_device_t kSdSpiHost = SPI2_HOST;
#endif

constexpr size_t kCopyBufferSize = 1024;

bool path_has_parent_char(char ch) {
    return ch == '/' || ch == '\\';
}

int current_errno_or_io() {
    return errno != 0 ? errno : EIO;
}

void copy_text(char *out, size_t out_len, const char *value) {
    if (!out || out_len == 0) {
        return;
    }
    strncpy(out, value ? value : "", out_len - 1);
    out[out_len - 1] = '\0';
}

} // namespace

SdCardManager::SdCardManager() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutex();
    status_mutex_ = xSemaphoreCreateMutex();
#endif
}

SdCardManager::~SdCardManager() {
    (void)end();
#ifndef UNIT_TEST
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
    if (status_mutex_) {
        vSemaphoreDelete(status_mutex_);
        status_mutex_ = nullptr;
    }
#endif
}

void SdCardManager::setState(SdCardPolicy::State state, const char *error, int32_t error_code) {
    state_ = state;
    last_error_ = error ? error : "";
    last_error_code_ = error_code;
}

DailyStorageFailureKind SdCardManager::classifyErrno(int error_code) {
    switch (error_code) {
        case 0: return DailyStorageFailureKind::None;
        case EBUSY:
        case EAGAIN: return DailyStorageFailureKind::Busy;
        case ENOENT: return DailyStorageFailureKind::Missing;
        case ENOSPC: return DailyStorageFailureKind::NoSpace;
        case EROFS:
        case EACCES:
        case EPERM: return DailyStorageFailureKind::ReadOnly;
        case EINVAL:
        case ENAMETOOLONG: return DailyStorageFailureKind::Invalid;
        case EIO:
#ifdef ENODEV
        case ENODEV:
#endif
#ifdef ENXIO
        case ENXIO:
#endif
            return DailyStorageFailureKind::Io;
        default: return DailyStorageFailureKind::Unknown;
    }
}

void SdCardManager::recordFailure(const char *operation,
                                  const char *stage,
                                  int error_code,
                                  DailyStorageFailureKind kind,
                                  bool write_fault) const {
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
#endif
    const bool preserve_active_write_fault = write_fault_ && !write_fault;
    if (!preserve_active_write_fault) {
        last_failure_kind_ = kind;
        runtime_error_code_ = error_code;
        copy_text(last_operation_, sizeof(last_operation_), operation);
        copy_text(last_stage_, sizeof(last_stage_), stage);
        copy_text(runtime_error_, sizeof(runtime_error_),
                  error_code ? strerror(error_code) : "operation failed");
    }
    last_failure_ms_ = millis();
    ++total_failures_;
    if (kind == DailyStorageFailureKind::Busy) {
        ++busy_count_;
    } else if (kind != DailyStorageFailureKind::Missing) {
        runtime_healthy_ = false;
        ++consecutive_failures_;
        if (write_fault) {
            write_fault_ = true;
        }
    }
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
}

void SdCardManager::recordSuccess(const char *operation, bool write_success) const {
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
#endif
    if (write_success) {
        last_write_success_ms_ = millis();
        const bool clears_active_fault =
            !write_fault_ || strcmp(last_operation_, operation ? operation : "") == 0;
        if (clears_active_fault) {
            copy_text(last_operation_, sizeof(last_operation_), operation);
            write_fault_ = false;
            runtime_healthy_ = true;
            last_failure_kind_ = DailyStorageFailureKind::None;
            runtime_error_code_ = 0;
            last_stage_[0] = '\0';
            runtime_error_[0] = '\0';
            consecutive_failures_ = 0;
        }
    } else if (!write_fault_) {
        const bool same_operation = strcmp(last_operation_, operation ? operation : "") == 0;
        const bool transient_failure = last_failure_kind_ == DailyStorageFailureKind::Busy ||
                                       last_failure_kind_ == DailyStorageFailureKind::Missing;
        if (same_operation || transient_failure) {
            copy_text(last_operation_, sizeof(last_operation_), operation);
            runtime_healthy_ = true;
            last_failure_kind_ = DailyStorageFailureKind::None;
            runtime_error_code_ = 0;
            last_stage_[0] = '\0';
            runtime_error_[0] = '\0';
            consecutive_failures_ = 0;
        }
    }
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
}

DailyStorageFailureKind SdCardManager::lastFailureKind() const {
    DailyStorageFailureKind result = DailyStorageFailureKind::Unknown;
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
#endif
    result = last_failure_kind_;
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
    return result;
}

bool SdCardManager::lock(uint32_t timeout_ms) const {
#ifdef UNIT_TEST
    (void)timeout_ms;
    return true;
#else
    if (!mutex_) {
        return true;
    }
    return xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
#endif
}

void SdCardManager::unlock() const {
#ifndef UNIT_TEST
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
#endif
}

bool SdCardManager::acquireFileAccess(uint32_t timeout_ms) const {
    if (lock(timeout_ms)) {
        return true;
    }
    recordFailure("file_access", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
    return false;
}

void SdCardManager::releaseFileAccess() const {
    unlock();
}

void SdCardManager::noteStreamReadFailure(const char *stage, int error_code) const {
    const int resolved_error = error_code != 0 ? error_code : EIO;
    recordFailure("csv_stream", stage, resolved_error, classifyErrno(resolved_error), false);
}

void SdCardManager::noteStreamReadSuccess() const {
    recordSuccess("csv_stream", false);
}

bool SdCardManager::setCardSelect(bool selected) {
#ifdef UNIT_TEST
    (void)selected;
    return false;
#else
    if (!board_ || !board_->getIO_Expander()) {
        return false;
    }
    auto *expander = board_->getIO_Expander()->getBase();
    if (!expander) {
        return false;
    }
    return expander->digitalWrite(kSdCsExio, selected ? 0 : 1);
#endif
}

bool SdCardManager::begin(esp_panel::board::Board *board) {
#ifdef UNIT_TEST
    (void)board;
    attempted_ = true;
    mounted_ = false;
    setState(SdCardPolicy::State::Fault, "unit test");
    return false;
#else
    attempted_ = true;
    if (mounted_) {
        return true;
    }
    board_ = board;
    spi_bus_owned_ = false;
    if (!board_) {
        setState(SdCardPolicy::State::BoardUnavailable, "board unavailable");
        LOGI("SD", "optional storage unavailable: board not initialized");
        return false;
    }
    if (!setCardSelect(false)) {
        setState(SdCardPolicy::State::Fault, "CS expander unavailable");
        LOGW("SD", "skip SD mount: failed to deassert CS through IO expander");
        return false;
    }

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = kSdMosiPin;
    bus_cfg.miso_io_num = kSdMisoPin;
    bus_cfg.sclk_io_num = kSdSckPin;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = kSdSpiHost;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    esp_err_t err = spi_bus_initialize(kSdSpiHost, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        setState(SdCardPolicy::State::Fault, "SPI bus init failed", err);
        LOGW("SD", "SPI bus init failed: %s", esp_err_to_name(err));
        setCardSelect(false);
        return false;
    }
    spi_bus_owned_ = err == ESP_OK;

    // The Waveshare TF slot CS is on CH422G EXIO4, not an ESP GPIO.
    // FatFS/SDSPI is mounted without GPIO CS and EXIO4 is held active while mounted.
    if (!setCardSelect(true)) {
        if (spi_bus_owned_) {
            spi_bus_free(kSdSpiHost);
            spi_bus_owned_ = false;
        }
        setState(SdCardPolicy::State::Fault, "CS expander write failed");
        LOGW("SD", "skip SD mount: failed to assert CS through IO expander");
        return false;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SDSPI_SLOT_NO_CS;
    slot_config.host_id = kSdSpiHost;

    esp_vfs_fat_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 4;
    mount_config.allocation_unit_size = 16 * 1024;
    mount_config.disk_status_check_enable = true;
    mount_config.use_one_fat = false;

    const esp_log_level_t previous_mount_log_level = esp_log_level_get("vfs_fat_sdmmc");
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_NONE);
    err = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    esp_log_level_set("vfs_fat_sdmmc", previous_mount_log_level);
    if (err != ESP_OK) {
        mounted_ = false;
        card_ = nullptr;
        const bool cs_released = setCardSelect(false);
        if (spi_bus_owned_) {
            spi_bus_free(kSdSpiHost);
            spi_bus_owned_ = false;
        }
        if (!cs_released) {
            setState(SdCardPolicy::State::Fault, "CS release failed", err);
            LOGW("SD", "mount failed and CS release failed: %s", esp_err_to_name(err));
            return false;
        }

        const SdCardPolicy::MountOutcome outcome =
            (err == ESP_ERR_TIMEOUT) ? SdCardPolicy::MountOutcome::NoResponse
                                     : SdCardPolicy::MountOutcome::Error;
        const SdCardPolicy::State state = SdCardPolicy::stateForMountOutcome(outcome);
        if (SdCardPolicy::isFault(state)) {
            setState(state, "SD mount failed", err);
            LOGW("SD", "mount failed: %s", esp_err_to_name(err));
        } else {
            setState(state);
            LOGI("SD", "optional card not detected; continuing without SD storage");
        }
        return false;
    }

    mounted_ = true;
    setState(SdCardPolicy::State::Mounted);
    updateSpaceInfoLocked();
    recordSuccess("mount", false);
    LOGI("SD", "mounted card size=%llu MB free=%llu MB",
         static_cast<unsigned long long>(status().card_size_bytes / (1024ULL * 1024ULL)),
         static_cast<unsigned long long>(filesystem_free_bytes_ / (1024ULL * 1024ULL)));
    return true;
#endif
}

bool SdCardManager::end() {
#ifdef UNIT_TEST
    return true;
#else
    if (!mounted_) {
        return true;
    }
    if (!lock(5000)) {
        recordFailure("unmount", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    const esp_err_t unmount_err = esp_vfs_fat_sdcard_unmount(kMountPoint, card_);
    if (spi_bus_owned_) {
        spi_bus_free(kSdSpiHost);
    }
    const bool cs_released = setCardSelect(false);
    mounted_ = false;
    card_ = nullptr;
    spi_bus_owned_ = false;
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
#endif
    filesystem_total_bytes_ = 0;
    filesystem_free_bytes_ = 0;
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
    unlock();
    if (unmount_err != ESP_OK) {
        LOGW("SD", "unmount failed: %s", esp_err_to_name(unmount_err));
        return false;
    }
    if (!cs_released) {
        LOGW("SD", "unmounted but failed to release card select");
        return false;
    }
    recordSuccess("unmount", false);
    return true;
#endif
}

SdCardManager::Status SdCardManager::status() const {
    Status result{};
    result.state = state_;
    result.mounted = mounted_;
    result.attempted = attempted_;
    result.mount_point = kMountPoint;
    result.last_error = last_error_;
    result.last_error_code = last_error_code_;
#ifndef UNIT_TEST
    if (card_) {
        result.card_size_bytes =
            static_cast<uint64_t>(card_->csd.capacity) * static_cast<uint64_t>(card_->csd.sector_size);
    }
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
#endif
    result.runtime_healthy = runtime_healthy_;
    result.write_fault = write_fault_;
    result.runtime_failure_kind = last_failure_kind_;
    result.runtime_error_code = runtime_error_code_;
    copy_text(result.last_operation, sizeof(result.last_operation), last_operation_);
    copy_text(result.last_stage, sizeof(result.last_stage), last_stage_);
    copy_text(result.runtime_error, sizeof(result.runtime_error), runtime_error_);
    result.last_failure_ms = last_failure_ms_;
    result.last_write_success_ms = last_write_success_ms_;
    result.consecutive_failures = consecutive_failures_;
    result.total_failures = total_failures_;
    result.busy_count = busy_count_;
    result.filesystem_total_bytes = filesystem_total_bytes_;
    result.filesystem_free_bytes = filesystem_free_bytes_;
#ifndef UNIT_TEST
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
    return result;
}

String SdCardManager::fullPath(const char *relative_path) const {
    String full = kMountPoint;
    if (!relative_path || relative_path[0] == '\0') {
        return full;
    }
    if (relative_path[0] != '/') {
        full += '/';
    }
    full += relative_path;
    return full;
}

bool SdCardManager::ensureParentDirsLocked(const char *relative_path, const char *operation) const {
    String path = fullPath(relative_path);
    const int start = strlen(kMountPoint);
    for (int i = start + 1; i < static_cast<int>(path.length()); ++i) {
        if (!path_has_parent_char(path[i])) {
            continue;
        }
        String dir = path.substring(0, i);
        if (dir.length() <= strlen(kMountPoint)) {
            continue;
        }
        errno = 0;
        if (mkdir(dir.c_str(), 0775) != 0 && errno != EEXIST) {
            const int error_code = current_errno_or_io();
            recordFailure(operation, "mkdir", error_code, classifyErrno(error_code), true);
            return false;
        }
    }
    return true;
}

bool SdCardManager::recoverAtomicBackupLocked(const String &final_path, const char *operation) const {
    const String backup_path = final_path + ".bak";
    struct stat final_stat = {};
    struct stat backup_stat = {};
    errno = 0;
    const bool final_exists = stat(final_path.c_str(), &final_stat) == 0;
    if (!final_exists && errno != ENOENT) {
        const int error_code = current_errno_or_io();
        recordFailure(operation, "stat_final", error_code, classifyErrno(error_code), false);
        return false;
    }
    errno = 0;
    const bool backup_exists = stat(backup_path.c_str(), &backup_stat) == 0;
    if (!backup_exists && errno != ENOENT) {
        const int error_code = current_errno_or_io();
        recordFailure(operation, "stat_backup", error_code, classifyErrno(error_code), false);
        return false;
    }
    if (!final_exists && backup_exists) {
        errno = 0;
        if (rename(backup_path.c_str(), final_path.c_str()) != 0) {
            const int error_code = current_errno_or_io();
            recordFailure(operation, "restore_backup", error_code, classifyErrno(error_code), true);
            return false;
        }
        LOGW("SD", "restored interrupted atomic file: %s", final_path.c_str());
    }
    return true;
}

bool SdCardManager::replaceWithTempLocked(const String &final_path,
                                          const String &tmp_path,
                                          const char *operation) const {
    const String backup_path = final_path + ".bak";
    if (!recoverAtomicBackupLocked(final_path, operation)) {
        return false;
    }

    struct stat st = {};
    errno = 0;
    const bool final_exists = stat(final_path.c_str(), &st) == 0;
    if (!final_exists && errno != ENOENT) {
        const int error_code = current_errno_or_io();
        recordFailure(operation, "stat_replace", error_code, classifyErrno(error_code), true);
        return false;
    }

    errno = 0;
    if (remove(backup_path.c_str()) != 0 && errno != ENOENT) {
        const int error_code = current_errno_or_io();
        recordFailure(operation, "remove_stale_backup", error_code, classifyErrno(error_code), true);
        return false;
    }

    bool moved_final = false;
    if (final_exists) {
        errno = 0;
        if (rename(final_path.c_str(), backup_path.c_str()) != 0) {
            const int error_code = current_errno_or_io();
            recordFailure(operation, "backup_current", error_code, classifyErrno(error_code), true);
            return false;
        }
        moved_final = true;
    }

    errno = 0;
    if (rename(tmp_path.c_str(), final_path.c_str()) != 0) {
        const int error_code = current_errno_or_io();
        if (moved_final && rename(backup_path.c_str(), final_path.c_str()) != 0) {
            LOGE("SD", "failed to restore backup after atomic replace failure: %s", final_path.c_str());
        }
        recordFailure(operation, "commit_rename", error_code, classifyErrno(error_code), true);
        return false;
    }

    errno = 0;
    if (moved_final && remove(backup_path.c_str()) != 0 && errno != ENOENT) {
        LOGW("SD", "committed %s but stale backup remains errno=%d", final_path.c_str(), errno);
    }
    return true;
}

void SdCardManager::updateSpaceInfoLocked() {
#ifndef UNIT_TEST
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    if (!mounted_) {
        total_bytes = 0;
        free_bytes = 0;
    } else if (esp_vfs_fat_info(kMountPoint, &total_bytes, &free_bytes) != ESP_OK) {
        return;
    }
    if (status_mutex_) {
        xSemaphoreTake(status_mutex_, portMAX_DELAY);
    }
    filesystem_total_bytes_ = total_bytes;
    filesystem_free_bytes_ = free_bytes;
    if (status_mutex_) {
        xSemaphoreGive(status_mutex_);
    }
#endif
}

bool SdCardManager::fileInfoUnlocked(const char *path, bool &exists, size_t &out_size) const {
    exists = false;
    out_size = 0;
    if (!mounted_ || !path) {
        recordFailure("stat", "validate", EINVAL, DailyStorageFailureKind::Invalid, false);
        return false;
    }
    const String full = fullPath(path);
    if (!recoverAtomicBackupLocked(full, "stat")) {
        return false;
    }
    struct stat st = {};
    errno = 0;
    if (stat(full.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        const int error_code = current_errno_or_io();
        recordFailure("stat", "stat", error_code, classifyErrno(error_code), false);
        return false;
    }
    if (S_ISREG(st.st_mode)) {
        exists = true;
        out_size = static_cast<size_t>(st.st_size);
    }
    recordSuccess("stat", false);
    return true;
}

bool SdCardManager::fileInfo(const char *path, bool &exists, size_t &out_size) const {
    if (!lock()) {
        recordFailure("stat", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        exists = false;
        out_size = 0;
        return false;
    }
    const bool ok = fileInfoUnlocked(path, exists, out_size);
    unlock();
    return ok;
}

bool SdCardManager::fileInfoLocked(const char *path, bool &exists, size_t &out_size) const {
    return fileInfoUnlocked(path, exists, out_size);
}

bool SdCardManager::fileExists(const char *path) const {
    bool exists = false;
    size_t size = 0;
    return fileInfo(path, exists, size) && exists;
}

bool SdCardManager::fileSize(const char *path, size_t &out_size) const {
    bool exists = false;
    return fileInfo(path, exists, out_size) && exists;
}

bool SdCardManager::appendText(const char *path, const char *text) {
    if (!mounted_ || !path || !text) {
        recordFailure("append", "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure("append", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    if (!ensureParentDirsLocked(path, "append")) {
        unlock();
        return false;
    }
    const String full = fullPath(path);
    errno = 0;
    FILE *file = fopen(full.c_str(), "ab");
    if (!file) {
        const int error_code = current_errno_or_io();
        recordFailure("append", "open", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }

    const size_t len = strlen(text);
    errno = 0;
    bool ok = len == 0 || fwrite(text, 1, len, file) == len;
    int error_code = ok ? 0 : current_errno_or_io();
    const char *stage = ok ? "" : "write";
    if (ok && fflush(file) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "flush";
    }
    if (ok && fsync(fileno(file)) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "sync";
    }
    if (fclose(file) != 0 && ok) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "close";
    }
    if (ok) {
        updateSpaceInfoLocked();
        recordSuccess("append", true);
    } else {
        recordFailure("append", stage, error_code, classifyErrno(error_code), true);
    }
    unlock();
    return ok;
}

bool SdCardManager::appendUniqueTextBlockAtomic(const char *path,
                                                const char *unique_line_prefix,
                                                const char *header,
                                                const char *block) {
    if (!mounted_ || !path || !unique_line_prefix || unique_line_prefix[0] == '\0' || !block) {
        recordFailure("daily_csv", "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure("daily_csv", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    if (!ensureParentDirsLocked(path, "daily_csv")) {
        unlock();
        return false;
    }

    const String final_path = fullPath(path);
    const String tmp_path = final_path + ".tmp";
    if (!recoverAtomicBackupLocked(final_path, "daily_csv")) {
        unlock();
        return false;
    }

    errno = 0;
    FILE *source = fopen(final_path.c_str(), "rb");
    if (!source && errno != ENOENT) {
        const int error_code = current_errno_or_io();
        recordFailure("daily_csv", "open_source", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }

    errno = 0;
    FILE *target = fopen(tmp_path.c_str(), "wb");
    if (!target) {
        const int error_code = current_errno_or_io();
        if (source) {
            fclose(source);
        }
        recordFailure("daily_csv", "open_temp", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }

    bool ok = true;
    int error_code = 0;
    const char *stage = "";
    size_t copied = 0;
    char last_char = '\0';
    char line[kCopyBufferSize];
    if (source) {
        const size_t prefix_len = strlen(unique_line_prefix);
        while (fgets(line, sizeof(line), source)) {
            const size_t line_len = strlen(line);
            if (strncmp(line, unique_line_prefix, prefix_len) == 0) {
                continue;
            }
            if (line_len > 0 && fwrite(line, 1, line_len, target) != line_len) {
                ok = false;
                error_code = current_errno_or_io();
                stage = "copy_write";
                break;
            }
            copied += line_len;
            if (line_len > 0) {
                last_char = line[line_len - 1];
            }
        }
        if (ok && ferror(source)) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "copy_read";
        }
        if (fclose(source) != 0 && ok) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "close_source";
        }
    }

    if (ok && copied == 0 && header && header[0] != '\0') {
        const size_t header_len = strlen(header);
        if (fwrite(header, 1, header_len, target) != header_len) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "write_header";
        } else if (header_len > 0) {
            last_char = header[header_len - 1];
        }
    }
    if (ok && copied > 0 && last_char != '\n') {
        if (fwrite("\n", 1, 1, target) != 1) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "write_separator";
        }
    }
    if (ok) {
        const size_t block_len = strlen(block);
        if (block_len > 0 && fwrite(block, 1, block_len, target) != block_len) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "write_block";
        }
    }
    if (ok && fflush(target) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "flush_temp";
    }
    if (ok && fsync(fileno(target)) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "sync_temp";
    }
    if (fclose(target) != 0 && ok) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "close_temp";
    }

    if (ok) {
        ok = replaceWithTempLocked(final_path, tmp_path, "daily_csv");
    }
    if (!ok) {
        const int replace_error = error_code != 0 ? error_code : current_errno_or_io();
        errno = 0;
        remove(tmp_path.c_str());
        if (error_code != 0) {
            recordFailure("daily_csv", stage, replace_error, classifyErrno(replace_error), true);
        }
        unlock();
        return false;
    }

    updateSpaceInfoLocked();
    recordSuccess("daily_csv", true);
    unlock();
    return true;
}

bool SdCardManager::readBinary(const char *path, void *out, size_t len, size_t &out_len) const {
    out_len = 0;
    if (!mounted_ || !path || !out) {
        recordFailure("state_read", "validate", EINVAL, DailyStorageFailureKind::Invalid, false);
        return false;
    }
    if (!lock()) {
        recordFailure("state_read", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    const String full = fullPath(path);
    if (!recoverAtomicBackupLocked(full, "state_read")) {
        unlock();
        return false;
    }
    errno = 0;
    FILE *file = fopen(full.c_str(), "rb");
    if (!file) {
        const int error_code = current_errno_or_io();
        recordFailure("state_read", "open", error_code, classifyErrno(error_code), false);
        unlock();
        return false;
    }
    out_len = fread(out, 1, len, file);
    bool ok = ferror(file) == 0;
    int error_code = ok ? 0 : current_errno_or_io();
    const char *stage = ok ? "" : "read";
    if (fclose(file) != 0 && ok) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "close";
    }
    if (ok) {
        recordSuccess("state_read", false);
    } else {
        recordFailure("state_read", stage, error_code, classifyErrno(error_code), false);
    }
    unlock();
    return ok;
}

bool SdCardManager::writeBinaryAtomic(const char *path, const void *data, size_t len) {
    if (!mounted_ || !path || !data) {
        recordFailure("state_write", "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure("state_write", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    if (!ensureParentDirsLocked(path, "state_write")) {
        unlock();
        return false;
    }
    const String final_path = fullPath(path);
    const String tmp_path = final_path + ".tmp";
    if (!recoverAtomicBackupLocked(final_path, "state_write")) {
        unlock();
        return false;
    }

    errno = 0;
    FILE *file = fopen(tmp_path.c_str(), "wb");
    if (!file) {
        const int error_code = current_errno_or_io();
        recordFailure("state_write", "open_temp", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }
    errno = 0;
    bool ok = len == 0 || fwrite(data, 1, len, file) == len;
    int error_code = ok ? 0 : current_errno_or_io();
    const char *stage = ok ? "" : "write_temp";
    if (ok && fflush(file) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "flush_temp";
    }
    if (ok && fsync(fileno(file)) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "sync_temp";
    }
    if (fclose(file) != 0 && ok) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "close_temp";
    }
    if (ok) {
        ok = replaceWithTempLocked(final_path, tmp_path, "state_write");
    }
    if (!ok) {
        errno = 0;
        remove(tmp_path.c_str());
        if (error_code != 0) {
            recordFailure("state_write", stage, error_code, classifyErrno(error_code), true);
        }
        unlock();
        return false;
    }
    updateSpaceInfoLocked();
    recordSuccess("state_write", true);
    unlock();
    return true;
}

bool SdCardManager::removeFile(const char *path) {
    if (!mounted_ || !path) {
        recordFailure("remove", "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure("remove", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    const String full = fullPath(path);
    errno = 0;
    const bool ok = remove(full.c_str()) == 0;
    const int error_code = ok ? 0 : current_errno_or_io();
    if (ok) {
        updateSpaceInfoLocked();
        recordSuccess("remove", true);
    } else {
        recordFailure("remove", "remove", error_code, classifyErrno(error_code), true);
    }
    unlock();
    return ok;
}

FILE *SdCardManager::openReadUnlocked(const char *relative_path) const {
    if (!mounted_ || !relative_path) {
        recordFailure("open_read", "validate", EINVAL, DailyStorageFailureKind::Invalid, false);
        return nullptr;
    }
    const String full = fullPath(relative_path);
    if (!recoverAtomicBackupLocked(full, "open_read")) {
        return nullptr;
    }
    errno = 0;
    FILE *file = fopen(full.c_str(), "rb");
    if (!file) {
        const int error_code = current_errno_or_io();
        recordFailure("open_read", "open", error_code, classifyErrno(error_code), false);
        return nullptr;
    }
    recordSuccess("open_read", false);
    return file;
}

FILE *SdCardManager::openRead(const char *relative_path) const {
    if (!lock()) {
        recordFailure("open_read", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return nullptr;
    }
    FILE *file = openReadUnlocked(relative_path);
    unlock();
    return file;
}

FILE *SdCardManager::openReadLocked(const char *relative_path) const {
    return openReadUnlocked(relative_path);
}
