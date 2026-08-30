// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace I2cFaultDomainPolicy {

constexpr bool sensorRuntimeReady(bool sensor_bus_separate,
                                  bool panel_bus_ready,
                                  bool sensor_host_ready) {
    return sensor_bus_separate ? sensor_host_ready : panel_bus_ready;
}

constexpr bool panelFailureDisablesSensorDomain(bool sensor_bus_separate) {
    return !sensor_bus_separate;
}

constexpr bool sensorBusExclusiveForShutdown(bool sensor_bus_separate,
                                             bool panel_owners_drained,
                                             bool sensor_owners_drained) {
    return sensor_owners_drained &&
           (sensor_bus_separate || panel_owners_drained);
}

constexpr bool lvglPauseSatisfiedForSensorOutput(bool sensor_bus_separate,
                                                 bool lvgl_quiesced) {
    return sensor_bus_separate || lvgl_quiesced;
}

} // namespace I2cFaultDomainPolicy
