#include "I2cMock.h"

#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "driver/i2c.h"
#include "ArduinoMock.h"

struct MockI2cCmd {
    bool has_address = false;
    uint8_t addr = 0;
    std::vector<uint8_t> payload;
};

namespace {

struct DeviceState {
    bool present = false;
    uint32_t address_only_probe_count = 0;
    std::array<uint8_t, 256> regs{};
    std::array<bool, 256> read_fail{};
    std::array<uint32_t, 256> read_call_count{};
    std::array<uint32_t, 256> read_fail_on_call{};
    std::array<bool, 256> write_fail{};
    std::unordered_set<uint16_t> failing_cmds;
    std::unordered_map<uint16_t, std::vector<uint8_t>> cmd_reads;
    std::unordered_map<uint16_t, uint32_t> sensor_cmd_counts;
    uint16_t read_wrap_last_reg = 0xFF;
    uint16_t last_sensor_cmd = 0;
    bool has_last_sensor_cmd = false;
};

std::array<DeviceState, 256> g_devices{};
uint32_t g_command_advance_ms = 0;
uint32_t g_transaction_count = 0;
std::array<uint32_t, 2> g_port_transaction_counts{};
i2c_port_t g_last_transaction_port = -1;
uint32_t g_parameter_config_count = 0;
uint32_t g_driver_install_count = 0;
i2c_port_t g_configured_port = -1;
i2c_port_t g_installed_port = -1;
i2c_config_t g_configured_config{};
esp_err_t g_parameter_config_result = ESP_OK;
esp_err_t g_driver_install_result = ESP_OK;

DeviceState &device(uint8_t addr) {
    return g_devices[addr];
}

void noteTransaction(i2c_port_t port) {
    ++g_transaction_count;
    g_last_transaction_port = port;
    if (port >= 0 && static_cast<size_t>(port) < g_port_transaction_counts.size()) {
        ++g_port_transaction_counts[static_cast<size_t>(port)];
    }
    advanceMillis(g_command_advance_ms);
}

void noteSensorCommand(DeviceState &state, uint16_t command) {
    state.last_sensor_cmd = command;
    state.has_last_sensor_cmd = true;
    ++state.sensor_cmd_counts[command];
}

} // namespace

namespace I2cMock {

void reset() {
    g_devices = {};
    g_command_advance_ms = 0;
    g_transaction_count = 0;
    g_port_transaction_counts = {};
    g_last_transaction_port = -1;
    g_parameter_config_count = 0;
    g_driver_install_count = 0;
    g_configured_port = -1;
    g_installed_port = -1;
    g_configured_config = {};
    g_parameter_config_result = ESP_OK;
    g_driver_install_result = ESP_OK;
}

void setDevicePresent(uint8_t addr, bool present) {
    device(addr).present = present;
}

void setCommandFailure(uint8_t addr, uint16_t cmd, bool fail) {
    if (fail) {
        device(addr).failing_cmds.insert(cmd);
    } else {
        device(addr).failing_cmds.erase(cmd);
    }
}

void setCommandRead(uint8_t addr, uint16_t cmd, const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        device(addr).cmd_reads.erase(cmd);
        return;
    }
    device(addr).cmd_reads[cmd] = std::vector<uint8_t>(data, data + len);
}

void setRegister(uint8_t addr, uint8_t reg, uint8_t value) {
    device(addr).regs[reg] = value;
}

void setRegisters(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len) {
    if (!data) {
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        device(addr).regs[static_cast<uint8_t>(reg + i)] = data[i];
    }
}

void setReadWrap(uint8_t addr, uint8_t last_reg) {
    device(addr).read_wrap_last_reg = last_reg;
}

void setReadFailure(uint8_t addr, uint8_t reg, bool fail) {
    device(addr).read_fail[reg] = fail;
}

void setCommandAdvanceMs(uint32_t advance_ms) {
    g_command_advance_ms = advance_ms;
}

void setReadFailureOnCall(uint8_t addr, uint8_t reg, uint32_t call_number) {
    device(addr).read_call_count[reg] = 0;
    device(addr).read_fail_on_call[reg] = call_number;
}

void setWriteFailure(uint8_t addr, uint8_t reg, bool fail) {
    device(addr).write_fail[reg] = fail;
}

void setParameterConfigResult(esp_err_t result) {
    g_parameter_config_result = result;
}

void setDriverInstallResult(esp_err_t result) {
    g_driver_install_result = result;
}

uint8_t getRegister(uint8_t addr, uint8_t reg) {
    return device(addr).regs[reg];
}

uint32_t addressOnlyProbeCount(uint8_t addr) {
    return device(addr).address_only_probe_count;
}

uint32_t transactionCount() {
    return g_transaction_count;
}

uint32_t transactionCount(i2c_port_t port) {
    if (port < 0 || static_cast<size_t>(port) >= g_port_transaction_counts.size()) {
        return 0;
    }
    return g_port_transaction_counts[static_cast<size_t>(port)];
}

i2c_port_t lastTransactionPort() {
    return g_last_transaction_port;
}

uint32_t parameterConfigCount() {
    return g_parameter_config_count;
}

uint32_t driverInstallCount() {
    return g_driver_install_count;
}

i2c_port_t configuredPort() {
    return g_configured_port;
}

i2c_port_t installedPort() {
    return g_installed_port;
}

const i2c_config_t &configuredConfig() {
    return g_configured_config;
}

uint32_t sensorCommandCount(uint8_t addr, uint16_t cmd) {
    const DeviceState &state = device(addr);
    const auto it = state.sensor_cmd_counts.find(cmd);
    return it == state.sensor_cmd_counts.end() ? 0U : it->second;
}

} // namespace I2cMock

