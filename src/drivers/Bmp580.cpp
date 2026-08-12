// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Bmp580.h"

#include <driver/i2c.h>
#include <math.h>

#include "config/AppConfig.h"
#include "core/Logger.h"

namespace {

constexpr uint8_t kOdrMask = 0x7C;
constexpr uint8_t kOdrShift = 2;
constexpr uint8_t kPowerModeMask = 0x03;
constexpr uint8_t kDeepDisableMask = 0x80;

constexpr uint8_t kOsrTempMask = 0x07;
constexpr uint8_t kOsrPressMask = 0x38;
constexpr uint8_t kOsrPressShift = 3;
constexpr uint8_t kPressEnableMask = 0x40;

constexpr uint8_t kIirTempMask = 0x07;
constexpr uint8_t kIirPressMask = 0x38;
constexpr uint8_t kIirPressShift = 3;
constexpr uint8_t kIirReservedMask = 0xC0;

int32_t signExtend24(int32_t value) {
    return (value << 8) >> 8;
}

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

const char *bmp58x_variant_label(Bmp580::Variant variant) {
    switch (variant) {
        case Bmp580::Variant::BMP580_581:
            return "BMP580/581";
        case Bmp580::Variant::BMP585:
            return "BMP585";
        default:
            return "BMP58x";
    }
}

} // namespace

bool Bmp580::begin() {
    ok_ = false;
    addr_ = 0;
    pressure_has_ = false;
    pressure_filtered_ = 0.0f;
    temperature_c_ = 0.0f;
    raw_temperature_ = 0;
    raw_pressure_ = 0;
    last_poll_ms_ = 0;
    last_data_ms_ = 0;
    no_data_since_ms_ = 0;
    last_recover_ms_ = 0;
    pressure_valid_ = false;
    has_new_data_ = false;
    variant_ = Variant::Unknown;
    late_start_phase_ = LateStartPhase::Idle;
    late_start_due_ms_ = 0;
    late_start_deadline_ms_ = 0;
    late_start_reg_value_ = 0;
    return true;
}

