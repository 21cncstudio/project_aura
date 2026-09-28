// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "drivers/OptionalGasSensor.h"
#include "core/Logger.h"

void OptionalGasSensor::begin() {
    source_ = Source::None;
    legacy_.begin();
    ozone_.begin();
}

bool OptionalGasSensor::start() {
    const bool legacy_present = legacy_.start();
    const bool ozone_present = ozone_.start(); // probe only, no mode writes
    if (legacy_present) {
        source_ = Source::Legacy;
        if (ozone_present)
            LOGW("Sensors", "two optional gas modules detected; using 0x75 until reboot, SEN0321 ignored");
    } else if (ozone_present) source_ = Source::Sen0321;
    return source_ != Source::None;
}

void OptionalGasSensor::poll() {
    if (source_ == Source::Legacy) { legacy_.poll(); return; }
    if (source_ == Source::Sen0321) { ozone_.poll(); return; }
    // Permit delayed startup, but never switch a source already used by history.
    legacy_.poll();
    if (legacy_.isPresent()) { source_ = Source::Legacy; return; }
    ozone_.poll();
    if (ozone_.isPresent()) source_ = Source::Sen0321;
}
