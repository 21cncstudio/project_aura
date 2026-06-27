// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/SdCardManager.h"

#include <string.h>
#include <sys/stat.h>

#include "core/Logger.h"

#ifndef UNIT_TEST
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_display_panel.hpp>
#include <esp_err.h>
#include <esp_vfs_fat.h>
#endif

namespace {

#ifndef UNIT_TEST
constexpr spi_host_device_t kSdSpiHost = SPI2_HOST;
#endif

bool path_has_parent_char(char ch) {
    return ch == '/' || ch == '\\';
}

} // namespace

SdCardManager::SdCardManager() {
#ifndef UNIT_TEST
    mutex_ = xSemaphoreCreateMutex();
#endif
}

SdCardManager::~SdCardManager() {
    end();
#ifndef UNIT_TEST
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
#endif
}

void SdCardManager::setError(const char *error) {
    last_error_ = error ? error : "";
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
    expander->digitalWrite(kSdCsExio, selected ? 0 : 1);
    return true;
#endif
}

bool SdCardManager::begin(esp_panel::board::Board *board) {
#ifdef UNIT_TEST
    (void)board;
    attempted_ = true;
    mounted_ = false;
    setError("unit test");
    return false;
#else
    attempted_ = true;
    if (mounted_) {
        return true;
    }
    board_ = board;
    spi_bus_owned_ = false;
    if (!board_) {
        setError("board unavailable");
        LOGW("SD", "skip SD mount: board unavailable");
        return false;
    }
    if (!setCardSelect(false)) {
        setError("CS expander unavailable");
        LOGW("SD", "skip SD mount: CS expander unavailable");
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
        setError("SPI bus init failed");
        LOGW("SD", "SPI bus init failed: %s", esp_err_to_name(err));
        setCardSelect(false);
        return false;
    }
    spi_bus_owned_ = err == ESP_OK;

    // The Waveshare TF slot CS is on CH422G EXIO4, not an ESP GPIO.
    // FatFS/SDSPI is mounted without GPIO CS and EXIO4 is held active while mounted.
    setCardSelect(true);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SDSPI_SLOT_NO_CS;
    slot_config.host_id = kSdSpiHost;

    esp_vfs_fat_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 4;
    mount_config.allocation_unit_size = 16 * 1024;

    err = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        mounted_ = false;
        card_ = nullptr;
        setCardSelect(false);
        if (spi_bus_owned_) {
            spi_bus_free(kSdSpiHost);
            spi_bus_owned_ = false;
        }
        setError("SD mount failed");
        LOGW("SD", "mount failed: %s", esp_err_to_name(err));
        return false;
    }

    mounted_ = true;
    setError("");
    LOGI("SD", "mounted card size=%llu MB",
         static_cast<unsigned long long>(status().card_size_bytes / (1024ULL * 1024ULL)));
    return true;
#endif
}

void SdCardManager::end() {
#ifndef UNIT_TEST
    if (mounted_) {
        esp_vfs_fat_sdcard_unmount(kMountPoint, card_);
        if (spi_bus_owned_) {
            spi_bus_free(kSdSpiHost);
        }
        setCardSelect(false);
    }
    mounted_ = false;
    card_ = nullptr;
    spi_bus_owned_ = false;
#endif
}

SdCardManager::Status SdCardManager::status() const {
    Status result{};
    result.mounted = mounted_;
    result.attempted = attempted_;
    result.mount_point = kMountPoint;
    result.last_error = last_error_;
#ifndef UNIT_TEST
    if (card_) {
        result.card_size_bytes =
            static_cast<uint64_t>(card_->csd.capacity) * static_cast<uint64_t>(card_->csd.sector_size);
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

bool SdCardManager::ensureParentDirs(const char *relative_path) const {
    String path = fullPath(relative_path);
    int start = strlen(kMountPoint);
    for (int i = start + 1; i < static_cast<int>(path.length()); ++i) {
        if (!path_has_parent_char(path[i])) {
            continue;
        }
        String dir = path.substring(0, i);
        if (dir.length() <= strlen(kMountPoint)) {
            continue;
        }
        mkdir(dir.c_str(), 0775);
    }
    return true;
}

bool SdCardManager::fileExists(const char *path) const {
    if (!mounted_ || !path) {
        return false;
    }
    struct stat st = {};
    const String full = fullPath(path);
    return stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool SdCardManager::fileSize(const char *path, size_t &out_size) const {
    out_size = 0;
    if (!mounted_ || !path) {
        return false;
    }
    struct stat st = {};
    const String full = fullPath(path);
    if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    out_size = static_cast<size_t>(st.st_size);
    return true;
}

bool SdCardManager::appendText(const char *path, const char *text) {
    if (!mounted_ || !path || !text || !lock()) {
        return false;
    }
    ensureParentDirs(path);
    const String full = fullPath(path);
    FILE *file = fopen(full.c_str(), "ab");
    if (!file) {
        unlock();
        return false;
    }
    const size_t len = strlen(text);
    const bool ok = len == 0 || fwrite(text, 1, len, file) == len;
    fflush(file);
    fclose(file);
    unlock();
    return ok;
}

bool SdCardManager::readBinary(const char *path, void *out, size_t len, size_t &out_len) const {
    out_len = 0;
    if (!mounted_ || !path || !out || !lock()) {
        return false;
    }
    const String full = fullPath(path);
    FILE *file = fopen(full.c_str(), "rb");
    if (!file) {
        unlock();
        return false;
    }
    out_len = fread(out, 1, len, file);
    const bool ok = ferror(file) == 0;
    fclose(file);
    unlock();
    return ok;
}

bool SdCardManager::writeBinaryAtomic(const char *path, const void *data, size_t len) {
    if (!mounted_ || !path || !data || !lock()) {
        return false;
    }
    ensureParentDirs(path);
    const String final_path = fullPath(path);
    const String tmp_path = final_path + ".tmp";
    FILE *file = fopen(tmp_path.c_str(), "wb");
    if (!file) {
        unlock();
        return false;
    }
    const bool wrote = len == 0 || fwrite(data, 1, len, file) == len;
    fflush(file);
    fclose(file);
    bool ok = wrote;
    if (ok) {
        remove(final_path.c_str());
        ok = rename(tmp_path.c_str(), final_path.c_str()) == 0;
    }
    if (!ok) {
        remove(tmp_path.c_str());
    }
    unlock();
    return ok;
}

bool SdCardManager::removeFile(const char *path) {
    if (!mounted_ || !path || !lock()) {
        return false;
    }
    const String full = fullPath(path);
    const bool ok = remove(full.c_str()) == 0;
    unlock();
    return ok;
}

FILE *SdCardManager::openRead(const char *relative_path) const {
    if (!mounted_ || !relative_path) {
        return nullptr;
    }
    const String full = fullPath(relative_path);
    return fopen(full.c_str(), "rb");
}
