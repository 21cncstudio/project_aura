#include "OtaPlatformMock.h"

#include <algorithm>

#include "Update.h"
#include "WiFi.h"
#include "esp_ota_ops.h"
#include "core/AppVersion.h"
#include "core/OtaRollback.h"

namespace OtaPlatformMock {

State &state() {
    static State platform;
    return platform;
}

void reset() {
    state() = State{};
}

}  // namespace OtaPlatformMock

UpdateClass Update;
WiFiClass WiFi;

bool UpdateClass::begin(size_t size, int command, int, uint8_t, const char *) {
    auto &mock = OtaPlatformMock::state().update;
    ++mock.begin_calls;
    mock.begin_size = size;
    mock.begin_command = command;
    mock.running = mock.begin_result;
    return mock.begin_result;
}

size_t UpdateClass::write(uint8_t *data, size_t length) {
    auto &mock = OtaPlatformMock::state().update;
    ++mock.write_calls;
    mock.write_sizes.push_back(length);
    if (!mock.running || (!data && length != 0)) {
        return 0;
    }
    const bool fail = mock.fail_write_call != 0 && mock.write_calls == mock.fail_write_call;
    const size_t accepted = fail ? std::min(mock.failed_write_bytes, length) : length;
    if (accepted != 0) {
        mock.bytes.insert(mock.bytes.end(), data, data + accepted);
    }
    if (fail && mock.self_abort_on_write_failure) {
        mock.running = false;
    }
    return accepted;
}

bool UpdateClass::end(bool even_if_remaining) {
    auto &mock = OtaPlatformMock::state().update;
    ++mock.end_calls;
    mock.end_even_if_remaining = even_if_remaining;
    if (!mock.running) {
        return false;
    }
    if (mock.end_result || mock.self_abort_on_end_failure) {
        mock.running = false;
    }
    return mock.end_result;
}

void UpdateClass::abort() {
    auto &mock = OtaPlatformMock::state().update;
    ++mock.abort_calls;
    mock.running = false;
    // Like the platform, abort may replace its own error. The handler must have
    // captured the original failure before invoking this cleanup.
    mock.error_text = "aborted by cleanup";
}

bool UpdateClass::isRunning() {
    return OtaPlatformMock::state().update.running;
}

const char *UpdateClass::errorString() {
    return OtaPlatformMock::state().update.error_text.c_str();
}

wl_status_t WiFiClass::status() {
    return OtaPlatformMock::state().wifi_connected ? WL_CONNECTED : WL_DISCONNECTED;
}

int WiFiClass::RSSI() {
    return OtaPlatformMock::state().wifi_rssi;
}

const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *) {
    if (!OtaPlatformMock::state().partition_available) {
        return nullptr;
    }
    static esp_partition_t partition{};
    partition.size = OtaPlatformMock::state().partition_size;
    return &partition;
}

namespace AppVersion {
const char *hardwareTarget() {
    return OtaPlatformMock::state().hardware_target.c_str();
}
const char *firmwareFlavor() {
    return OtaPlatformMock::state().firmware_flavor.c_str();
}
}  // namespace AppVersion

namespace OtaRollback {
bool isPendingVerify() {
    return OtaPlatformMock::state().boot_pending_verify;
}
}  // namespace OtaRollback