bool Bmp580::detect(uint8_t addr) {
    uint8_t reg = Config::BMP580_REG_CHIP_ID;
    uint8_t value = 0;
    esp_err_t err = i2c_master_write_read_device(
        Config::I2C_PORT,
        addr,
        &reg,
        1,
        &value,
        1,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    if (err != ESP_OK) {
        return false;
    }
    switch (value) {
        case Config::BMP580_CHIP_ID_PRIMARY:
            variant_ = Variant::BMP580_581;
            break;
        case Config::BMP580_CHIP_ID_SECONDARY:
            variant_ = Variant::BMP585;
            break;
        default:
            return false;
    }
    addr_ = addr;
    return true;
}

bool Bmp580::writeU8(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    esp_err_t err = i2c_master_write_to_device(
        Config::I2C_PORT,
        addr_,
        data,
        sizeof(data),
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Bmp580::readBytes(uint8_t reg, uint8_t *buf, size_t len) {
    esp_err_t err = i2c_master_write_read_device(
        Config::I2C_PORT,
        addr_,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Bmp580::readU8(uint8_t reg, uint8_t &value) {
    return readBytes(reg, &value, 1);
}

bool Bmp580::softReset() {
    if (!writeU8(Config::BMP580_REG_CMD, Config::BMP580_SOFT_RESET_CMD)) {
        return false;
    }
    delay(5);
    return true;
}

bool Bmp580::waitNvmReady(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        uint8_t status = 0;
        if (readU8(Config::BMP580_REG_STATUS, status)) {
            if (status & Config::BMP580_STATUS_NVM_RDY) {
                return true;
            }
        }
        delay(1);
    }
    return false;
}

bool Bmp580::configure() {
    uint8_t osr = static_cast<uint8_t>(Config::BMP580_OSR_4X & kOsrTempMask);
    osr |= static_cast<uint8_t>((Config::BMP580_OSR_4X & kOsrTempMask) << kOsrPressShift);
    osr |= kPressEnableMask;
    if (!writeU8(Config::BMP580_REG_OSR_CONFIG, osr)) {
        return false;
    }

    uint8_t iir = 0;
    if (!readU8(Config::BMP580_REG_DSP_IIR, iir)) {
        return false;
    }
    iir &= kIirReservedMask;
    iir |= static_cast<uint8_t>(Config::BMP580_IIR_BYPASS & kIirTempMask);
    iir |= static_cast<uint8_t>((Config::BMP580_IIR_BYPASS & kIirTempMask) << kIirPressShift);
    if (!writeU8(Config::BMP580_REG_DSP_IIR, iir)) {
        return false;
    }

    uint8_t odr = 0;
    if (!readU8(Config::BMP580_REG_ODR_CONFIG, odr)) {
        return false;
    }
    odr &= ~kPowerModeMask;
    odr |= Config::BMP580_POWERMODE_CONTINUOUS & kPowerModeMask;
    odr &= ~kOdrMask;
    odr |= static_cast<uint8_t>((Config::BMP580_ODR_1_HZ << kOdrShift) & kOdrMask);
    odr |= kDeepDisableMask;
    if (!writeU8(Config::BMP580_REG_ODR_CONFIG, odr)) {
        return false;
    }

    return true;
}

bool Bmp580::readRaw() {
    uint8_t buf[6] = {};
    if (!readBytes(Config::BMP580_REG_TEMP_XLSB, buf, sizeof(buf))) {
        return false;
    }
    int32_t raw_t = static_cast<int32_t>(
        (static_cast<uint32_t>(buf[2]) << 16) |
        (static_cast<uint32_t>(buf[1]) << 8) |
        static_cast<uint32_t>(buf[0])
    );
    raw_temperature_ = signExtend24(raw_t);

    raw_pressure_ =
        (static_cast<uint32_t>(buf[5]) << 16) |
        (static_cast<uint32_t>(buf[4]) << 8) |
        static_cast<uint32_t>(buf[3]);
    return true;
}

bool Bmp580::compute(float &pressure_hpa, float &temperature_c) {
    temperature_c = static_cast<float>(raw_temperature_) / 65536.0f;
    float pressure_pa = static_cast<float>(raw_pressure_) / 64.0f;
    pressure_hpa = pressure_pa / 100.0f;
    return true;
}

const char *Bmp580::variantLabel() const {
    return bmp58x_variant_label(variant_);
}

bool Bmp580::start() {
    addr_ = 0;
    variant_ = Variant::Unknown;
    if (detect(Config::BMP580_ADDR_PRIMARY)) {
        Logger::log(Logger::Info, "BMP58x", "%s found at 0x%02X",
                    variantLabel(), static_cast<unsigned>(Config::BMP580_ADDR_PRIMARY));
    } else if (detect(Config::BMP580_ADDR_ALT)) {
        Logger::log(Logger::Info, "BMP58x", "%s found at 0x%02X",
                    variantLabel(), static_cast<unsigned>(Config::BMP580_ADDR_ALT));
    } else {
        ok_ = false;
        return false;
    }

    if (!softReset()) {
        ok_ = false;
        return false;
    }
    if (!waitNvmReady(20)) {
        LOGW("BMP58x", "%s NVM_RDY timeout", variantLabel());
        ok_ = false;
        return false;
    }

    if (!configure()) {
        ok_ = false;
        return false;
    }

    ok_ = true;
    pressure_valid_ = false;
    pressure_has_ = false;
    has_new_data_ = false;
    no_data_since_ms_ = 0;
    last_data_ms_ = 0;
    return true;
}

void Bmp580::beginLateStart() {
    addr_ = 0;
    variant_ = Variant::Unknown;
    ok_ = false;
    late_start_due_ms_ = 0;
    late_start_deadline_ms_ = 0;
    late_start_reg_value_ = 0;
    late_start_phase_ = LateStartPhase::DetectPrimary;
}

CooperativeStart::Result Bmp580::finishLateStart(bool success) {
    late_start_phase_ = LateStartPhase::Idle;
    if (!success) {
        ok_ = false;
        return CooperativeStart::Result::Failed;
    }
    ok_ = true;
    pressure_valid_ = false;
    pressure_has_ = false;
    has_new_data_ = false;
    no_data_since_ms_ = 0;
    last_data_ms_ = 0;
    return CooperativeStart::Result::Success;
}

CooperativeStart::Result Bmp580::pollLateStart(uint32_t now_ms) {
    switch (late_start_phase_) {
        case LateStartPhase::Idle:
            return CooperativeStart::Result::Idle;
        case LateStartPhase::DetectPrimary:
            if (detect(Config::BMP580_ADDR_PRIMARY)) {
                Logger::log(Logger::Info, "BMP58x", "%s found at 0x%02X",
                            variantLabel(), static_cast<unsigned>(Config::BMP580_ADDR_PRIMARY));
                late_start_phase_ = LateStartPhase::SoftReset;
            } else {
                late_start_phase_ = LateStartPhase::DetectAlt;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAlt:
            if (!detect(Config::BMP580_ADDR_ALT)) {
                return finishLateStart(false);
            }
            Logger::log(Logger::Info, "BMP58x", "%s found at 0x%02X",
                        variantLabel(), static_cast<unsigned>(Config::BMP580_ADDR_ALT));
            late_start_phase_ = LateStartPhase::SoftReset;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::SoftReset:
            if (!writeU8(Config::BMP580_REG_CMD, Config::BMP580_SOFT_RESET_CMD)) {
                return finishLateStart(false);
            }
            late_start_due_ms_ = millis() + 5U;
            late_start_phase_ = LateStartPhase::WaitReset;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitReset:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            late_start_deadline_ms_ = now_ms + 20U;
            late_start_phase_ = LateStartPhase::WaitNvm;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitNvm: {
            uint8_t status = 0;
            if (readU8(Config::BMP580_REG_STATUS, status) &&
                (status & Config::BMP580_STATUS_NVM_RDY) != 0U) {
                late_start_phase_ = LateStartPhase::WriteOsr;
                return CooperativeStart::Result::InProgress;
            }
            if (deadlineReached(now_ms, late_start_deadline_ms_)) {
                LOGW("BMP58x", "%s NVM_RDY timeout", variantLabel());
                return finishLateStart(false);
            }
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::WriteOsr: {
            uint8_t osr = static_cast<uint8_t>(Config::BMP580_OSR_4X & kOsrTempMask);
            osr |= static_cast<uint8_t>((Config::BMP580_OSR_4X & kOsrTempMask) << kOsrPressShift);
            osr |= kPressEnableMask;
            if (!writeU8(Config::BMP580_REG_OSR_CONFIG, osr)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::ReadIir;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::ReadIir:
            if (!readU8(Config::BMP580_REG_DSP_IIR, late_start_reg_value_)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteIir;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteIir: {
            uint8_t iir = static_cast<uint8_t>(late_start_reg_value_ & kIirReservedMask);
            iir |= static_cast<uint8_t>(Config::BMP580_IIR_BYPASS & kIirTempMask);
            iir |= static_cast<uint8_t>((Config::BMP580_IIR_BYPASS & kIirTempMask) << kIirPressShift);
            if (!writeU8(Config::BMP580_REG_DSP_IIR, iir)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::ReadOdr;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::ReadOdr:
            if (!readU8(Config::BMP580_REG_ODR_CONFIG, late_start_reg_value_)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteOdr;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteOdr: {
            uint8_t odr = static_cast<uint8_t>(late_start_reg_value_ & ~kPowerModeMask);
            odr |= Config::BMP580_POWERMODE_CONTINUOUS & kPowerModeMask;
            odr &= ~kOdrMask;
            odr |= static_cast<uint8_t>((Config::BMP580_ODR_1_HZ << kOdrShift) & kOdrMask);
            odr |= kDeepDisableMask;
            if (!writeU8(Config::BMP580_REG_ODR_CONFIG, odr)) {
                return finishLateStart(false);
            }
            return finishLateStart(true);
        }
        default:
            return finishLateStart(false);
    }
}

void Bmp580::tryRecover(uint32_t now, const char *reason) {
    if (now - last_recover_ms_ < Config::BMP580_RECOVER_COOLDOWN_MS) {
        return;
    }
    last_recover_ms_ = now;
    Logger::log(Logger::Warn, "BMP58x", "%s - reinit", reason);
    bool ok = start();
    if (ok) {
        Logger::log(Logger::Info, "BMP58x", "%s recovery OK", variantLabel());
        ok_ = true;
        pressure_has_ = false;
        no_data_since_ms_ = 0;
        last_data_ms_ = 0;
        pressure_valid_ = false;
    } else {
        LOGW("BMP58x", "recovery failed");
        ok_ = false;
    }
}

void Bmp580::handleNoData(uint32_t now, const char *reason) {
    if (no_data_since_ms_ == 0) {
        no_data_since_ms_ = now;
    }
    if (last_data_ms_ != 0 && (now - last_data_ms_) > Config::BMP580_STALE_MS) {
        pressure_valid_ = false;
    }
    if (now - no_data_since_ms_ >= Config::BMP580_RECOVER_MS) {
        tryRecover(now, reason);
    }
}

void Bmp580::poll() {
    uint32_t now = millis();
    if (!ok_) {
        tryRecover(now, "not ready");
        return;
    }
    if (now - last_poll_ms_ < Config::BMP580_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    if (!readRaw()) {
        handleNoData(now, "read fail");
        return;
    }

    float pressure_hpa = 0.0f;
    float temperature_c = 0.0f;
    if (!compute(pressure_hpa, temperature_c)) {
        handleNoData(now, "compute fail");
        return;
    }

    if (!isfinite(pressure_hpa) || pressure_hpa <= 0.0f) {
        handleNoData(now, "invalid");
        return;
    }

    temperature_c_ = temperature_c;
    no_data_since_ms_ = 0;

    if (!pressure_has_) {
        pressure_filtered_ = pressure_hpa;
        pressure_has_ = true;
    } else {
        pressure_filtered_ += Config::BMP580_PRESSURE_ALPHA *
                              (pressure_hpa - pressure_filtered_);
    }

    last_data_ms_ = now;
    pressure_valid_ = true;
    has_new_data_ = true;
}

bool Bmp580::takeNewData(float &pressure_hpa, float &temperature_c) {
    if (!has_new_data_) {
        return false;
    }
    pressure_hpa = pressure_filtered_;
    temperature_c = temperature_c_;
    has_new_data_ = false;
    return true;
}

void Bmp580::invalidate() {
    pressure_valid_ = false;
    has_new_data_ = false;
}