i2c_cmd_handle_t i2c_cmd_link_create() {
    return new MockI2cCmd();
}

void i2c_cmd_link_delete(i2c_cmd_handle_t cmd) {
    delete cmd;
}

esp_err_t i2c_master_start(i2c_cmd_handle_t cmd) {
    return cmd ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd) {
    return cmd ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool) {
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cmd->has_address) {
        cmd->has_address = true;
        cmd->addr = static_cast<uint8_t>(data >> 1);
        return ESP_OK;
    }
    cmd->payload.push_back(data);
    return ESP_OK;
}

esp_err_t i2c_master_write(i2c_cmd_handle_t cmd, const uint8_t *data, size_t data_len, bool) {
    if (!cmd || (!data && data_len != 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data && data_len > 0) {
        cmd->payload.insert(cmd->payload.end(), data, data + data_len);
    }
    return ESP_OK;
}

esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *config) {
    ++g_parameter_config_count;
    g_configured_port = port;
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    g_configured_config = *config;
    return g_parameter_config_result;
}

esp_err_t i2c_driver_install(i2c_port_t port,
                             i2c_mode_t,
                             size_t,
                             size_t,
                             int) {
    ++g_driver_install_count;
    g_installed_port = port;
    return g_driver_install_result;
}

esp_err_t i2c_master_cmd_begin(i2c_port_t port, i2c_cmd_handle_t cmd, TickType_t) {
    noteTransaction(port);
    if (!cmd || !cmd->has_address) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cmd->payload.empty()) {
        ++device(cmd->addr).address_only_probe_count;
    }
    if (!device(cmd->addr).present) {
        return ESP_FAIL;
    }
    if (cmd->payload.size() >= 2) {
        const uint16_t sensor_cmd =
            (static_cast<uint16_t>(cmd->payload[0]) << 8) | cmd->payload[1];
        noteSensorCommand(device(cmd->addr), sensor_cmd);
        if (device(cmd->addr).failing_cmds.count(sensor_cmd) != 0) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t i2c_master_write_read_device(i2c_port_t port,
                                       uint8_t addr,
                                       const uint8_t *write_buffer,
                                       size_t write_size,
                                       uint8_t *read_buffer,
                                       size_t read_size,
                                       TickType_t) {
    noteTransaction(port);
    if (!device(addr).present || !write_buffer || write_size == 0 ||
        !read_buffer || read_size == 0) {
        return ESP_FAIL;
    }
    const uint8_t reg = write_buffer[0];
    DeviceState &state = device(addr);
    ++state.read_call_count[reg];
    if (state.read_fail[reg] ||
        (state.read_fail_on_call[reg] != 0U &&
         state.read_call_count[reg] == state.read_fail_on_call[reg])) {
        return ESP_FAIL;
    }
    if (state.has_last_sensor_cmd) {
        auto it = state.cmd_reads.find(state.last_sensor_cmd);
        if (it != state.cmd_reads.end()) {
            if (it->second.size() < read_size) {
                return ESP_FAIL;
            }
            for (size_t i = 0; i < read_size; ++i) {
                read_buffer[i] = it->second[i];
            }
            return ESP_OK;
        }
    }
    uint8_t current_reg = reg;
    for (size_t i = 0; i < read_size; ++i) {
        read_buffer[i] = device(addr).regs[current_reg];
        if (device(addr).read_wrap_last_reg != 0xFF &&
            current_reg == static_cast<uint8_t>(device(addr).read_wrap_last_reg)) {
            current_reg = 0x00;
        } else {
            current_reg = static_cast<uint8_t>(current_reg + 1);
        }
    }
    return ESP_OK;
}

esp_err_t i2c_master_write_to_device(i2c_port_t port,
                                     uint8_t addr,
                                     const uint8_t *write_buffer,
                                     size_t write_size,
                                     TickType_t) {
    noteTransaction(port);
    if (!write_buffer || write_size == 0) {
        return ESP_FAIL;
    }
    const uint8_t reg = write_buffer[0];
    if (write_size >= 4 && write_buffer[0] == 0x00 && write_buffer[1] == 0xFF &&
        write_buffer[2] == 0x01) {
        // Count command attempts before emulating NACK/write failure so safety
        // tests cannot pass merely because a hazardous write was rejected.
        noteSensorCommand(device(addr), write_buffer[3]);
    }
    if (!device(addr).present) {
        return ESP_FAIL;
    }
    if (device(addr).write_fail[reg]) {
        return ESP_FAIL;
    }
    if (write_size == 1) {
        return ESP_OK;
    }
    for (size_t i = 1; i < write_size; ++i) {
        device(addr).regs[static_cast<uint8_t>(reg + i - 1)] = write_buffer[i];
    }
    return ESP_OK;
}

esp_err_t i2c_master_read_from_device(i2c_port_t port,
                                      uint8_t addr,
                                      uint8_t *read_buffer,
                                      size_t read_size,
                                      TickType_t) {
    noteTransaction(port);
    if (!device(addr).present || !read_buffer || read_size == 0) {
        return ESP_FAIL;
    }
    if (device(addr).has_last_sensor_cmd) {
        auto it = device(addr).cmd_reads.find(device(addr).last_sensor_cmd);
        if (it != device(addr).cmd_reads.end()) {
            if (it->second.size() < read_size) {
                return ESP_FAIL;
            }
            for (size_t i = 0; i < read_size; ++i) {
                read_buffer[i] = it->second[i];
            }
            return ESP_OK;
        }
    }
    for (size_t i = 0; i < read_size; ++i) {
        read_buffer[i] = 0;
    }
    return ESP_OK;
}
