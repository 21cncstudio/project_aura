// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Gt911Hardware.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_display_panel.hpp>
#include <esp_err.h>

#include "config/AppConfig.h"
#include "core/BootDiagnostics.h"
#include "core/Gt911AddressSelect.h"
#include "core/Gt911DiagnosticPolicy.h"
#include "core/Logger.h"
#include "esp_panel_board_custom_conf.h"

namespace Gt911Hardware {
namespace {

struct Context {
    esp_expander::Base *expander = nullptr;
    gpio_num_t int_gpio = GPIO_NUM_NC;
    int reset_exio = -1;
};

bool setIntOutput(void *opaque) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_set_direction(ctx->int_gpio, GPIO_MODE_OUTPUT) == ESP_OK;
}

bool setIntLevel(void *opaque, bool high) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_set_level(ctx->int_gpio, high ? 1 : 0) == ESP_OK;
}

bool setResetLevel(void *opaque, bool high) {
    auto *ctx = static_cast<Context *>(opaque);
    return ctx->expander != nullptr &&
           ctx->expander->digitalWrite(ctx->reset_exio, high ? 1 : 0);
}

bool releaseInt(void *opaque) {
    auto *ctx = static_cast<Context *>(opaque);
    return gpio_reset_pin(ctx->int_gpio) == ESP_OK;
}

void delayMs(void *, uint32_t delay_ms) {
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

#if AURA_GT911_STARTUP_DIAGNOSTICS
static_assert(Config::SENSOR_I2C_SEPARATE &&
                  Config::SENSOR_I2C_PORT != Config::I2C_PORT,
              "Dual-address GT911 reads must not reach the external SFA30 bus");

Logger::Level diagnosticLogLevel(Gt911DiagnosticPolicy::Severity severity) {
    return severity == Gt911DiagnosticPolicy::Severity::Info ? Logger::Info : Logger::Warn;
}

Gt911DiagnosticPolicy::ReadResult diagnosticReadResult(esp_err_t error) {
    using Gt911DiagnosticPolicy::ReadResult;
    switch (error) {
        case ESP_OK: return ReadResult::Ok;
        case ESP_FAIL: return ReadResult::GenericFailure;
        case ESP_ERR_TIMEOUT: return ReadResult::Timeout;
        default: return ReadResult::OtherFailure;
    }
}

bool logDiagnosticAddress(uint8_t address,
                          Gt911DiagnosticPolicy::ProbeRole role,
                          bool configured_healthy,
                          Gt911StartupDiagnostics::Probe &snapshot) {
    const uint8_t product_reg[] = {0x81, 0x40};
    uint8_t product_id[3] = {};
    const esp_err_t error = i2c_master_write_read_device(
        Config::I2C_PORT, address, product_reg, sizeof(product_reg),
        product_id, sizeof(product_id), pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS));
    const bool valid = error == ESP_OK && product_id[0] == '9' &&
                       product_id[1] == '1' && product_id[2] == '1';
    const auto identity_result = diagnosticReadResult(error);
    const auto identity_severity = Gt911DiagnosticPolicy::identitySeverity(
        role, identity_result, valid, configured_healthy);
    snapshot.role = role;
    snapshot.address = address;
    snapshot.port = static_cast<int>(Config::I2C_PORT);
    snapshot.identity_attempted = true;
    snapshot.identity_error = static_cast<int32_t>(error);
    snapshot.identity_result = identity_result;
    snapshot.product_id[0] = product_id[0];
    snapshot.product_id[1] = product_id[1];
    snapshot.product_id[2] = product_id[2];
    snapshot.identity_valid = valid;
    snapshot.identity_severity = identity_severity;
    Logger::log(diagnosticLogLevel(identity_severity),
         "GT911DIAG",
         "post-reset port=%d addr=0x%02X reg=0x8140 err=%d(%s) id=%02X,%02X,%02X valid=%u",
         static_cast<int>(Config::I2C_PORT), address, static_cast<int>(error),
         esp_err_to_name(error), product_id[0], product_id[1], product_id[2],
         valid ? 1U : 0U);
    if (!valid) {
        return false;
    }

    const uint8_t config_reg[] = {0x80, 0x47};
    uint8_t config_version = 0;
    const esp_err_t config_error = i2c_master_write_read_device(
        Config::I2C_PORT, address, config_reg, sizeof(config_reg),
        &config_version, 1U, pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS));
    const auto config_result = diagnosticReadResult(config_error);
    const auto config_severity =
        Gt911DiagnosticPolicy::configSeverity(role, config_result);
    snapshot.config_attempted = true;
    snapshot.config_error = static_cast<int32_t>(config_error);
    snapshot.config_result = config_result;
    snapshot.config_version = config_version;
    snapshot.config_severity = config_severity;
    Logger::log(diagnosticLogLevel(config_severity),
         "GT911DIAG", "post-reset addr=0x%02X reg=0x8047 err=%d(%s) config=0x%02X",
         address, static_cast<int>(config_error), esp_err_to_name(config_error),
         config_version);
    return Gt911DiagnosticPolicy::configuredAddressHealthy(valid, config_result);
}
#endif

} // namespace

