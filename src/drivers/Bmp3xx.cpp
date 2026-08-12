// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Bmp3xx.h"
#include "drivers/Bmp3xxProbe.h"

#include <driver/i2c.h>
#include <math.h>

#include "config/AppConfig.h"
#include "core/Logger.h"

namespace {

constexpr uint8_t kBmp3xxErrReservedMask = 0xF8;
constexpr uint8_t kBmp3xxPwrCtrlReservedMask = 0xCC;
constexpr uint8_t kBmp3xxOsrReservedMask = 0xC0;
constexpr uint8_t kBmp3xxOdrReservedMask = 0xE0;

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

uint16_t readU16Le(const uint8_t *buf) {
    return static_cast<uint16_t>(buf[0]) |
           (static_cast<uint16_t>(buf[1]) << 8);
}

int16_t readS16Le(const uint8_t *buf) {
    return static_cast<int16_t>(readU16Le(buf));
}

const char *bmp3xx_variant_label(Bmp3xx::Variant variant) {
    switch (variant) {
        case Bmp3xx::Variant::BMP388:
            return "BMP388";
        case Bmp3xx::Variant::BMP390:
            return "BMP390";
        default:
            return "BMP3xx";
    }
}

} // namespace

bool Bmp3xx::begin() {
    ok_ = false;
    addr_ = 0;
    pressure_has_ = false;
    pressure_filtered_ = 0.0f;
    temperature_c_ = 0.0f;
    calib_ = Calibration{};
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
    return true;
}

bool Bmp3xx::detect(uint8_t addr) {
    Bmp3xxProbe::Variant detected = Bmp3xxProbe::Variant::Unknown;
    if (!Bmp3xxProbe::detect(addr, detected)) {
        return false;
    }

    switch (detected) {
        case Bmp3xxProbe::Variant::BMP388:
            variant_ = Variant::BMP388;
            break;
        case Bmp3xxProbe::Variant::BMP390:
            variant_ = Variant::BMP390;
            break;
        default:
            return false;
    }

    addr_ = addr;
    return true;
}

