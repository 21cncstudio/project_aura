#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/i2c.h"

namespace I2cMock {

void reset();
void setDevicePresent(uint8_t addr, bool present);
void setCommandFailure(uint8_t addr, uint16_t cmd, bool fail);
void setCommandAdvanceMs(uint32_t advance_ms);
void setCommandRead(uint8_t addr, uint16_t cmd, const uint8_t *data, size_t len);
void setRegister(uint8_t addr, uint8_t reg, uint8_t value);
void setRegisters(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);
void setReadWrap(uint8_t addr, uint8_t last_reg);
void setReadFailure(uint8_t addr, uint8_t reg, bool fail);
void setReadFailureOnCall(uint8_t addr, uint8_t reg, uint32_t call_number);
void setWriteFailure(uint8_t addr, uint8_t reg, bool fail);
void setParameterConfigResult(esp_err_t result);
void setDriverInstallResult(esp_err_t result);
uint8_t getRegister(uint8_t addr, uint8_t reg);
uint32_t addressOnlyProbeCount(uint8_t addr);
uint32_t transactionCount();
uint32_t transactionCount(i2c_port_t port);
i2c_port_t lastTransactionPort();
uint32_t parameterConfigCount();
uint32_t driverInstallCount();
i2c_port_t configuredPort();
i2c_port_t installedPort();
const i2c_config_t &configuredConfig();
uint32_t sensorCommandCount(uint8_t addr, uint16_t cmd);

} // namespace I2cMock
