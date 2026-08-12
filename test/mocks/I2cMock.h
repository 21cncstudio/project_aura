#pragma once

#include <cstddef>
#include <cstdint>

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
uint8_t getRegister(uint8_t addr, uint8_t reg);
uint32_t transactionCount();

} // namespace I2cMock