bool selectConfiguredAddress(esp_panel::board::Board *board) {
    if (board == nullptr || board->getIO_Expander() == nullptr) {
        LOGE("GT911", "address select unavailable: board/expander missing");
        return false;
    }

    Context context{
        board->getIO_Expander()->getBase(),
        static_cast<gpio_num_t>(ESP_PANEL_BOARD_TOUCH_INT_IO),
        1,
    };
    if (context.expander == nullptr || context.int_gpio == GPIO_NUM_NC) {
        LOGE("GT911", "address select unavailable: invalid reset/INT path");
        return false;
    }

    const Gt911AddressSelect::Ops ops{
        &context,
        setIntOutput,
        setIntLevel,
        setResetLevel,
        releaseInt,
        delayMs,
    };
    const Gt911AddressSelect::Result result =
        Gt911AddressSelect::selectAddress(ops, ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS);
#if AURA_GT911_STARTUP_DIAGNOSTICS
    Gt911StartupDiagnostics::Snapshot diagnostics{};
    diagnostics.captured = true;
    diagnostics.requested_address = ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS;
    diagnostics.int_gpio = static_cast<int>(context.int_gpio);
    diagnostics.int_level_high = ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS == 0x14;
    diagnostics.reset_exio = context.reset_exio;
    // The sequence reports commanded levels only. Reading the pins here would
    // add hardware transactions and change the diagnostic candidate.
    diagnostics.pin_levels_measured = false;
    diagnostics.selection_succeeded = result.ok();
    diagnostics.selection_failure = result.failure;
    diagnostics.selection_severity =
        Gt911DiagnosticPolicy::selectionSeverity(result.ok());
    Logger::log(diagnosticLogLevel(diagnostics.selection_severity),
         "GT911DIAG",
         "requested=0x%02X INT=%s RESET=EXIO1 sequence_failure=%s; pin levels not measured",
         ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS,
         ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS == 0x14 ? "HIGH" : "LOW",
         Gt911AddressSelect::failureText(result.failure));
    if (!result.ok() && !BootDiagnostics::state.gt911_startup.captured) {
        BootDiagnostics::state.gt911_startup = diagnostics;
    }
#endif
    if (!result.ok()) {
        LOGE("GT911",
             "address select failed at %s",
             Gt911AddressSelect::failureText(result.failure));
        return false;
    }
#if AURA_GT911_STARTUP_DIAGNOSTICS
    // Read only known GT911 identity registers, before vendor touch begin and
    // board teardown. No scan, fallback address change, or second reset.
    Gt911StartupDiagnostics::Probe &configured_probe =
        diagnostics.probes[diagnostics.probe_count++];
    const bool configured_healthy = logDiagnosticAddress(
        ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS,
        Gt911DiagnosticPolicy::ProbeRole::Configured,
        false,
        configured_probe);
    Gt911StartupDiagnostics::Probe &opposite_probe =
        diagnostics.probes[diagnostics.probe_count++];
    (void)logDiagnosticAddress(
        ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS == 0x14 ? 0x5D : 0x14,
        Gt911DiagnosticPolicy::ProbeRole::Opposite,
        configured_healthy,
        opposite_probe);
    if (!BootDiagnostics::state.gt911_startup.captured) {
        BootDiagnostics::state.gt911_startup = diagnostics;
    }
#endif
    return true;
}

} // namespace Gt911Hardware