bool Bmp3xx::writeU8(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    esp_err_t err = i2c_master_write_to_device(
        Config::I2C_PORT,
        addr_,
        data,
        sizeof(data),
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Bmp3xx::readBytes(uint8_t reg, uint8_t *buf, size_t len) {
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

bool Bmp3xx::readU8(uint8_t reg, uint8_t &value) {
    return readBytes(reg, &value, 1);
}

bool Bmp3xx::softReset() {
    if (!writeU8(Config::BMP3XX_REG_CMD, Config::BMP3XX_CMD_SOFT_RESET)) {
        return false;
    }
    delay(5);
    return waitCmdReady(20);
}

bool Bmp3xx::waitCmdReady(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        uint8_t status = 0;
        if (readU8(Config::BMP3XX_REG_STATUS, status) &&
            (status & Config::BMP3XX_STATUS_CMD_RDY)) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool Bmp3xx::readCalibration() {
    uint8_t coeffs[Config::BMP3XX_CALIB_DATA_LEN] = {};
    if (!readBytes(Config::BMP3XX_REG_CALIB_DATA, coeffs, sizeof(coeffs))) {
        return false;
    }

    // Compensation scaling derived from Bosch BMP3_SensorAPI.
    calib_.par_t1 = static_cast<double>(readU16Le(&coeffs[0])) / 0.00390625;
    calib_.par_t2 = static_cast<double>(readU16Le(&coeffs[2])) / 1073741824.0;
    calib_.par_t3 = static_cast<double>(static_cast<int8_t>(coeffs[4])) /
                    281474976710656.0;

    calib_.par_p1 = (static_cast<double>(readS16Le(&coeffs[5])) - 16384.0) /
                    1048576.0;
    calib_.par_p2 = (static_cast<double>(readS16Le(&coeffs[7])) - 16384.0) /
                    536870912.0;
    calib_.par_p3 = static_cast<double>(static_cast<int8_t>(coeffs[9])) /
                    4294967296.0;
    calib_.par_p4 = static_cast<double>(static_cast<int8_t>(coeffs[10])) /
                    137438953472.0;
    calib_.par_p5 = static_cast<double>(readU16Le(&coeffs[11])) / 0.125;
    calib_.par_p6 = static_cast<double>(readU16Le(&coeffs[13])) / 64.0;
    calib_.par_p7 = static_cast<double>(static_cast<int8_t>(coeffs[15])) / 256.0;
    calib_.par_p8 = static_cast<double>(static_cast<int8_t>(coeffs[16])) /
                    32768.0;
    calib_.par_p9 = static_cast<double>(readS16Le(&coeffs[17])) /
                    281474976710656.0;
    calib_.par_p10 = static_cast<double>(static_cast<int8_t>(coeffs[19])) /
                     281474976710656.0;
    calib_.par_p11 = static_cast<double>(static_cast<int8_t>(coeffs[20])) /
                     36893488147419103232.0;
    calib_.t_lin = 0.0;
    return true;
}

bool Bmp3xx::configure() {
    const uint8_t osr =
        static_cast<uint8_t>(Config::BMP3XX_PRESS_OS_4X & 0x07) |
        static_cast<uint8_t>((Config::BMP3XX_TEMP_OS_2X & 0x07) << 3);
    if (!writeU8(Config::BMP3XX_REG_OSR, osr)) {
        return false;
    }

    if (!writeU8(Config::BMP3XX_REG_ODR, Config::BMP3XX_ODR_0P78_HZ & 0x1F)) {
        return false;
    }

    const uint8_t config =
        static_cast<uint8_t>((Config::BMP3XX_IIR_BYPASS & 0x07) << 1);
    if (!writeU8(Config::BMP3XX_REG_CONFIG, config)) {
        return false;
    }

    const uint8_t power =
        static_cast<uint8_t>(Config::BMP3XX_PRESS_EN |
                             Config::BMP3XX_TEMP_EN |
                             ((Config::BMP3XX_MODE_NORMAL & 0x03) << 4));
    if (!writeU8(Config::BMP3XX_REG_PWR_CTRL, power)) {
        return false;
    }

    delay(2);

    uint8_t err = 0;
    if (readU8(Config::BMP3XX_REG_ERR, err) &&
        (err & (Config::BMP3XX_ERR_FATAL |
                Config::BMP3XX_ERR_CMD |
                Config::BMP3XX_ERR_CONF))) {
        Logger::log(Logger::Warn, "BMP3xx",
                    "%s config error 0x%02X",
                    variantLabel(),
                    static_cast<unsigned>(err));
        return false;
    }

    return true;
}

bool Bmp3xx::dataReady() {
    uint8_t status = 0;
    if (!readU8(Config::BMP3XX_REG_STATUS, status)) {
        return false;
    }
    return (status & (Config::BMP3XX_STATUS_DRDY_PRESS |
                      Config::BMP3XX_STATUS_DRDY_TEMP)) ==
           (Config::BMP3XX_STATUS_DRDY_PRESS |
            Config::BMP3XX_STATUS_DRDY_TEMP);
}

bool Bmp3xx::readRaw() {
    uint8_t buf[6] = {};
    if (!readBytes(Config::BMP3XX_REG_DATA, buf, sizeof(buf))) {
        return false;
    }

    raw_pressure_ = static_cast<uint32_t>(buf[0]) |
                    (static_cast<uint32_t>(buf[1]) << 8) |
                    (static_cast<uint32_t>(buf[2]) << 16);
    raw_temperature_ = static_cast<uint32_t>(buf[3]) |
                       (static_cast<uint32_t>(buf[4]) << 8) |
                       (static_cast<uint32_t>(buf[5]) << 16);
    return true;
}

bool Bmp3xx::compute(float &pressure_hpa, float &temperature_c) {
    const double uncomp_temp = static_cast<double>(raw_temperature_);
    const double temp_delta = uncomp_temp - calib_.par_t1;
    calib_.t_lin = temp_delta * calib_.par_t2 +
                   (temp_delta * temp_delta) * calib_.par_t3;

    const double temp = calib_.t_lin;
    const double temp2 = temp * temp;
    const double temp3 = temp2 * temp;
    const double pressure = static_cast<double>(raw_pressure_);
    const double pressure2 = pressure * pressure;
    const double pressure3 = pressure2 * pressure;

    const double partial_out1 =
        calib_.par_p5 +
        calib_.par_p6 * temp +
        calib_.par_p7 * temp2 +
        calib_.par_p8 * temp3;
    const double partial_out2 =
        pressure *
        (calib_.par_p1 +
         calib_.par_p2 * temp +
         calib_.par_p3 * temp2 +
         calib_.par_p4 * temp3);
    const double comp_press =
        partial_out1 +
        partial_out2 +
        pressure2 * (calib_.par_p9 + calib_.par_p10 * temp) +
        pressure3 * calib_.par_p11;

    temperature_c = static_cast<float>(temp);
    pressure_hpa = static_cast<float>(comp_press / 100.0);
    return isfinite(temperature_c) && isfinite(pressure_hpa);
}

const char *Bmp3xx::variantLabel() const {
    return bmp3xx_variant_label(variant_);
}

bool Bmp3xx::start() {
    addr_ = 0;
    variant_ = Variant::Unknown;

    if (detect(Config::BMP3XX_ADDR_PRIMARY)) {
        Logger::log(Logger::Info, "BMP3xx", "%s found at 0x%02X",
                    variantLabel(), static_cast<unsigned>(Config::BMP3XX_ADDR_PRIMARY));
    } else if (detect(Config::BMP3XX_ADDR_ALT)) {
        Logger::log(Logger::Info, "BMP3xx", "%s found at 0x%02X",
                    variantLabel(), static_cast<unsigned>(Config::BMP3XX_ADDR_ALT));
    } else {
        ok_ = false;
        return false;
    }

    if (!softReset()) {
        ok_ = false;
        return false;
    }
    if (!readCalibration()) {
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

void Bmp3xx::beginLateStart() {
    addr_ = 0;
    variant_ = Variant::Unknown;
    ok_ = false;
    late_start_due_ms_ = 0;
    late_start_deadline_ms_ = 0;
    late_start_phase_ = LateStartPhase::DetectPrimaryChip;
}

bool Bmp3xx::lateDetectRead(uint8_t addr,
                            uint8_t reg,
                            uint8_t reserved_mask,
                            LateStartPhase next_phase,
                            LateStartPhase fallback_phase) {
    addr_ = addr;
    uint8_t value = 0;
    if (!readU8(reg, value) || (value & reserved_mask) != 0U) {
        variant_ = Variant::Unknown;
        late_start_phase_ = fallback_phase;
        return false;
    }
    late_start_phase_ = next_phase;
    return true;
}

CooperativeStart::Result Bmp3xx::finishLateStart(bool success) {
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

CooperativeStart::Result Bmp3xx::pollLateStart(uint32_t now_ms) {
    auto detect_chip = [this](uint8_t addr,
                              LateStartPhase next_phase,
                              LateStartPhase fallback_phase) {
        addr_ = addr;
        uint8_t chip_id = 0;
        if (!readU8(Config::BMP3XX_REG_CHIP_ID, chip_id)) {
            variant_ = Variant::Unknown;
            late_start_phase_ = fallback_phase;
            return;
        }
        if (chip_id == Config::BMP3XX_CHIP_ID_BMP388) {
            variant_ = Variant::BMP388;
        } else if (chip_id == Config::BMP3XX_CHIP_ID_BMP390) {
            variant_ = Variant::BMP390;
        } else {
            variant_ = Variant::Unknown;
            late_start_phase_ = fallback_phase;
            return;
        }
        late_start_phase_ = next_phase;
    };

    switch (late_start_phase_) {
        case LateStartPhase::Idle:
            return CooperativeStart::Result::Idle;
        case LateStartPhase::DetectPrimaryChip:
            detect_chip(Config::BMP3XX_ADDR_PRIMARY,
                        LateStartPhase::DetectPrimaryErr,
                        LateStartPhase::DetectAltChip);
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectPrimaryErr:
            lateDetectRead(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_ERR,
                           kBmp3xxErrReservedMask, LateStartPhase::DetectPrimaryPower,
                           LateStartPhase::DetectAltChip);
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectPrimaryPower:
            lateDetectRead(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_PWR_CTRL,
                           kBmp3xxPwrCtrlReservedMask, LateStartPhase::DetectPrimaryOsr,
                           LateStartPhase::DetectAltChip);
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectPrimaryOsr:
            lateDetectRead(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_OSR,
                           kBmp3xxOsrReservedMask, LateStartPhase::DetectPrimaryOdr,
                           LateStartPhase::DetectAltChip);
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectPrimaryOdr:
            lateDetectRead(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_ODR,
                           kBmp3xxOdrReservedMask, LateStartPhase::SoftReset,
                           LateStartPhase::DetectAltChip);
            if (late_start_phase_ == LateStartPhase::SoftReset) {
                Logger::log(Logger::Info, "BMP3xx", "%s found at 0x%02X",
                            variantLabel(), static_cast<unsigned>(Config::BMP3XX_ADDR_PRIMARY));
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAltChip:
            detect_chip(Config::BMP3XX_ADDR_ALT,
                        LateStartPhase::DetectAltErr,
                        LateStartPhase::Idle);
            if (late_start_phase_ == LateStartPhase::Idle) {
                return finishLateStart(false);
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAltErr:
            lateDetectRead(Config::BMP3XX_ADDR_ALT, Config::BMP3XX_REG_ERR,
                           kBmp3xxErrReservedMask, LateStartPhase::DetectAltPower,
                           LateStartPhase::Idle);
            return late_start_phase_ == LateStartPhase::Idle
                       ? finishLateStart(false)
                       : CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAltPower:
            lateDetectRead(Config::BMP3XX_ADDR_ALT, Config::BMP3XX_REG_PWR_CTRL,
                           kBmp3xxPwrCtrlReservedMask, LateStartPhase::DetectAltOsr,
                           LateStartPhase::Idle);
            return late_start_phase_ == LateStartPhase::Idle
                       ? finishLateStart(false)
                       : CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAltOsr:
            lateDetectRead(Config::BMP3XX_ADDR_ALT, Config::BMP3XX_REG_OSR,
                           kBmp3xxOsrReservedMask, LateStartPhase::DetectAltOdr,
                           LateStartPhase::Idle);
            return late_start_phase_ == LateStartPhase::Idle
                       ? finishLateStart(false)
                       : CooperativeStart::Result::InProgress;
        case LateStartPhase::DetectAltOdr:
            lateDetectRead(Config::BMP3XX_ADDR_ALT, Config::BMP3XX_REG_ODR,
                           kBmp3xxOdrReservedMask, LateStartPhase::SoftReset,
                           LateStartPhase::Idle);
            if (late_start_phase_ == LateStartPhase::Idle) {
                return finishLateStart(false);
            }
            Logger::log(Logger::Info, "BMP3xx", "%s found at 0x%02X",
                        variantLabel(), static_cast<unsigned>(Config::BMP3XX_ADDR_ALT));
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::SoftReset:
            if (!writeU8(Config::BMP3XX_REG_CMD, Config::BMP3XX_CMD_SOFT_RESET)) {
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
            late_start_phase_ = LateStartPhase::WaitCmdReady;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitCmdReady: {
            uint8_t status = 0;
            if (readU8(Config::BMP3XX_REG_STATUS, status) &&
                (status & Config::BMP3XX_STATUS_CMD_RDY) != 0U) {
                late_start_phase_ = LateStartPhase::ReadCalibration;
                return CooperativeStart::Result::InProgress;
            }
            if (deadlineReached(now_ms, late_start_deadline_ms_)) {
                return finishLateStart(false);
            }
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::ReadCalibration:
            if (!readCalibration()) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteOsr;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteOsr: {
            const uint8_t osr =
                static_cast<uint8_t>(Config::BMP3XX_PRESS_OS_4X & 0x07) |
                static_cast<uint8_t>((Config::BMP3XX_TEMP_OS_2X & 0x07) << 3);
            if (!writeU8(Config::BMP3XX_REG_OSR, osr)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteOdr;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::WriteOdr:
            if (!writeU8(Config::BMP3XX_REG_ODR, Config::BMP3XX_ODR_0P78_HZ & 0x1F)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteConfig;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteConfig: {
            const uint8_t config =
                static_cast<uint8_t>((Config::BMP3XX_IIR_BYPASS & 0x07) << 1);
            if (!writeU8(Config::BMP3XX_REG_CONFIG, config)) {
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WritePower;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::WritePower: {
            const uint8_t power =
                static_cast<uint8_t>(Config::BMP3XX_PRESS_EN |
                                     Config::BMP3XX_TEMP_EN |
                                     ((Config::BMP3XX_MODE_NORMAL & 0x03) << 4));
            if (!writeU8(Config::BMP3XX_REG_PWR_CTRL, power)) {
                return finishLateStart(false);
            }
            late_start_due_ms_ = millis() + 2U;
            late_start_phase_ = LateStartPhase::WaitConfig;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::WaitConfig:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            late_start_phase_ = LateStartPhase::ReadError;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::ReadError: {
            uint8_t err = 0;
            if (!readU8(Config::BMP3XX_REG_ERR, err)) {
                return finishLateStart(false);
            }
            if ((err & (Config::BMP3XX_ERR_FATAL |
                        Config::BMP3XX_ERR_CMD |
                        Config::BMP3XX_ERR_CONF)) != 0U) {
                Logger::log(Logger::Warn, "BMP3xx", "%s config error 0x%02X",
                            variantLabel(), static_cast<unsigned>(err));
                return finishLateStart(false);
            }
            return finishLateStart(true);
        }
        default:
            return finishLateStart(false);
    }
}

void Bmp3xx::tryRecover(uint32_t now, const char *reason) {
    if (now - last_recover_ms_ < Config::BMP3XX_RECOVER_COOLDOWN_MS) {
        return;
    }
    last_recover_ms_ = now;
    Logger::log(Logger::Warn, "BMP3xx", "%s - reinit", reason);
    bool ok = start();
    if (ok) {
        Logger::log(Logger::Info, "BMP3xx", "%s recovery OK", variantLabel());
        ok_ = true;
        pressure_has_ = false;
        no_data_since_ms_ = 0;
        last_data_ms_ = 0;
        pressure_valid_ = false;
    } else {
        LOGW("BMP3xx", "recovery failed");
        ok_ = false;
    }
}

void Bmp3xx::handleNoData(uint32_t now, const char *reason) {
    if (no_data_since_ms_ == 0) {
        no_data_since_ms_ = now;
    }
    if (last_data_ms_ != 0 && (now - last_data_ms_) > Config::BMP3XX_STALE_MS) {
        pressure_valid_ = false;
    }
    if (now - no_data_since_ms_ >= Config::BMP3XX_RECOVER_MS) {
        tryRecover(now, reason);
    }
}

void Bmp3xx::poll() {
    uint32_t now = millis();
    if (!ok_) {
        tryRecover(now, "not ready");
        return;
    }
    if (now - last_poll_ms_ < Config::BMP3XX_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    if (!dataReady()) {
        handleNoData(now, "data not ready");
        return;
    }
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
        pressure_filtered_ += Config::BMP3XX_PRESSURE_ALPHA *
                              (pressure_hpa - pressure_filtered_);
    }

    last_data_ms_ = now;
    pressure_valid_ = true;
    has_new_data_ = true;
}

bool Bmp3xx::takeNewData(float &pressure_hpa, float &temperature_c) {
    if (!has_new_data_) {
        return false;
    }
    pressure_hpa = pressure_filtered_;
    temperature_c = temperature_c_;
    has_new_data_ = false;
    return true;
}

void Bmp3xx::invalidate() {
    pressure_valid_ = false;
    has_new_data_ = false;
}
