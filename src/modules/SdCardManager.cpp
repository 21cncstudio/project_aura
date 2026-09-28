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
#ifdef UNIT_TEST
#include <chrono>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#endif

#include "core/Logger.h"
#include "core/WakePowerGuard.h"

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

int sync_file(FILE *file) {
#if defined(UNIT_TEST) && defined(_WIN32)
    return _commit(_fileno(file));
#else
    return fsync(fileno(file));
#endif
}

bool is_decimal_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool is_leap_year(unsigned year) {
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

bool is_valid_iso_day(const char *text) {
    if (!text ||
        !is_decimal_digit(text[0]) || !is_decimal_digit(text[1]) ||
        !is_decimal_digit(text[2]) || !is_decimal_digit(text[3]) ||
        text[4] != '-' ||
        !is_decimal_digit(text[5]) || !is_decimal_digit(text[6]) ||
        text[7] != '-' ||
        !is_decimal_digit(text[8]) || !is_decimal_digit(text[9])) {
        return false;
    }

    const unsigned year = static_cast<unsigned>(text[0] - '0') * 1000U +
                          static_cast<unsigned>(text[1] - '0') * 100U +
                          static_cast<unsigned>(text[2] - '0') * 10U +
                          static_cast<unsigned>(text[3] - '0');
    const unsigned month = static_cast<unsigned>(text[5] - '0') * 10U +
                           static_cast<unsigned>(text[6] - '0');
    const unsigned day = static_cast<unsigned>(text[8] - '0') * 10U +
                         static_cast<unsigned>(text[9] - '0');
    if (month == 0U || month > 12U) {
        return false;
    }

    static constexpr uint8_t kDaysPerMonth[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    unsigned max_day = kDaysPerMonth[month - 1U];
    if (month == 2U && is_leap_year(year)) {
        max_day = 29U;
    }
    return day > 0U && day <= max_day;
}

bool is_valid_daily_csv_row(const char *line, size_t line_len) {
    return line && line_len >= 11U && is_valid_iso_day(line) && line[10] == ',';
}

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
    lockStatus();
    state_ = state;
    last_error_ = error ? error : "";
    last_error_code_ = error_code;
    if (state == SdCardPolicy::State::Fault && error_code)
        captureFirstErrorLocked("mount", error, error_code, "esp");
    unlockStatus();
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
        case ETIMEDOUT:
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
    lockStatus();
    if (kind != DailyStorageFailureKind::Busy && kind != DailyStorageFailureKind::Missing)
        captureFirstErrorLocked(operation, stage, error_code, "errno");
    recovery_.ioFailure(kind, millis());
    if (recovery_.pending()) io_blocked_.store(true);
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
    unlockStatus();
}

void SdCardManager::recordSuccess(const char *operation, bool write_success) const {
    lockStatus();
    if (write_success || !write_fault_) recovery_.ioSuccess();
    if (write_success) {
        has_successful_write_ = true;
        if (operation && (strcmp(operation, "state_write") == 0 || strcmp(operation, "daily_csv") == 0)) {
            write_verification_pending_ = false;
            history_session_ready_.store(true);
        }
        last_write_success_ms_ = millis();
        const bool clears_active_fault =
            !write_fault_ || strcmp(last_operation_, operation ? operation : "") == 0;
        if (clears_active_fault) {
            copy_text(last_operation_, sizeof(last_operation_), operation);
            write_fault_ = false;
            incident_active_ = false;
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
    unlockStatus();
}

DailyStorageFailureKind SdCardManager::lastFailureKind() const {
    DailyStorageFailureKind result = DailyStorageFailureKind::Unknown;
    lockStatus();
    result = last_failure_kind_;
    unlockStatus();
    return result;
}

void SdCardManager::lockStatus() const {
#ifdef UNIT_TEST
    status_mutex_test_.lock();
#else
    if (status_mutex_) xSemaphoreTake(status_mutex_, portMAX_DELAY);
#endif
}

void SdCardManager::unlockStatus() const {
#ifdef UNIT_TEST
    status_mutex_test_.unlock();
#else
    if (status_mutex_) xSemaphoreGive(status_mutex_);
#endif
}

void SdCardManager::captureFirstErrorLocked(const char *operation, const char *stage,
                                          int32_t code, const char *domain) const {
    if (incident_active_) return;
    incident_active_ = true;
    copy_text(first_error_operation_, sizeof(first_error_operation_), operation);
    copy_text(first_error_stage_, sizeof(first_error_stage_), stage);
    first_error_code_ = code;
    first_error_ms_ = millis();
    first_error_domain_ = domain;
}

bool SdCardManager::lockRaw(uint32_t timeout_ms) const {
#ifdef UNIT_TEST
    return file_mutex_test_.try_lock_for(std::chrono::milliseconds(timeout_ms));
#else
    return mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
#endif
}

bool SdCardManager::lock(uint32_t timeout_ms) const {
    if (!isReady() || !lockRaw(timeout_ms)) return false;
    // Recheck after acquiring: an unmount may have run while we waited.
    if (!isReady()) { unlock(); return false; }
    return true;
}

void SdCardManager::unlock() const {
#ifdef UNIT_TEST
    file_mutex_test_.unlock();
#else
    if (mutex_) xSemaphoreGive(mutex_);
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
    if (!lockRaw(0)) return false;
    board_ = board;
    const bool ok = mountLocked();
    initial_mount_ = false;
    lockStatus();
    recovery_.mountResult(ok, mount_retryable_, millis());
    io_blocked_.store(!ok);
    unlockStatus();
    unlock();
    return ok;
}

bool SdCardManager::mountLocked() {
    lockStatus();
    attempted_ = true;
    unlockStatus();
    mount_retryable_ = false;
    if (cleanup_failed_) return false;
#ifdef UNIT_TEST
    const bool ok = test_backend_.mount && test_backend_.mount(test_backend_.context);
    mounted_.store(ok);
    mount_retryable_ = !ok;
    if (!ok) {
        setState(SdCardPolicy::State::NotDetected);
        return false;
    }
    const uint32_t serial = test_backend_.card_serial
        ? test_backend_.card_serial(test_backend_.context) : 1;
    const bool history_present = test_backend_.history_present &&
        test_backend_.history_present(test_backend_.context);
    if (!acceptMountedCard(serial, 0, 0, history_present)) return false;
    setState(SdCardPolicy::State::Mounted);
    return true;
#else
    if (mounted_) return true;
    if (!board_ || !mutex_ || !status_mutex_) {
        setState(SdCardPolicy::State::BoardUnavailable, "board or SD locks unavailable");
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
    if (err != ESP_OK) {
        setState(SdCardPolicy::State::Fault, "SPI bus init failed", err);
        LOGW("SD", "SPI bus init failed: %s", esp_err_to_name(err));
        setCardSelect(false);
        return false;
    }
    spi_bus_owned_ = true;

    // The Waveshare TF slot CS is on CH422G EXIO4, not an ESP GPIO.
    // FatFS/SDSPI is mounted without GPIO CS and EXIO4 is held active while mounted.
    if (!setCardSelect(true)) {
        if (spi_bus_owned_) {
            const esp_err_t free_err = spi_bus_free(kSdSpiHost);
            spi_bus_owned_ = free_err != ESP_OK;
            cleanup_failed_ = spi_bus_owned_;
            if (cleanup_failed_) mount_retryable_ = false;
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

    // Keep the SDK's first mount/FatFs error in the serial log.
    err = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        mount_retryable_ = err == ESP_ERR_TIMEOUT || err == ESP_ERR_INVALID_CRC ||
                           err == ESP_ERR_INVALID_RESPONSE;
        mounted_ = false;
        card_ = nullptr;
        const bool cs_released = setCardSelect(false);
        if (spi_bus_owned_) {
            const esp_err_t free_err = spi_bus_free(kSdSpiHost);
            spi_bus_owned_ = free_err != ESP_OK;
            cleanup_failed_ = spi_bus_owned_;
            if (cleanup_failed_) mount_retryable_ = false;
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
    lockStatus();
    card_size_bytes_ = static_cast<uint64_t>(card_->csd.capacity) * card_->csd.sector_size;
    unlockStatus();
    bool history_present = false;
    if (!history_session_ready_ && !blank_history_accepted_ && !initial_mount_) {
        // Include .bak/.tmp leftovers too: a directory from a previous session
        // needs normal boot-time restoration before any new RAM snapshot is saved.
        struct stat st = {};
        errno = 0;
        const String path = String(kMountPoint) + "/aura/history";
        history_present = stat(path.c_str(), &st) == 0 || errno != ENOENT;
    }
    if (!acceptMountedCard(card_->cid.serial, card_->cid.mfg_id, card_->cid.oem_id,
                           history_present)) return false;
    setState(SdCardPolicy::State::Mounted);
    updateSpaceInfoLocked();
    recordSuccess("mount", false);
    LOGI("SD", "mounted card size=%llu MB free=%llu MB",
         static_cast<unsigned long long>(status().card_size_bytes / (1024ULL * 1024ULL)),
         static_cast<unsigned long long>(filesystem_free_bytes_ / (1024ULL * 1024ULL)));
    return true;
#endif
}

bool SdCardManager::acceptMountedCard(uint32_t serial, uint32_t manufacturer,
                                      uint32_t oem, bool history_present) {
    // RAM history belongs to the accepted card. Do not merge another card's history.
    const bool same_card = have_card_identity_ && card_serial_ == serial &&
        card_manufacturer_ == manufacturer && card_oem_ == oem;
    const bool session_ready = history_session_ready_.load() || blank_history_accepted_;
    media_changed_ = !SdCardPolicy::mayResumeHistory(initial_mount_, have_card_identity_,
                                                    same_card, history_present, session_ready);
    if (media_changed_) {
        setState(SdCardPolicy::State::Fault, "SD history may differ; export current day, then restart");
        return false;
    }
    // Accepting an empty late-inserted store binds it to the existing RAM even
    // before the first write succeeds. Our own failed write may create history
    // directories/sidecars; they must not be mistaken for an unrestored session.
    // This does not acknowledge a successful save, or authorize a different CID.
    if (!initial_mount_ && !session_ready && !history_present)
        blank_history_accepted_ = true;
    have_card_identity_ = true;
    card_serial_ = serial;
    card_manufacturer_ = manufacturer;
    card_oem_ = oem;
    return true;
}

bool SdCardManager::end() {
    recovery_paused_.store(true);
    io_blocked_.store(true);
    if (!waitForRecoveryIdle(5000) || !lockRaw(5000)) return false;
    const bool ok = unmountLocked();
    unlock();
    return ok;
}

bool SdCardManager::unmountLocked() {
    if (cleanup_failed_) return false;
#ifdef UNIT_TEST
    const bool ok = !mounted_ || !test_backend_.unmount ||
                    test_backend_.unmount(test_backend_.context);
    mounted_.store(false);
    cleanup_failed_ = !ok;
    return ok;
#else
    if (mounted_) {
        const esp_err_t err = esp_vfs_fat_sdcard_unmount(kMountPoint, card_);
        mounted_ = false;
        // The SDK may have freed the card even on an unregister error. Never reuse it.
        card_ = nullptr;
        if (err != ESP_OK) {
            cleanup_failed_ = true;
            setState(SdCardPolicy::State::Fault, "SD unmount failed; restart required", err);
            return false;
        }
    }
    if (spi_bus_owned_) {
        const esp_err_t err = spi_bus_free(kSdSpiHost);
        if (err != ESP_OK) {
            cleanup_failed_ = true;
            setState(SdCardPolicy::State::Fault, "SD bus cleanup failed; restart required", err);
            return false;
        }
        spi_bus_owned_ = false;
    }
    const bool cs_released = !board_ || setCardSelect(false);
    lockStatus();
    filesystem_total_bytes_ = 0;
    filesystem_free_bytes_ = 0;
    card_size_bytes_ = 0;
    unlockStatus();
    if (!cs_released) {
        setState(SdCardPolicy::State::Fault, "SD card select unavailable");
        return false;
    }
    setState(SdCardPolicy::State::NotDetected);
    return true;
#endif
}

void SdCardManager::prepareForShutdown() {
    recovery_paused_.store(true);
    lockStatus();
    // A queued manual reconnect must not prevent the normal final history flush
    // on an otherwise healthy card. Failed/changed media stay quarantined.
    if (!worker_running_ && mounted_ && runtime_healthy_ && !write_fault_ &&
        !cleanup_failed_ && !media_changed_) {
        recovery_.cancelPending();
        io_blocked_.store(false);
    }
    unlockStatus();
}

bool SdCardManager::requestRecovery() {
    lockStatus();
    const bool allowed = attempted_ && !cleanup_failed_ &&
                         !recovery_paused_ && state_ != SdCardPolicy::State::BoardUnavailable;
    if (allowed) {
        recovery_.request(millis());
        io_blocked_.store(true);
    }
    unlockStatus();
    return allowed;
}

void SdCardManager::poll(uint32_t now, bool paused) {
    recovery_paused_.store(paused || WakePowerGuard::backgroundPaused(now));
    if (recovery_paused_ || worker_running_) return;
    lockStatus();
    const bool start = recovery_.start(now);
    if (start) worker_running_.store(true);
    unlockStatus();
    if (!start) return;
#ifndef UNIT_TEST
    if (xTaskCreate(recoveryTask, "sd_recovery", 6144, this, 1, nullptr) != pdPASS) {
        lockStatus();
        recovery_.defer(now);
        unlockStatus();
        worker_running_.store(false);
        LOGW("SD", "recovery deferred: task allocation failed");
    }
#endif
}

void SdCardManager::recoveryTask(void *self) {
    static_cast<SdCardManager *>(self)->recover();
#ifndef UNIT_TEST
    vTaskDelete(nullptr);
#endif
}

void SdCardManager::recover() {
    // Atomically admit this worker against cross-core backlight wake. The lease
    // covers teardown, CH422G writes and mounting, so a later wake waits for us.
    // It also rejects a wake that started after poll() dispatched this task.
    WakePowerGuard::Activity wake_activity = WakePowerGuard::tryAcquireActivity(millis());
    // Do not interrupt an open CSV stream. New file access is already gated.
    if (!wake_activity || recovery_paused_ || !lockRaw(0)) {
        lockStatus();
        recovery_.defer(millis());
        unlockStatus();
        worker_running_.store(false);
        return;
    }
    if (recovery_paused_) {
        unlock();
        lockStatus();
        recovery_.defer(millis());
        unlockStatus();
        worker_running_.store(false);
        return;
    }
    mount_retryable_ = false;
    const bool ok = unmountLocked() && mountLocked();
    lockStatus();
    recovery_.mountResult(ok, !cleanup_failed_ && mount_retryable_, millis());
    if (ok) {
        write_verification_pending_ = true;
        recovered_.store(true);
    }
    io_blocked_.store(!ok);
    unlockStatus();
    unlock();
    LOGI("SD", "recovery %s", ok ? "mounted; awaiting history write" : "failed");
    worker_running_.store(false);
}

bool SdCardManager::waitForRecoveryIdle(uint32_t timeout_ms) const {
#ifdef UNIT_TEST
    (void)timeout_ms;
    return recoveryIdle();
#else
    const uint32_t started = millis();
    while (worker_running_) {
        if (millis() - started >= timeout_ms) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
#endif
}

#ifdef UNIT_TEST
void SdCardManager::runRecoveryForTest() {
    if (worker_running_) recover();
}
#endif

SdCardManager::Status SdCardManager::status() const {
    Status result{};
    lockStatus();
    result.state = state_;
    result.mounted = mounted_;
    result.ready = isReady();
    result.attempted = attempted_;
    result.mount_point = kMountPoint;
    result.last_error = last_error_;
    result.last_error_code = last_error_code_;
    result.card_size_bytes = card_size_bytes_;
    result.can_reconnect = attempted_ && !cleanup_failed_ && !recovery_paused_ &&
                           state_ != SdCardPolicy::State::BoardUnavailable;
    result.recovery_pending = recovery_.pending();
    result.recovering = recovery_.active();
    result.recovery_paused = recovery_paused_;
    result.recovery_blocked = recovery_.blocked() || cleanup_failed_ || media_changed_;
    result.recovery_retry_ms = recovery_.remainingMs(millis());
    result.recovery_attempts = recovery_.attempts();
    result.recovery_successes = recovery_.successes();
    result.write_verification_pending = write_verification_pending_;
    result.has_successful_write = has_successful_write_;
    result.last_write_age_ms = has_successful_write_ ? millis() - last_write_success_ms_ : 0;
    copy_text(result.first_error_operation, sizeof(result.first_error_operation), first_error_operation_);
    copy_text(result.first_error_stage, sizeof(result.first_error_stage), first_error_stage_);
    result.first_error_code = first_error_code_;
    result.first_error_ms = first_error_ms_;
    result.first_error_domain = first_error_domain_;
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
    unlockStatus();
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
        String dir(path.c_str(), static_cast<size_t>(i));
        if (dir.length() <= strlen(kMountPoint)) {
            continue;
        }
        errno = 0;
#if defined(UNIT_TEST) && defined(_WIN32)
        const int mkdir_result = _mkdir(dir.c_str());
#else
        const int mkdir_result = mkdir(dir.c_str(), 0775);
#endif
        if (mkdir_result != 0 && errno != EEXIST) {
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
    if (!path) {
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
    if (!path || !text) {
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
    if (ok && sync_file(file) != 0) {
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
    if (!path || !unique_line_prefix || unique_line_prefix[0] == '\0' || !block) {
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
    if (ok && sync_file(target) != 0) {
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

bool SdCardManager::removeDailyCsvRowsOnOrAfterAtomic(const char *path,
                                                       const char *cutoff_iso_day) {
    constexpr const char *kOperation = "daily_csv_prune";
    if (!path || !cutoff_iso_day || strlen(cutoff_iso_day) != 10U ||
        !is_valid_iso_day(cutoff_iso_day)) {
        recordFailure(kOperation, "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure(kOperation, "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }

    const String final_path = fullPath(path);
    const String tmp_path = final_path + ".tmp";
    const String backup_path = final_path + ".bak";
    const auto purge_sidecar = [&](const String &candidate,
                                   const char *stat_stage,
                                   const char *remove_stage,
                                   const char *verify_stage) {
        struct stat st = {};
        errno = 0;
        if (stat(candidate.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                return true;
            }
            const int sidecar_error = current_errno_or_io();
            recordFailure(kOperation,
                          stat_stage,
                          sidecar_error,
                          classifyErrno(sidecar_error),
                          true);
            return false;
        }
        errno = 0;
        if (remove(candidate.c_str()) != 0) {
            const int sidecar_error = current_errno_or_io();
            recordFailure(kOperation,
                          remove_stage,
                          sidecar_error,
                          classifyErrno(sidecar_error),
                          true);
            return false;
        }
        errno = 0;
        if (stat(candidate.c_str(), &st) != 0 && errno == ENOENT) {
            return true;
        }
        const int sidecar_error = errno == 0 ? EIO : current_errno_or_io();
        recordFailure(kOperation,
                      verify_stage,
                      sidecar_error,
                      classifyErrno(sidecar_error),
                      true);
        return false;
    };
    if (!recoverAtomicBackupLocked(final_path, kOperation)) {
        unlock();
        return false;
    }
    // If a previous committed replace could not remove its backup, discard it
    // before using the current final as the source of truth. A prune journal
    // must never be cleared while an unpruned recoverable backup remains.
    if (!purge_sidecar(tmp_path,
                       "stat_stale_temp",
                       "remove_stale_temp",
                       "verify_stale_temp") ||
        !purge_sidecar(backup_path,
                       "stat_stale_backup",
                       "remove_stale_backup",
                       "verify_stale_backup")) {
        unlock();
        return false;
    }

    errno = 0;
    FILE *source = fopen(final_path.c_str(), "rb");
    if (!source) {
        if (errno == ENOENT) {
            recordSuccess(kOperation, false);
            unlock();
            return true;
        }
        const int error_code = current_errno_or_io();
        recordFailure(kOperation, "open_source", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }

    errno = 0;
    FILE *target = fopen(tmp_path.c_str(), "wb");
    if (!target) {
        const int error_code = current_errno_or_io();
        fclose(source);
        recordFailure(kOperation, "open_temp", error_code, classifyErrno(error_code), true);
        unlock();
        return false;
    }

    bool ok = true;
    bool removed_any = false;
    bool at_line_start = true;
    bool remove_current_line = false;
    int error_code = 0;
    const char *stage = "";
    char line[kCopyBufferSize];
    while (fgets(line, sizeof(line), source)) {
        const size_t line_len = strlen(line);
        if (at_line_start) {
            remove_current_line = is_valid_daily_csv_row(line, line_len) &&
                                  strncmp(line, cutoff_iso_day, 10U) >= 0;
            removed_any = removed_any || remove_current_line;
        }
        if (!remove_current_line && line_len > 0U &&
            fwrite(line, 1, line_len, target) != line_len) {
            ok = false;
            error_code = current_errno_or_io();
            stage = "copy_write";
            break;
        }
        at_line_start = line_len > 0U && line[line_len - 1U] == '\n';
        if (at_line_start) {
            remove_current_line = false;
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
    if (ok && fflush(target) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "flush_temp";
    }
    if (ok && sync_file(target) != 0) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "sync_temp";
    }
    if (fclose(target) != 0 && ok) {
        ok = false;
        error_code = current_errno_or_io();
        stage = "close_temp";
    }

    if (ok && removed_any) {
        ok = replaceWithTempLocked(final_path, tmp_path, kOperation);
    } else {
        ok = purge_sidecar(tmp_path,
                           "stat_temp_cleanup",
                           "remove_temp_cleanup",
                           "verify_temp_cleanup");
    }
    if (!ok) {
        const int operation_error = error_code != 0 ? error_code : current_errno_or_io();
        errno = 0;
        remove(tmp_path.c_str());
        if (error_code != 0) {
            recordFailure(kOperation,
                          stage,
                          operation_error,
                          classifyErrno(operation_error),
                          true);
        }
        unlock();
        return false;
    }

    // replaceWithTempLocked deliberately treats a committed final as success
    // even when backup cleanup fails. Tighten that contract for journaled CSV
    // pruning: success here requires that no stale atomic artifact can later
    // restore rows on or after the cutoff.
    if (!purge_sidecar(tmp_path,
                       "stat_final_temp",
                       "remove_final_temp",
                       "verify_final_temp") ||
        !purge_sidecar(backup_path,
                       "stat_final_backup",
                       "remove_final_backup",
                       "verify_final_backup")) {
        unlock();
        return false;
    }

    if (removed_any) {
        updateSpaceInfoLocked();
    }
    recordSuccess(kOperation, removed_any);
    unlock();
    return true;
}

bool SdCardManager::readBinary(const char *path, void *out, size_t len, size_t &out_len) const {
    out_len = 0;
    if (!path || !out) {
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
    if (!path || !data) {
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
    if (ok && sync_file(file) != 0) {
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
    if (!path) {
        recordFailure("remove", "validate", EINVAL, DailyStorageFailureKind::Invalid, true);
        return false;
    }
    if (!lock()) {
        recordFailure("remove", "lock", EBUSY, DailyStorageFailureKind::Busy, false);
        return false;
    }
    const String final_path = fullPath(path);
    const String backup_path = final_path + ".bak";
    const String tmp_path = final_path + ".tmp";
    bool any_existed = false;

    const auto remove_if_present = [&](const String &candidate,
                                       const char *stat_stage,
                                       const char *remove_stage,
                                       const char *verify_stage) {
        struct stat st = {};
        errno = 0;
        if (stat(candidate.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                return true;
            }
            const int error_code = current_errno_or_io();
            recordFailure("remove",
                          stat_stage,
                          error_code,
                          classifyErrno(error_code),
                          true);
            return false;
        }
        any_existed = true;
        errno = 0;
        if (remove(candidate.c_str()) != 0) {
            const int error_code = current_errno_or_io();
            recordFailure("remove",
                          remove_stage,
                          error_code,
                          classifyErrno(error_code),
                          true);
            return false;
        }
        errno = 0;
        if (stat(candidate.c_str(), &st) != 0 && errno == ENOENT) {
            return true;
        }
        const int error_code = errno == 0 ? EIO : current_errno_or_io();
        recordFailure("remove",
                      verify_stage,
                      error_code,
                      classifyErrno(error_code),
                      true);
        return false;
    };

    // An explicit logical-file delete must also purge atomic-write artifacts.
    // Delete sidecars first and the committed file last so an
    // intermediate failure cannot make an old backup become authoritative.
    const bool ok =
        remove_if_present(tmp_path, "stat_temp", "remove_temp", "verify_temp") &&
        remove_if_present(backup_path,
                          "stat_backup",
                          "remove_backup",
                          "verify_backup") &&
        remove_if_present(final_path, "stat_final", "remove_final", "verify_final");
    if (ok) {
        if (any_existed) {
            updateSpaceInfoLocked();
        }
        recordSuccess("remove", any_existed);
    }
    unlock();
    return ok;
}

FILE *SdCardManager::openReadUnlocked(const char *relative_path) const {
    if (!relative_path) {
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

FILE *SdCardManager::openReadLocked(const char *relative_path) const {
    return openReadUnlocked(relative_path);
}
