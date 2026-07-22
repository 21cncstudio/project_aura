// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/DfrOptionalGasSensor.h"

#include "config/AppConfig.h"

namespace {

constexpr uint8_t kOptionalGasTypes[] = {
    Config::DFR_GAS_TYPE_NH3,
    Config::DFR_GAS_TYPE_O3,
    Config::DFR_GAS_TYPE_SO2,
    Config::DFR_GAS_TYPE_NO2,
    Config::DFR_GAS_TYPE_H2S,
    Config::DFR_GAS_TYPE_O2,
};

} // namespace

DfrOptionalGasSensor::DfrOptionalGasSensor()
    : DfrMultiGasSensor({
          "DFR-GAS",
          "DFR Optional Gas",
          Config::DFR_OPTIONAL_GAS_ADDR,
          0,
          0.0f,
          0.0f,
          kOptionalGasTypes,
          sizeof(kOptionalGasTypes) / sizeof(kOptionalGasTypes[0]),
      }) {}

void DfrOptionalGasSensor::poll() {
    DfrMultiGasSensor::poll();

    const OptionalGasType type = optionalGasType();
    if (type == OptionalGasType::None) {
        return;
    }
    clampPpm(minPpmForType(type), maxPpmForType(type));
}

DfrOptionalGasSensor::OptionalGasType DfrOptionalGasSensor::optionalGasType() const {
    switch (gasType()) {
        case GasType::NH3:
            return OptionalGasType::NH3;
        case GasType::SO2:
            return OptionalGasType::SO2;
        case GasType::NO2:
            return OptionalGasType::NO2;
        case GasType::H2S:
            return OptionalGasType::H2S;
        case GasType::O3:
            return OptionalGasType::O3;
        case GasType::O2:
            return OptionalGasType::O2;
        case GasType::None:
        case GasType::CO:
        case GasType::Unknown:
        default:
            return OptionalGasType::None;
    }
}

const char *DfrOptionalGasSensor::optionalGasLabel() const {
    return optionalGasLabel(optionalGasType());
}

const char *DfrOptionalGasSensor::optionalGasLabel(OptionalGasType type) {
    switch (type) {
        case OptionalGasType::NH3:
            return "NH3";
        case OptionalGasType::SO2:
            return "SO2";
        case OptionalGasType::NO2:
            return "NO2";
        case OptionalGasType::H2S:
            return "H2S";
        case OptionalGasType::O3:
            return "O3";
        case OptionalGasType::O2:
            return "O2";
        case OptionalGasType::None:
        default:
            return "None";
    }
}

const char *DfrOptionalGasSensor::unitForType(OptionalGasType type) {
    return type == OptionalGasType::O2 ? "%Vol" : "ppm";
}

float DfrOptionalGasSensor::minPpmForType(OptionalGasType type) {
    switch (type) {
        case OptionalGasType::NH3:
            return Config::SEN0469_NH3_MIN_PPM;
        case OptionalGasType::H2S:
            return Config::SEN0467_H2S_MIN_PPM;
        case OptionalGasType::O3:
            return Config::SEN0472_O3_MIN_PPM;
        case OptionalGasType::SO2:
            return Config::SEN0470_SO2_MIN_PPM;
        case OptionalGasType::NO2:
            return Config::SEN0471_NO2_MIN_PPM;
        case OptionalGasType::O2:
            return Config::SEN0465_O2_MIN_PERCENT_VOL;
        case OptionalGasType::None:
        default:
            return 0.0f;
    }
}

float DfrOptionalGasSensor::maxPpmForType(OptionalGasType type) {
    switch (type) {
        case OptionalGasType::NH3:
            return Config::SEN0469_NH3_MAX_PPM;
        case OptionalGasType::H2S:
            return Config::SEN0467_H2S_MAX_PPM;
        case OptionalGasType::O3:
            return Config::SEN0472_O3_MAX_PPM;
        case OptionalGasType::SO2:
            return Config::SEN0470_SO2_MAX_PPM;
        case OptionalGasType::NO2:
            return Config::SEN0471_NO2_MAX_PPM;
        case OptionalGasType::O2:
            return Config::SEN0465_O2_MAX_PERCENT_VOL;
        case OptionalGasType::None:
        default:
            return 0.0f;
    }
}
