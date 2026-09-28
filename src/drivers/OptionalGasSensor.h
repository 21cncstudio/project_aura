// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "drivers/DfrOptionalGasSensor.h"
#include "drivers/Sen0321.h"

// One optional-gas slot. Once detected, its source stays fixed until reboot.
class OptionalGasSensor {
public:
    using Type = DfrOptionalGasSensor::OptionalGasType;
    void begin();
    bool start();
    void poll();
    bool isPresent() const { return useSen0321() ? ozone_.isPresent() : legacy_.isPresent(); }
    bool isDataValid() const { return useSen0321() ? ozone_.isDataValid() : legacy_.isDataValid(); }
    bool isWarmupActive() const { return useSen0321() ? ozone_.isWarmupActive() : legacy_.isWarmupActive(); }
    float ppm() const { return useSen0321() ? ozone_.ppm() : legacy_.ppm(); }
    uint8_t ppmDecimals() const { return useSen0321() ? 3 : legacy_.ppmDecimals(); }
    uint32_t lastDataMs() const { return useSen0321() ? ozone_.lastDataMs() : legacy_.lastDataMs(); }
    Type optionalGasType() const { return useSen0321() ? Type::O3_SEN0321 : legacy_.optionalGasType(); }
    const char *label() const { return useSen0321() ? "SEN0321 O3" : legacy_.label(); }
    uint8_t address() const { return useSen0321() ? Sen0321::Address : legacy_.address(); }
    void invalidate() { if (useSen0321()) ozone_.invalidate(); else legacy_.invalidate(); }
private:
    enum class Source : uint8_t { None, Legacy, Sen0321 };
    bool useSen0321() const { return source_ == Source::Sen0321; }
    Source source_ = Source::None;
    DfrOptionalGasSensor legacy_;
    Sen0321 ozone_;
};
