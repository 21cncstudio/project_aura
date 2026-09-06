#include <unity.h>

#include <ArduinoJson.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ArduinoMock.h"
#include "OtaPlatformMock.h"
#include "Update.h"
#include "core/OtaImageIdentity.h"
#include "web/WebOtaHandlers.h"

namespace {

constexpr size_t kFixtureSize = 512;
constexpr size_t kMetadataOffset = 288;
constexpr size_t kPrefixSize = 384;
constexpr uint32_t kUploadTimeoutMs = 5000;
constexpr uint32_t kRestartDelayMs = 800;

void put16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

// Independent wire-format fixture, not a serialized production Descriptor.
// It has an identity-valid prefix and an opaque deterministic remainder.
// Full-image checksum/signature verification remains the mocked Update.end()
// responsibility; this suite proves handler gating and byte accounting only.
std::vector<uint8_t> image_for(const char *target,
                               const char *flavor = "production") {
    std::vector<uint8_t> bytes(kFixtureSize, 0);
    for (size_t i = kPrefixSize; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(31 * i + 7);
    }
    bytes[0] = 0xE9;
    bytes[1] = 1;
    put16(bytes, 12, 9);
    put32(bytes, 24, 0x3C000020);
    put32(bytes, 28, kFixtureSize - 32);
    put32(bytes, 32, 0xABCD5432);
    const char magic[16] = "AURA_OTA_TARGET";
    std::memcpy(bytes.data() + kMetadataOffset, magic, sizeof(magic));
    put16(bytes, kMetadataOffset + 16, 1);
    put16(bytes, kMetadataOffset + 18, 64);
    std::memcpy(bytes.data() + kMetadataOffset + 20, target, std::strlen(target));
    if (flavor) {
        const char flavor_magic[16] = "AURA_OTA_FLAVOR";
        std::memcpy(bytes.data() + kMetadataOffset + 64,
                    flavor_magic, sizeof(flavor_magic));
        put16(bytes, kMetadataOffset + 80, 1);
        put16(bytes, kMetadataOffset + 82, 32);
        std::memcpy(bytes.data() + kMetadataOffset + 84,
                    flavor, std::strlen(flavor));
    }
    return bytes;
}

class FakeRequest final : public WebRequest {
public:
    std::map<String, String> args;
    std::vector<std::pair<String, String>> headers;
    WebUpload current_upload;
    size_t received = 0;
    bool connected = true;
    bool rejected = false;
    bool stopped = false;
    uint32_t upload_deadline_ms = 0;
    size_t pending_body = 0;
    size_t drained = 0;
    size_t drain_limit = 0;
    uint32_t drain_timeout = 0;
    int response_status = 0;
    String response_type;
    String response;

    bool hasArg(const char *name) const override { return args.count(name) != 0; }
    String arg(const char *name) const override {
        const auto found = args.find(name);
        return found == args.end() ? "" : found->second;
    }
    String uri() const override { return "/api/ota"; }
    void sendHeader(const char *name, const String &value, bool = false) override {
        headers.emplace_back(name, value);
    }
    void send(int status, const char *type, const String &content) override {
        response_status = status;
        response_type = type;
        response = content;
    }
    void send(int status, const char *type, const char *content) override {
        send(status, type, String(content));
    }
    bool clientConnected() const override { return connected; }
    void setUploadDeadlineMs(uint32_t timeout_ms) override { upload_deadline_ms = timeout_ms; }
    void clearUploadDeadline() override { upload_deadline_ms = 0; }
    void rejectUpload() override { rejected = true; }
    bool uploadRejected() const override { return rejected; }
    size_t pendingRequestBodyBytes() const override { return pending_body; }
    size_t drainPendingRequestBody(size_t max_bytes, uint32_t max_time_ms) override {
        drain_limit = max_bytes;
        drain_timeout = max_time_ms;
        const size_t amount = std::min(pending_body, max_bytes);
        pending_body -= amount;
        drained += amount;
        return amount;
    }
    void stopClient() override { stopped = true; connected = false; }
    bool beginStreamResponse(int, const char *, size_t, bool = false) override { return false; }
    int32_t writeStreamChunk(const uint8_t *, size_t, int &error) override {
        error = 0;
        return -1;
    }
    bool waitUntilWritable(uint16_t, int &error) override { error = 0; return false; }
    void endStreamResponse() override {}
    WebUpload upload() override { return current_upload; }

    ArduinoJson::JsonDocument json() const {
        ArduinoJson::JsonDocument document;
        const auto error = deserializeJson(document, response);
        TEST_ASSERT_FALSE_MESSAGE(error, response.c_str());
        return document;
    }

    String header(const char *name) const {
        for (const auto &entry : headers) {
            if (entry.first == name) {
                return entry.second;
            }
        }
        return "";
    }
};

struct Harness;
Harness *current = nullptr;
uint32_t upload_timeout(size_t);
void disable_power_save();
void restore_power_save();
void arm_preflight(uint32_t);
void cancel_preflight(uint32_t);
void set_ui(WebUiBridge::FirmwareUpdateScreenMode, uint32_t);
void set_error(const String &, const char *);
bool try_begin_upload();
void end_upload();
void stop_scan();
OtaPhysicalConfirm::PrepareDecision prepare_confirm(size_t, bool, uint32_t);
OtaPhysicalConfirm::ConsumeDecision consume_confirm(size_t, bool, uint32_t);

struct Harness {
    FakeRequest request;
    WebHandlerContext context;
    WebOtaState state;
    OtaDeferredRestart::Controller restart;
    std::atomic<uint32_t> upload_confirm_id{0};
    OtaImageIdentity::PrefixValidator validator;
    OtaPhysicalConfirm::StateMachine physical_confirm;
    WebOtaHandlers::Runtime runtime;
    bool upload_locked = false;
    unsigned try_begin_calls = 0;
    unsigned end_upload_calls = 0;
    unsigned disable_power_save_calls = 0;
    unsigned restore_power_save_calls = 0;
    unsigned stop_scan_calls = 0;
    std::vector<uint32_t> armed;
    std::vector<uint32_t> cancelled;
    std::vector<std::pair<WebUiBridge::FirmwareUpdateScreenMode, uint32_t>> ui;

    explicit Harness(const char *target = "aura-aq-v1",
                     const char *flavor = "production")
        : runtime{context, state, restart, upload_confirm_id, validator} {
        OtaPlatformMock::reset();
        OtaPlatformMock::state().hardware_target = target;
        OtaPlatformMock::state().firmware_flavor = flavor;
        current = this;
        context.server = &request;
        context.wifi_stop_scan = stop_scan;
        runtime.deferred_restart_delay_ms = kRestartDelayMs;
        runtime.upload_timeout_ms = upload_timeout;
        runtime.disable_wifi_power_save_for_upload = disable_power_save;
        runtime.restore_wifi_power_save = restore_power_save;
        runtime.arm_preflight_ui = arm_preflight;
        runtime.cancel_preflight_ui = cancel_preflight;
        runtime.set_ui_screen = set_ui;
        runtime.set_error = set_error;
        runtime.try_begin_upload = try_begin_upload;
        runtime.end_upload = end_upload;
        runtime.prepare_physical_confirm = prepare_confirm;
        runtime.consume_physical_confirm = consume_confirm;
    }

    ~Harness() { current = nullptr; }

    uint32_t authorize(size_t size, const char *filename = "firmware.bin") {
        request = FakeRequest{};
        const auto decision = physical_confirm.prepare(size, false, 0, millis());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(OtaPhysicalConfirm::PrepareStatus::Required),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(physical_confirm.allowCurrent(decision.confirm_id, millis()));
        request.args["ota_size"] = std::to_string(size);
        request.args["ota_confirm_id"] = std::to_string(decision.confirm_id);
        request.current_upload.filename = filename;
        return decision.confirm_id;
    }

    uint32_t start(size_t size = kFixtureSize, const char *filename = "firmware.bin") {
        const uint32_t id = authorize(size, filename);
        request.current_upload.status = WebUploadStatus::Start;
        WebOtaHandlers::handleUpload(runtime, false);
        return id;
    }

    void write(std::vector<uint8_t> &image, size_t offset, size_t count) {
        TEST_ASSERT_TRUE(offset <= image.size() && count <= image.size() - offset);
        request.current_upload.status = WebUploadStatus::Write;
        request.current_upload.currentSize = count;
        request.current_upload.buf = image.data() + offset;
        request.received += count;
        request.current_upload.totalSize = request.received;
        WebOtaHandlers::handleUpload(runtime, upload_locked);
    }

    void finish() {
        request.current_upload.status = WebUploadStatus::End;
        request.current_upload.currentSize = 0;
        request.current_upload.buf = nullptr;
        request.current_upload.totalSize = request.received;
        WebOtaHandlers::handleUpload(runtime, upload_locked);
    }

    void abort(WebUploadAbortReason reason) {
        request.current_upload.status = WebUploadStatus::Aborted;
        request.current_upload.abort_reason = reason;
        request.current_upload.currentSize = 0;
        request.current_upload.buf = nullptr;
        WebOtaHandlers::handleUpload(runtime, upload_locked);
    }

    void respond() { WebOtaHandlers::handleUpdate(runtime, upload_locked); }
};

uint32_t upload_timeout(size_t) { return kUploadTimeoutMs; }
void disable_power_save() { ++current->disable_power_save_calls; }
void restore_power_save() { ++current->restore_power_save_calls; }
void arm_preflight(uint32_t id) { current->armed.push_back(id); }
void cancel_preflight(uint32_t id) { current->cancelled.push_back(id); }
void set_ui(WebUiBridge::FirmwareUpdateScreenMode mode, uint32_t id) {
    current->ui.emplace_back(mode, id);
}
void set_error(const String &error, const char *code) {
    current->state.setErrorOnce(error, millis(), code);
}
bool try_begin_upload() {
    ++current->try_begin_calls;
    if (current->upload_locked) {
        return false;
    }
    current->upload_locked = true;
    return true;
}
void end_upload() { current->upload_locked = false; ++current->end_upload_calls; }
void stop_scan() { ++current->stop_scan_calls; }
OtaPhysicalConfirm::PrepareDecision prepare_confirm(size_t size, bool has_id, uint32_t id) {
    return current->physical_confirm.prepare(size, has_id, id, millis());
}
OtaPhysicalConfirm::ConsumeDecision consume_confirm(size_t size, bool has_id, uint32_t id) {
    return current->physical_confirm.consumeForUpload(size, has_id, id, millis());
}

void assert_no_flash_calls() {
    const auto &update = OtaPlatformMock::state().update;
    TEST_ASSERT_EQUAL_UINT32(0, update.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.write_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.end_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.abort_calls);
    TEST_ASSERT_TRUE(update.bytes.empty());
    TEST_ASSERT_FALSE(update.running);
}

void assert_failure(Harness &h, const char *code, int status, uint32_t confirm_id,
                    size_t written = 0) {
    h.respond();
    const auto json = h.request.json();
    TEST_ASSERT_EQUAL_INT(status, h.request.response_status);
    TEST_ASSERT_FALSE(json["success"].as<bool>());
    TEST_ASSERT_FALSE(json["rebooting"].as<bool>());
    TEST_ASSERT_EQUAL_STRING(code, json["error_code"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(written, json["written"].as<uint32_t>());
    TEST_ASSERT_FALSE(h.state.snapshot().active);
    TEST_ASSERT_FALSE(h.state.snapshot().success);
    TEST_ASSERT_FALSE(h.state.snapshot().reboot_pending);
    TEST_ASSERT_FALSE(h.state.isBusy());
    TEST_ASSERT_FALSE(h.upload_locked);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    TEST_ASSERT_FALSE(h.restart.is_requested());
    TEST_ASSERT_EQUAL_UINT32(0, h.upload_confirm_id.load());
    TEST_ASSERT_TRUE(h.request.stopped);
    TEST_ASSERT_EQUAL_STRING("close", h.request.header("Connection").c_str());
    TEST_ASSERT_FALSE(h.ui.empty());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebUiBridge::FirmwareUpdateScreenMode::Hidden),
                         static_cast<int>(h.ui.back().first));
    TEST_ASSERT_EQUAL_UINT32(confirm_id, h.ui.back().second);
}

void assert_success(Harness &h, const std::vector<uint8_t> &image) {
    const auto &update = OtaPlatformMock::state().update;
    TEST_ASSERT_EQUAL_UINT32(1, update.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(image.size(), update.begin_size);
    TEST_ASSERT_EQUAL_INT(U_FLASH, update.begin_command);
    TEST_ASSERT_EQUAL_UINT32(image.size(), update.bytes.size());
    TEST_ASSERT_EQUAL_MEMORY(image.data(), update.bytes.data(), image.size());
    TEST_ASSERT_EQUAL_UINT32(image.size(), h.state.snapshot().written_size);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    h.finish();
    TEST_ASSERT_EQUAL_UINT32(1, update.end_calls);
    TEST_ASSERT_TRUE(update.end_even_if_remaining);
    TEST_ASSERT_TRUE(h.state.snapshot().success);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    h.respond();
    const auto json = h.request.json();
    TEST_ASSERT_EQUAL_INT(200, h.request.response_status);
    TEST_ASSERT_TRUE(json["success"].as<bool>());
    TEST_ASSERT_TRUE(json["rebooting"].as<bool>());
    TEST_ASSERT_TRUE(json["error_code"].isNull());
    TEST_ASSERT_EQUAL_UINT32(image.size(), json["written"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(0, update.abort_calls);
    TEST_ASSERT_FALSE(h.state.isBusy());
    TEST_ASSERT_FALSE(h.upload_locked);
    TEST_ASSERT_TRUE(h.state.snapshot().reboot_pending);
    TEST_ASSERT_TRUE(h.restart.is_scheduled());
    TEST_ASSERT_EQUAL_UINT32(millis() + kRestartDelayMs, h.restart.due_ms());
    TEST_ASSERT_FALSE(h.restart.is_requested());
    TEST_ASSERT_EQUAL_UINT32(0, h.upload_confirm_id.load());
}

void assert_terminal_result_unchanged(const WebOtaSnapshot &before,
                                      const WebOtaSnapshot &after) {
    TEST_ASSERT_EQUAL(before.upload_seen, after.upload_seen);
    TEST_ASSERT_EQUAL(before.active, after.active);
    TEST_ASSERT_EQUAL(before.success, after.success);
    TEST_ASSERT_EQUAL(before.reboot_pending, after.reboot_pending);
    TEST_ASSERT_EQUAL(before.size_known, after.size_known);
    TEST_ASSERT_EQUAL_UINT32(before.session_id, after.session_id);
    TEST_ASSERT_EQUAL_UINT32(before.expected_size, after.expected_size);
    TEST_ASSERT_EQUAL_UINT32(before.slot_size, after.slot_size);
    TEST_ASSERT_EQUAL_UINT32(before.written_size, after.written_size);
    TEST_ASSERT_EQUAL_STRING(before.error.c_str(), after.error.c_str());
    TEST_ASSERT_EQUAL_STRING(before.error_code.c_str(), after.error_code.c_str());
    TEST_ASSERT_EQUAL_UINT32(before.upload_start_ms, after.upload_start_ms);
    TEST_ASSERT_EQUAL_UINT32(before.result_set_ms, after.result_set_ms);
    TEST_ASSERT_EQUAL_UINT32(before.result_ttl_ms, after.result_ttl_ms);
    TEST_ASSERT_EQUAL_UINT32(before.chunk_count, after.chunk_count);
    TEST_ASSERT_EQUAL_UINT32(before.chunk_sum_size, after.chunk_sum_size);
}

void start_request_without_new_approval(Harness &h,
                                       const std::map<String, String> &args) {
    h.request = FakeRequest{};
    h.request.args = args;
    h.request.current_upload.status = WebUploadStatus::Start;
    h.request.current_upload.filename = "firmware.bin";
    WebOtaHandlers::handleUpload(h.runtime, false);
}

void complete_rejected_request(Harness &h, std::vector<uint8_t> &image) {
    TEST_ASSERT_TRUE(h.request.rejected);
    // A rejected HTTP request must remain inert even if callbacks arrive late.
    h.write(image, 0, image.size());
    h.finish();
    h.abort(WebUploadAbortReason::ClientDisconnected);
    h.respond();
    TEST_ASSERT_FALSE(h.state.isBusy());
    TEST_ASSERT_FALSE(h.upload_locked);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    TEST_ASSERT_FALSE(h.restart.is_requested());
    TEST_ASSERT_EQUAL_UINT32(0, h.upload_confirm_id.load());
    TEST_ASSERT_TRUE(h.request.stopped);
    TEST_ASSERT_EQUAL_STRING("close", h.request.header("Connection").c_str());
    assert_no_flash_calls();
}

}  // namespace

void setUp() { setMillis(100); OtaPlatformMock::reset(); }
void tearDown() { current = nullptr; }

void test_start_and_partial_prefix_do_not_begin_or_write_flash() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    assert_no_flash_calls();
    TEST_ASSERT_TRUE(h.state.isActive());
    TEST_ASSERT_EQUAL_UINT32(id, h.upload_confirm_id.load());
    TEST_ASSERT_EQUAL_UINT32(kUploadTimeoutMs, h.request.upload_deadline_ms);
    h.write(image, 0, kPrefixSize - 1);
    assert_no_flash_calls();
    TEST_ASSERT_EQUAL_UINT32(kPrefixSize - 1, h.validator.size());
    TEST_ASSERT_EQUAL_UINT32(kPrefixSize - 1, h.state.snapshot().chunk_sum_size);
    TEST_ASSERT_EQUAL_UINT32(0, h.state.snapshot().written_size);
}

void test_matching_43_image_writes_prefix_and_remainder_exactly_once() {
    Harness h("aura-aq-v1");
    auto image = image_for("aura-aq-v1");
    h.start();
    h.write(image, 0, image.size());
    const auto &sizes = OtaPlatformMock::state().update.write_sizes;
    TEST_ASSERT_EQUAL_UINT32(2, sizes.size());
    TEST_ASSERT_EQUAL_UINT32(kPrefixSize, sizes[0]);
    TEST_ASSERT_EQUAL_UINT32(image.size() - kPrefixSize, sizes[1]);
    assert_success(h, image);
}

void test_matching_7_image_accepts_one_byte_chunks_without_duplicate_prefix() {
    Harness h("aura-aq-7-v1");
    auto image = image_for("aura-aq-7-v1");
    h.start();
    for (size_t i = 0; i < image.size(); ++i) {
        h.write(image, i, 1);
        if (i + 1 < kPrefixSize) {
            assert_no_flash_calls();
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1 + image.size() - kPrefixSize,
                             OtaPlatformMock::state().update.write_calls);
    assert_success(h, image);
}

void test_matching_image_accepts_every_two_chunk_prefix_split() {
    auto image = image_for("aura-aq-v1");
    for (size_t split = 1; split < kPrefixSize; ++split) {
        Harness h;
        h.start();
        h.write(image, 0, split);
        assert_no_flash_calls();
        h.write(image, split, image.size() - split);
        assert_success(h, image);
    }
}

void test_exact_prefix_boundary_and_later_chunks_keep_byte_order() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    h.start();
    h.write(image, 0, kPrefixSize);
    TEST_ASSERT_EQUAL_UINT32(1, OtaPlatformMock::state().update.write_calls);
    h.write(image, kPrefixSize, 5);
    h.write(image, kPrefixSize + 5, image.size() - kPrefixSize - 5);
    TEST_ASSERT_EQUAL_UINT32(3, OtaPlatformMock::state().update.write_calls);
    assert_success(h, image);
}

void test_7_device_rejects_43_even_with_renamed_file_and_forged_form_target() {
    Harness h("aura-aq-7-v1");
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start(image.size(), "Aura_AQ_7_correct.bin");
    h.request.args["hardware_target"] = "aura-aq-7-v1";
    h.request.args["hardware_profile"] = "7_dual_i2c";
    h.write(image, 0, image.size());
    h.finish();
    h.abort(WebUploadAbortReason::ClientDisconnected);
    assert_no_flash_calls();
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "firmware is for Aura AQ 4.3\""));
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "device is Aura AQ 7\""));
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "Nothing was written"));
    TEST_ASSERT_EQUAL_UINT32(1, h.restore_power_save_calls);
    TEST_ASSERT_EQUAL_UINT32(1, h.end_upload_calls);
}

void test_43_device_rejects_7_even_with_renamed_file_and_forged_form_target() {
    Harness h("aura-aq-v1");
    auto image = image_for("aura-aq-7-v1");
    const uint32_t id = h.start(image.size(), "Aura_AQ_4_3_correct.bin");
    h.request.args["hardware_target"] = "aura-aq-v1";
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "firmware is for Aura AQ 7\""));
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "device is Aura AQ 4.3\""));
}

void test_production_7_rejects_diagnostic_bin_before_flash() {
    Harness h("aura-aq-7-v1", "production");
    auto image = image_for("aura-aq-7-diag-v1", "diagnostic");
    const uint32_t id = h.start(image.size(), "renamed-production.bin");
    h.write(image, 0, image.size());
    h.finish();
    assert_no_flash_calls();
    assert_failure(h, "FIRMWARE_FLAVOR_MISMATCH", 409, id);
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "diagnostic-only firmware"));
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "Nothing was written"));
}

void test_production_43_rejects_diagnostic_bin_before_flash() {
    Harness h("aura-aq-v1", "production");
    auto image = image_for("aura-aq-diag-v1", "diagnostic");
    const uint32_t id = h.start(image.size(), "renamed-production.bin");
    h.write(image, 0, image.size());
    h.finish();
    assert_no_flash_calls();
    assert_failure(h, "FIRMWARE_FLAVOR_MISMATCH", 409, id);
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "diagnostic-only firmware"));
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "Nothing was written"));
}

void test_7_device_reports_43_diagnostic_as_43_model() {
    Harness h("aura-aq-7-v1", "production");
    auto image = image_for("aura-aq-diag-v1", "diagnostic");
    const uint32_t id = h.start(image.size(), "renamed-7-inch.bin");
    h.write(image, 0, image.size());
    h.finish();
    assert_no_flash_calls();
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "firmware is for Aura AQ 4.3\""));
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "device is Aura AQ 7\""));
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "Nothing was written"));
}

void test_diagnostic_7_accepts_diagnostic_update_and_production_exit() {
    {
        Harness h("aura-aq-7-v1", "diagnostic");
        auto image = image_for("aura-aq-7-diag-v1", "diagnostic");
        h.start();
        h.write(image, 0, image.size());
        assert_success(h, image);
    }
    {
        Harness h("aura-aq-7-v1", "diagnostic");
        auto image = image_for("aura-aq-7-v1", "production");
        h.start();
        h.write(image, 0, image.size());
        assert_success(h, image);
    }
}

void test_diagnostic_43_accepts_diagnostic_update_and_production_exit() {
    {
        Harness h("aura-aq-v1", "diagnostic");
        auto image = image_for("aura-aq-diag-v1", "diagnostic");
        h.start();
        h.write(image, 0, image.size());
        assert_success(h, image);
    }
    {
        Harness h("aura-aq-v1", "diagnostic");
        auto image = image_for("aura-aq-v1", "production");
        h.start();
        h.write(image, 0, image.size());
        assert_success(h, image);
    }
}

void test_new_guard_rejects_legacy_target_only_bin_before_flash() {
    Harness h("aura-aq-7-v1", "production");
    auto image = image_for("aura-aq-7-v1", nullptr);
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "FIRMWARE_FLAVOR_MISSING", 400, id);
    TEST_ASSERT_NOT_NULL(std::strstr(
        h.state.snapshot().error.c_str(), "cannot distinguish production firmware"));
}

void test_legacy_bin_without_fixed_metadata_is_rejected_despite_target_elsewhere() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    std::fill(image.begin() + kMetadataOffset, image.begin() + kPrefixSize, 0);
    const char incidental_target[] = "aura-aq-v1";
    std::memcpy(image.data() + 400, incidental_target, sizeof(incidental_target));
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "HARDWARE_TARGET_MISSING", 400, id);
}

void test_unsupported_metadata_version_is_not_misclassified_as_missing_file() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    put16(image, kMetadataOffset + 16, 2);
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "HARDWARE_METADATA_UNSUPPORTED", 400, id);
}

void test_unknown_target_inside_valid_descriptor_is_rejected() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    image[kMetadataOffset + 20] = 'X';
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "INVALID_FIRMWARE", 400, id);
}

void test_unknown_running_target_fails_closed_before_flash() {
    Harness h("unknown");
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    assert_no_flash_calls();
    assert_failure(h, "INVALID_FIRMWARE", 400, id);
}

void test_non_application_and_wrong_chip_headers_are_rejected() {
    for (size_t bad_field : {size_t(0), size_t(12), size_t(32)}) {
        Harness h;
        auto image = image_for("aura-aq-v1");
        image[bad_field] ^= 0x40;
        const uint32_t id = h.start();
        h.write(image, 0, image.size());
        assert_no_flash_calls();
        assert_failure(h, "INVALID_FIRMWARE", 400, id);
    }
}

void test_end_with_partial_prefix_is_invalid_firmware_not_size_mismatch() {
    for (size_t received : {size_t(0), size_t(31), kPrefixSize - 1}) {
        Harness h;
        auto image = image_for("aura-aq-v1");
        const uint32_t id = h.start();
        if (received != 0) {
            h.write(image, 0, received);
        }
        h.finish();
        assert_no_flash_calls();
        assert_failure(h, "INVALID_FIRMWARE", 400, id);
    }
}

void test_declared_file_shorter_than_identity_prefix_is_rejected_at_start() {
    Harness h;
    const uint32_t id = h.start(kPrefixSize - 1);
    assert_no_flash_calls();
    assert_failure(h, "INVALID_FIRMWARE", 400, id);
}

void test_rejected_image_cleanup_allows_good_retry_on_same_runtime() {
    Harness h;
    auto wrong = image_for("aura-aq-7-v1");
    const uint32_t rejected_id = h.start();
    const uint32_t rejected_session = h.state.snapshot().session_id;
    h.write(wrong, 0, wrong.size());
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, rejected_id);
    assert_no_flash_calls();

    auto correct = image_for("aura-aq-v1");
    const uint32_t next_id = h.start();
    TEST_ASSERT_NOT_EQUAL(rejected_id, next_id);
    TEST_ASSERT_NOT_EQUAL(rejected_session, h.state.snapshot().session_id);
    TEST_ASSERT_FALSE(h.state.snapshot().hasError());
    TEST_ASSERT_TRUE(h.state.snapshot().error_code.empty());
    TEST_ASSERT_EQUAL_UINT32(0, h.validator.size());
    TEST_ASSERT_EQUAL_UINT32(0, h.state.snapshot().written_size);
    h.write(correct, 0, correct.size());
    assert_success(h, correct);
    TEST_ASSERT_EQUAL_UINT32(2, h.restore_power_save_calls);
    TEST_ASSERT_EQUAL_UINT32(2, h.end_upload_calls);
}

void test_terminal_identity_rejection_replays_original_result_for_same_confirmation() {
    for (const char *target : {"aura-aq-v1", "aura-aq-7-v1"}) {
        for (bool missing_metadata : {false, true}) {
            Harness h(target);
            const char *other_target = std::strcmp(target, "aura-aq-v1") == 0
                                           ? "aura-aq-7-v1" : "aura-aq-v1";
            auto rejected = image_for(missing_metadata ? target : other_target);
            if (missing_metadata) {
                std::fill(rejected.begin() + kMetadataOffset,
                          rejected.begin() + kPrefixSize, 0);
            }
            const char *code = missing_metadata ? "HARDWARE_TARGET_MISSING"
                                                : "HARDWARE_TARGET_MISMATCH";
            const int status = missing_metadata ? 400 : 409;
            const uint32_t id = h.start(rejected.size());
            h.write(rejected, 0, rejected.size());
            assert_failure(h, code, status, id);
            const auto before = h.state.snapshot();
            const size_t ui_count = h.ui.size();
            const size_t cancel_count = h.cancelled.size();
            const unsigned power_save_count = h.disable_power_save_calls;
            const unsigned scan_count = h.stop_scan_calls;

            // Match the observed separate POSTs at +10 and +18 seconds.
            for (uint32_t elapsed_ms : {10000U, 18000U}) {
                setMillis(before.result_set_ms + elapsed_ms);
                start_request_without_new_approval(h, {
                    {"ota_size", std::to_string(rejected.size())},
                    {"ota_confirm_id", std::to_string(id)},
                });
                complete_rejected_request(h, rejected);
                TEST_ASSERT_EQUAL_INT(status, h.request.response_status);
                const auto json = h.request.json();
                TEST_ASSERT_EQUAL_STRING(code, json["error_code"].as<const char *>());
                TEST_ASSERT_EQUAL_STRING(before.error.c_str(), json["error"].as<const char *>());
                TEST_ASSERT_TRUE(json["written"].is<uint32_t>());
                TEST_ASSERT_EQUAL_UINT32(0, json["written"].as<uint32_t>());
                TEST_ASSERT_EQUAL_UINT32(rejected.size(), json["expected"].as<uint32_t>());
                TEST_ASSERT_FALSE(json["success"].as<bool>());
                TEST_ASSERT_FALSE(json["rebooting"].as<bool>());
                assert_terminal_result_unchanged(before, h.state.snapshot());
                TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
                TEST_ASSERT_EQUAL_UINT32(cancel_count, h.cancelled.size());
                TEST_ASSERT_EQUAL_UINT32(power_save_count, h.disable_power_save_calls);
                TEST_ASSERT_EQUAL_UINT32(scan_count, h.stop_scan_calls);
            }

            // A new physical approval must still permit an actual retry.
            auto correct = image_for(target);
            const uint32_t next_id = h.start(correct.size());
            TEST_ASSERT_NOT_EQUAL(id, next_id);
            TEST_ASSERT_NOT_EQUAL(before.session_id, h.state.snapshot().session_id);
            TEST_ASSERT_FALSE(h.state.hasError());
            h.write(correct, 0, correct.size());
            assert_success(h, correct);
        }
    }
}

void test_unrelated_start_refusals_do_not_replace_terminal_identity_result() {
    struct RefusalCase {
        const char *size;
        const char *confirm_id;
        bool pending_boot;
        int status;
        const char *code;
        const char *message;
    };
    const RefusalCase cases[] = {
        {"512", "99999", false, 409, "OTA_PHYSICAL_CONFIRM_MISMATCH",
         "Firmware update confirmation does not match this upload."},
        {"512", nullptr, false, 403, "OTA_PHYSICAL_CONFIRM_REQUIRED",
         "Firmware update confirmation is required."},
        {"512", "invalid", false, 403, "OTA_PHYSICAL_CONFIRM_REQUIRED",
         "Firmware update confirmation is required."},
        {"512", "0", false, 403, "OTA_PHYSICAL_CONFIRM_REQUIRED",
         "Firmware update confirmation is required."},
        {"512", "4294967296", false, 403, "OTA_PHYSICAL_CONFIRM_REQUIRED",
         "Firmware update confirmation is required."},
        {"513", "1", false, 409, "OTA_PHYSICAL_CONFIRM_MISMATCH",
         "Firmware update confirmation does not match this upload."},
        {"invalid", "1", false, 400, "INVALID_SIZE", "Invalid firmware size"},
        {"0", "1", false, 400, "INVALID_SIZE", "Invalid firmware size"},
        {nullptr, "1", false, 400, "INVALID_SIZE", "Firmware size is required"},
        {"512", "1", true, 409, "OTA_BOOT_PENDING_VERIFY",
         "Firmware boot validation is still pending; wait until the device is stable before starting another OTA."},
    };
    for (const auto &test_case : cases) {
        Harness h;
        auto wrong = image_for("aura-aq-7-v1");
        const uint32_t id = h.start();
        TEST_ASSERT_EQUAL_UINT32(1, id);
        h.write(wrong, 0, wrong.size());
        assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
        const auto before = h.state.snapshot();
        const size_t ui_count = h.ui.size();
        const size_t cancel_count = h.cancelled.size();
        advanceMillis(1000);
        std::map<String, String> args;
        if (test_case.size) args["ota_size"] = test_case.size;
        if (test_case.confirm_id) args["ota_confirm_id"] = test_case.confirm_id;
        OtaPlatformMock::state().boot_pending_verify = test_case.pending_boot;
        start_request_without_new_approval(h, args);
        complete_rejected_request(h, wrong);
        const auto json = h.request.json();
        TEST_ASSERT_EQUAL_INT(test_case.status, h.request.response_status);
        TEST_ASSERT_EQUAL_STRING(test_case.code, json["error_code"].as<const char *>());
        TEST_ASSERT_EQUAL_STRING(test_case.message, json["error"].as<const char *>());
        TEST_ASSERT_FALSE(json["success"].as<bool>());
        assert_terminal_result_unchanged(before, h.state.snapshot());
        TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
        TEST_ASSERT_EQUAL_UINT32(cancel_count, h.cancelled.size());
    }
}

void test_rejection_replay_expires_at_original_terminal_deadline() {
    Harness h;
    auto wrong = image_for("aura-aq-7-v1");
    const uint32_t id = h.start();
    h.write(wrong, 0, wrong.size());
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    const auto before = h.state.snapshot();
    const std::map<String, String> args = {
        {"ota_size", std::to_string(wrong.size())},
        {"ota_confirm_id", std::to_string(id)},
    };
    setMillis(before.result_set_ms + before.result_ttl_ms - 1);
    start_request_without_new_approval(h, args);
    complete_rejected_request(h, wrong);
    TEST_ASSERT_EQUAL_STRING("HARDWARE_TARGET_MISMATCH",
                             h.request.json()["error_code"].as<const char *>());
    assert_terminal_result_unchanged(before, h.state.snapshot());

    advanceMillis(1);
    start_request_without_new_approval(h, args);
    complete_rejected_request(h, wrong);
    TEST_ASSERT_EQUAL_INT(403, h.request.response_status);
    TEST_ASSERT_EQUAL_STRING("OTA_PHYSICAL_CONFIRM_EXPIRED",
                             h.request.json()["error_code"].as<const char *>());
    TEST_ASSERT_FALSE(h.state.snapshot().hasTerminalResult(millis()));
    TEST_ASSERT_FALSE(h.state.isActive());
}

void test_old_confirmation_cannot_replay_after_new_session_replaces_result() {
    Harness h;
    auto wrong = image_for("aura-aq-7-v1");
    const uint32_t old_id = h.start();
    h.write(wrong, 0, wrong.size());
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, old_id);
    auto legacy = image_for("aura-aq-v1");
    std::fill(legacy.begin() + kMetadataOffset, legacy.begin() + kPrefixSize, 0);
    advanceMillis(1000);
    const uint32_t new_id = h.start();
    TEST_ASSERT_NOT_EQUAL(old_id, new_id);
    h.write(legacy, 0, legacy.size());
    assert_failure(h, "HARDWARE_TARGET_MISSING", 400, new_id);
    const auto before = h.state.snapshot();
    const size_t ui_count = h.ui.size();

    start_request_without_new_approval(h, {
        {"ota_size", std::to_string(wrong.size())},
        {"ota_confirm_id", std::to_string(old_id)},
    });
    complete_rejected_request(h, wrong);
    TEST_ASSERT_EQUAL_INT(409, h.request.response_status);
    TEST_ASSERT_EQUAL_STRING("OTA_PHYSICAL_CONFIRM_MISMATCH",
                             h.request.json()["error_code"].as<const char *>());
    assert_terminal_result_unchanged(before, h.state.snapshot());
    TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
}

void test_total_timeout_while_prefix_pending_never_begins_flash() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    h.write(image, 0, 100);
    advanceMillis(kUploadTimeoutMs);
    h.write(image, 100, image.size() - 100);
    h.finish();
    assert_no_flash_calls();
    assert_failure(h, "UPLOAD_TIMEOUT", 408, id);
}

void test_idle_abort_while_prefix_pending_resets_cleanly_for_retry() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    h.write(image, 0, 120);
    advanceMillis(800);
    h.abort(WebUploadAbortReason::IdleTimeout);
    assert_no_flash_calls();
    assert_failure(h, "UPLOAD_TIMEOUT", 408, id);

    h.start();
    TEST_ASSERT_EQUAL_UINT32(0, h.validator.size());
    h.write(image, 0, image.size());
    assert_success(h, image);
}

void test_begin_failure_reports_original_error_without_write_or_reboot() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    auto &update = OtaPlatformMock::state().update;
    update.begin_result = false;
    update.error_text = "simulated begin problem";
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    TEST_ASSERT_EQUAL_UINT32(1, update.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.write_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.end_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.abort_calls);
    assert_failure(h, "BEGIN_FAILED", 500, id);
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "simulated begin problem"));
}

void test_short_prefix_write_aborts_and_preserves_partial_byte_count() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    auto &update = OtaPlatformMock::state().update;
    update.fail_write_call = 1;
    update.failed_write_bytes = 17;
    update.error_text = "simulated prefix write problem";
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    h.finish();
    TEST_ASSERT_EQUAL_UINT32(1, update.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(1, update.write_calls);
    TEST_ASSERT_EQUAL_UINT32(17, update.bytes.size());
    TEST_ASSERT_EQUAL_MEMORY(image.data(), update.bytes.data(), 17);
    TEST_ASSERT_EQUAL_UINT32(1, update.abort_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.end_calls);
    assert_failure(h, "WRITE_FAILED", 500, id, 17);
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "simulated prefix write problem"));
}

void test_short_remainder_write_that_self_aborts_keeps_original_error() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    auto &update = OtaPlatformMock::state().update;
    update.fail_write_call = 2;
    update.failed_write_bytes = 7;
    update.self_abort_on_write_failure = true;
    update.error_text = "simulated remainder write problem";
    const uint32_t id = h.start();
    h.write(image, 0, image.size());
    h.finish();
    TEST_ASSERT_EQUAL_UINT32(2, update.write_calls);
    TEST_ASSERT_EQUAL_UINT32(kPrefixSize + 7, update.bytes.size());
    TEST_ASSERT_EQUAL_MEMORY(image.data(), update.bytes.data(), kPrefixSize + 7);
    TEST_ASSERT_EQUAL_UINT32(0, update.abort_calls);
    TEST_ASSERT_EQUAL_UINT32(0, update.end_calls);
    assert_failure(h, "WRITE_FAILED", 500, id, kPrefixSize + 7);
    TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "simulated remainder write problem"));
}

void test_finalize_failure_aborts_and_never_schedules_restart() {
    for (bool self_abort : {false, true}) {
        Harness h;
        auto image = image_for("aura-aq-v1");
        auto &update = OtaPlatformMock::state().update;
        update.end_result = false;
        update.self_abort_on_end_failure = self_abort;
        update.error_text = "simulated checksum failure";
        const uint32_t id = h.start();
        h.write(image, 0, image.size());
        h.finish();
        TEST_ASSERT_EQUAL_UINT32(1, update.end_calls);
        TEST_ASSERT_EQUAL_UINT32(self_abort ? 0 : 1, update.abort_calls);
        assert_failure(h, "FINALIZE_FAILED", 500, id, image.size());
        TEST_ASSERT_NOT_NULL(std::strstr(h.state.snapshot().error.c_str(), "simulated checksum failure"));
    }
}

void test_busy_start_cannot_reset_active_prefix_session_or_screen() {
    for (bool busy_argument : {false, true}) {
        Harness h;
        auto image = image_for("aura-aq-v1");
        const uint32_t id = h.start();
        h.write(image, 0, 150);
        const auto before = h.state.snapshot();
        const size_t ui_count = h.ui.size();
        const unsigned try_count = h.try_begin_calls;
        FakeRequest competing;
        competing.current_upload.status = WebUploadStatus::Start;
        competing.args["ota_size"] = "99999";
        competing.args["ota_confirm_id"] = "4294967295";
        h.context.server = &competing;
        WebOtaHandlers::handleUpload(h.runtime, busy_argument);
        TEST_ASSERT_TRUE(competing.rejected);
        WebOtaHandlers::handleUpdate(h.runtime, true);
        TEST_ASSERT_EQUAL_INT(503, competing.response_status);
        const auto json = competing.json();
        TEST_ASSERT_EQUAL_STRING("OTA_BUSY", json["error_code"].as<const char *>());
        TEST_ASSERT_EQUAL_UINT32(150, h.validator.size());
        TEST_ASSERT_EQUAL_UINT32(before.session_id, h.state.snapshot().session_id);
        TEST_ASSERT_EQUAL_UINT32(id, h.upload_confirm_id.load());
        TEST_ASSERT_TRUE(h.state.isActive());
        TEST_ASSERT_TRUE(h.upload_locked);
        TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
        TEST_ASSERT_EQUAL_UINT32(0, h.restore_power_save_calls);
        TEST_ASSERT_EQUAL_UINT32(0, h.end_upload_calls);
        TEST_ASSERT_EQUAL_UINT32(try_count + (busy_argument ? 0 : 1), h.try_begin_calls);
        assert_no_flash_calls();
        h.context.server = &h.request;
        h.write(image, 150, image.size() - 150);
        assert_success(h, image);
    }
}

void test_busy_replay_cannot_release_or_change_new_active_session() {
    for (bool busy_argument : {false, true}) {
        Harness h;
        auto wrong = image_for("aura-aq-7-v1");
        const uint32_t old_id = h.start();
        h.write(wrong, 0, wrong.size());
        assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, old_id);
        auto correct = image_for("aura-aq-v1");
        const uint32_t new_id = h.start();
        h.write(correct, 0, 150);
        const auto before = h.state.snapshot();
        const size_t ui_count = h.ui.size();
        const unsigned cleanup_count = h.end_upload_calls;
        const unsigned restore_count = h.restore_power_save_calls;
        FakeRequest competing;
        competing.args["ota_size"] = std::to_string(wrong.size());
        competing.args["ota_confirm_id"] = std::to_string(old_id);
        competing.current_upload.status = WebUploadStatus::Start;
        h.context.server = &competing;
        WebOtaHandlers::handleUpload(h.runtime, busy_argument);
        TEST_ASSERT_TRUE(competing.rejected);
        competing.current_upload.status = WebUploadStatus::Write;
        competing.current_upload.buf = wrong.data();
        competing.current_upload.currentSize = wrong.size();
        WebOtaHandlers::handleUpload(h.runtime, true);
        competing.current_upload.status = WebUploadStatus::End;
        WebOtaHandlers::handleUpload(h.runtime, true);
        competing.current_upload.status = WebUploadStatus::Aborted;
        competing.current_upload.abort_reason = WebUploadAbortReason::ClientDisconnected;
        WebOtaHandlers::handleUpload(h.runtime, true);
        WebOtaHandlers::handleUpdate(h.runtime, true);
        TEST_ASSERT_EQUAL_INT(503, competing.response_status);
        TEST_ASSERT_EQUAL_STRING("OTA_BUSY", competing.json()["error_code"].as<const char *>());
        assert_terminal_result_unchanged(before, h.state.snapshot());
        TEST_ASSERT_EQUAL_UINT32(new_id, h.upload_confirm_id.load());
        TEST_ASSERT_EQUAL_UINT32(150, h.validator.size());
        TEST_ASSERT_TRUE(h.state.isActive());
        TEST_ASSERT_TRUE(h.state.isBusy());
        TEST_ASSERT_TRUE(h.upload_locked);
        TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
        TEST_ASSERT_EQUAL_UINT32(cleanup_count, h.end_upload_calls);
        TEST_ASSERT_EQUAL_UINT32(restore_count, h.restore_power_save_calls);
        assert_no_flash_calls();
        h.context.server = &h.request;
        h.write(correct, 150, correct.size() - 150);
        assert_success(h, correct);
    }
}

void test_bare_update_request_does_not_reuse_previous_rejection() {
    Harness h;
    auto wrong = image_for("aura-aq-7-v1");
    const uint32_t id = h.start();
    h.write(wrong, 0, wrong.size());
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    const auto before = h.state.snapshot();
    const unsigned cleanup_count = h.end_upload_calls;
    const size_t ui_count = h.ui.size();
    FakeRequest bare;
    h.context.server = &bare;
    WebOtaHandlers::handleUpdate(h.runtime, false);
    TEST_ASSERT_EQUAL_INT(400, bare.response_status);
    const auto json = bare.json();
    TEST_ASSERT_EQUAL_STRING("MISSING_FILE", json["error_code"].as<const char *>());
    TEST_ASSERT_FALSE(json["success"].as<bool>());
    TEST_ASSERT_FALSE(json["rebooting"].as<bool>());
    assert_terminal_result_unchanged(before, h.state.snapshot());
    TEST_ASSERT_EQUAL_UINT32(cleanup_count, h.end_upload_calls);
    TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    assert_no_flash_calls();
}

void test_bare_update_request_does_not_reuse_success_or_reschedule_restart() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    h.start();
    h.write(image, 0, image.size());
    assert_success(h, image);
    const auto before = h.state.snapshot();
    const uint32_t restart_due = h.restart.due_ms();
    const unsigned cleanup_count = h.end_upload_calls;
    const size_t ui_count = h.ui.size();
    const auto before_update = OtaPlatformMock::state().update;
    advanceMillis(200);
    FakeRequest bare;
    h.context.server = &bare;
    WebOtaHandlers::handleUpdate(h.runtime, false);
    TEST_ASSERT_EQUAL_INT(400, bare.response_status);
    const auto json = bare.json();
    TEST_ASSERT_EQUAL_STRING("MISSING_FILE", json["error_code"].as<const char *>());
    TEST_ASSERT_FALSE(json["success"].as<bool>());
    TEST_ASSERT_FALSE(json["rebooting"].as<bool>());
    TEST_ASSERT_TRUE(h.restart.is_scheduled());
    TEST_ASSERT_EQUAL_UINT32(restart_due, h.restart.due_ms());
    assert_terminal_result_unchanged(before, h.state.snapshot());
    TEST_ASSERT_EQUAL_UINT32(cleanup_count, h.end_upload_calls);
    TEST_ASSERT_EQUAL_UINT32(ui_count, h.ui.size());
    const auto &after_update = OtaPlatformMock::state().update;
    TEST_ASSERT_EQUAL_UINT32(before_update.begin_calls, after_update.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(before_update.write_calls, after_update.write_calls);
    TEST_ASSERT_EQUAL_UINT32(before_update.end_calls, after_update.end_calls);
    TEST_ASSERT_EQUAL_UINT32(before_update.abort_calls, after_update.abort_calls);
}

void test_late_abort_after_successful_end_preserves_success_until_response() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    h.start();
    h.write(image, 0, image.size());
    h.finish();
    const auto completed = h.state.snapshot();
    TEST_ASSERT_TRUE(completed.success);
    TEST_ASSERT_FALSE(completed.active);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    const auto flash = OtaPlatformMock::state().update;
    advanceMillis(25);
    h.abort(WebUploadAbortReason::ClientDisconnected);
    assert_terminal_result_unchanged(completed, h.state.snapshot());
    TEST_ASSERT_TRUE(h.upload_locked);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    h.respond();
    const auto json = h.request.json();
    TEST_ASSERT_EQUAL_INT(200, h.request.response_status);
    TEST_ASSERT_TRUE(json["success"].as<bool>());
    TEST_ASSERT_TRUE(json["rebooting"].as<bool>());
    TEST_ASSERT_TRUE(json["error_code"].isNull());
    TEST_ASSERT_EQUAL_UINT32(image.size(), json["written"].as<uint32_t>());
    TEST_ASSERT_FALSE(h.state.isBusy());
    TEST_ASSERT_FALSE(h.upload_locked);
    TEST_ASSERT_EQUAL_UINT32(0, h.upload_confirm_id.load());
    TEST_ASSERT_EQUAL_UINT32(1, h.end_upload_calls);
    TEST_ASSERT_EQUAL_UINT32(1, h.restore_power_save_calls);
    TEST_ASSERT_TRUE(h.restart.is_scheduled());
    TEST_ASSERT_EQUAL_UINT32(millis() + kRestartDelayMs, h.restart.due_ms());
    const auto &after_flash = OtaPlatformMock::state().update;
    TEST_ASSERT_EQUAL_UINT32(flash.begin_calls, after_flash.begin_calls);
    TEST_ASSERT_EQUAL_UINT32(flash.write_calls, after_flash.write_calls);
    TEST_ASSERT_EQUAL_UINT32(1, after_flash.end_calls);
    TEST_ASSERT_EQUAL_UINT32(0, after_flash.abort_calls);
}

void test_reused_request_reset_clears_old_session_and_cached_refusal() {
    Harness h;
    auto wrong = image_for("aura-aq-7-v1");
    const uint32_t old_id = h.start();
    h.write(wrong, 0, wrong.size());
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, old_id);
    const auto before = h.state.snapshot();
    // Reuse the transport object, without assigning a fresh FakeRequest.
    const auto next_request = [&h]() {
        h.request.resetUploadResponseState();
        h.request.args.clear();
        h.request.headers.clear();
        h.request.current_upload = WebUpload{};
        h.request.received = 0;
        h.request.connected = true;
        h.request.rejected = false;
        h.request.stopped = false;
        h.request.response_status = 0;
        h.request.response.clear();
    };
    next_request();
    h.respond();
    TEST_ASSERT_EQUAL_INT(400, h.request.response_status);
    TEST_ASSERT_EQUAL_STRING("MISSING_FILE", h.request.json()["error_code"].as<const char *>());
    assert_terminal_result_unchanged(before, h.state.snapshot());
    assert_no_flash_calls();

    next_request();
    auto correct = image_for("aura-aq-v1");
    const auto approval = h.physical_confirm.prepare(correct.size(), false, 0, millis());
    TEST_ASSERT_TRUE(h.physical_confirm.allowCurrent(approval.confirm_id, millis()));
    TEST_ASSERT_NOT_EQUAL(old_id, approval.confirm_id);
    h.request.args["ota_size"] = std::to_string(correct.size());
    h.request.args["ota_confirm_id"] = std::to_string(approval.confirm_id);
    h.request.current_upload.status = WebUploadStatus::Start;
    h.request.current_upload.filename = "firmware.bin";
    WebOtaHandlers::handleUpload(h.runtime, false);
    h.write(correct, 0, correct.size());
    assert_success(h, correct);
    TEST_ASSERT_NOT_EQUAL(before.session_id, h.state.snapshot().session_id);
}

void test_rejection_drains_only_bounded_body_and_keeps_failure_payload() {
    Harness h;
    auto image = image_for("aura-aq-7-v1");
    const uint32_t id = h.start();
    h.write(image, 0, kPrefixSize);
    h.request.pending_body = 64 * 1024;
    assert_failure(h, "HARDWARE_TARGET_MISMATCH", 409, id);
    TEST_ASSERT_EQUAL_UINT32(32 * 1024, h.request.drained);
    TEST_ASSERT_EQUAL_UINT32(32 * 1024, h.request.pending_body);
    TEST_ASSERT_EQUAL_UINT32(1500, h.request.drain_timeout);
    assert_no_flash_calls();
}

void test_partition_unavailable_and_declared_oversize_do_not_begin_flash() {
    {
        Harness h;
        OtaPlatformMock::state().partition_available = false;
        const uint32_t id = h.start();
        assert_no_flash_calls();
        assert_failure(h, "OTA_UNAVAILABLE", 503, id);
    }
    {
        Harness h;
        OtaPlatformMock::state().partition_size = 400;
        const uint32_t id = h.start();
        assert_no_flash_calls();
        assert_failure(h, "IMAGE_TOO_LARGE", 413, id);
    }
}

void test_size_guard_counts_pending_prefix_not_only_written_bytes() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    h.write(image, 0, 200);
    h.write(image, 0, 400);
    assert_no_flash_calls();
    assert_failure(h, "SIZE_MISMATCH", 400, id);
}

void test_incomplete_body_after_valid_prefix_aborts_instead_of_finalizing() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    const uint32_t id = h.start();
    h.write(image, 0, kPrefixSize);
    h.finish();
    TEST_ASSERT_EQUAL_UINT32(1, OtaPlatformMock::state().update.abort_calls);
    TEST_ASSERT_EQUAL_UINT32(0, OtaPlatformMock::state().update.end_calls);
    assert_failure(h, "SIZE_MISMATCH", 400, id, kPrefixSize);
}

void test_prepare_requires_real_physical_confirm_and_never_touches_flash() {
    Harness h;
    h.request.args["ota_size"] = std::to_string(kFixtureSize);
    WebOtaHandlers::handlePrepare(h.runtime, false);
    auto json = h.request.json();
    TEST_ASSERT_EQUAL_INT(403, h.request.response_status);
    TEST_ASSERT_EQUAL_STRING("OTA_PHYSICAL_CONFIRM_REQUIRED", json["error_code"].as<const char *>());
    const uint32_t id = json["confirm_id"].as<uint32_t>();
    TEST_ASSERT_TRUE(id != 0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebUiBridge::FirmwareUpdateScreenMode::ConfirmPending),
                         static_cast<int>(h.ui.back().first));
    TEST_ASSERT_TRUE(h.physical_confirm.allowCurrent(id, millis()));
    h.request.args["ota_confirm_id"] = std::to_string(id);
    WebOtaHandlers::handlePrepare(h.runtime, false);
    json = h.request.json();
    TEST_ASSERT_EQUAL_INT(200, h.request.response_status);
    TEST_ASSERT_TRUE(json["success"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(1, h.armed.size());
    TEST_ASSERT_EQUAL_UINT32(id, h.armed.back());
    assert_no_flash_calls();
}

void test_unconfirmed_upload_cannot_publish_unvalidated_ui_id_or_begin_flash() {
    Harness h;
    auto image = image_for("aura-aq-v1");
    h.request.args["ota_size"] = std::to_string(image.size());
    h.request.args["ota_confirm_id"] = "4294967295";
    h.request.current_upload.status = WebUploadStatus::Start;
    WebOtaHandlers::handleUpload(h.runtime, false);
    h.write(image, 0, image.size());
    h.respond();
    const auto json = h.request.json();
    TEST_ASSERT_EQUAL_INT(409, h.request.response_status);
    TEST_ASSERT_EQUAL_STRING("OTA_PHYSICAL_CONFIRM_MISMATCH", json["error_code"].as<const char *>());
    TEST_ASSERT_TRUE(h.ui.empty());
    TEST_ASSERT_EQUAL_UINT32(0, h.upload_confirm_id.load());
    TEST_ASSERT_FALSE(h.upload_locked);
    TEST_ASSERT_FALSE(h.restart.is_scheduled());
    assert_no_flash_calls();
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_start_and_partial_prefix_do_not_begin_or_write_flash);
    RUN_TEST(test_matching_43_image_writes_prefix_and_remainder_exactly_once);
    RUN_TEST(test_matching_7_image_accepts_one_byte_chunks_without_duplicate_prefix);
    RUN_TEST(test_matching_image_accepts_every_two_chunk_prefix_split);
    RUN_TEST(test_exact_prefix_boundary_and_later_chunks_keep_byte_order);
    RUN_TEST(test_7_device_rejects_43_even_with_renamed_file_and_forged_form_target);
    RUN_TEST(test_43_device_rejects_7_even_with_renamed_file_and_forged_form_target);
    RUN_TEST(test_production_43_rejects_diagnostic_bin_before_flash);
    RUN_TEST(test_production_7_rejects_diagnostic_bin_before_flash);
    RUN_TEST(test_7_device_reports_43_diagnostic_as_43_model);
    RUN_TEST(test_diagnostic_43_accepts_diagnostic_update_and_production_exit);
    RUN_TEST(test_diagnostic_7_accepts_diagnostic_update_and_production_exit);
    RUN_TEST(test_new_guard_rejects_legacy_target_only_bin_before_flash);
    RUN_TEST(test_legacy_bin_without_fixed_metadata_is_rejected_despite_target_elsewhere);
    RUN_TEST(test_unsupported_metadata_version_is_not_misclassified_as_missing_file);
    RUN_TEST(test_unknown_target_inside_valid_descriptor_is_rejected);
    RUN_TEST(test_unknown_running_target_fails_closed_before_flash);
    RUN_TEST(test_non_application_and_wrong_chip_headers_are_rejected);
    RUN_TEST(test_end_with_partial_prefix_is_invalid_firmware_not_size_mismatch);
    RUN_TEST(test_declared_file_shorter_than_identity_prefix_is_rejected_at_start);
    RUN_TEST(test_rejected_image_cleanup_allows_good_retry_on_same_runtime);
    RUN_TEST(test_terminal_identity_rejection_replays_original_result_for_same_confirmation);
    RUN_TEST(test_unrelated_start_refusals_do_not_replace_terminal_identity_result);
    RUN_TEST(test_rejection_replay_expires_at_original_terminal_deadline);
    RUN_TEST(test_old_confirmation_cannot_replay_after_new_session_replaces_result);
    RUN_TEST(test_total_timeout_while_prefix_pending_never_begins_flash);
    RUN_TEST(test_idle_abort_while_prefix_pending_resets_cleanly_for_retry);
    RUN_TEST(test_begin_failure_reports_original_error_without_write_or_reboot);
    RUN_TEST(test_short_prefix_write_aborts_and_preserves_partial_byte_count);
    RUN_TEST(test_short_remainder_write_that_self_aborts_keeps_original_error);
    RUN_TEST(test_finalize_failure_aborts_and_never_schedules_restart);
    RUN_TEST(test_busy_start_cannot_reset_active_prefix_session_or_screen);
    RUN_TEST(test_busy_replay_cannot_release_or_change_new_active_session);
    RUN_TEST(test_bare_update_request_does_not_reuse_previous_rejection);
    RUN_TEST(test_bare_update_request_does_not_reuse_success_or_reschedule_restart);
    RUN_TEST(test_late_abort_after_successful_end_preserves_success_until_response);
    RUN_TEST(test_reused_request_reset_clears_old_session_and_cached_refusal);
    RUN_TEST(test_rejection_drains_only_bounded_body_and_keeps_failure_payload);
    RUN_TEST(test_partition_unavailable_and_declared_oversize_do_not_begin_flash);
    RUN_TEST(test_size_guard_counts_pending_prefix_not_only_written_bytes);
    RUN_TEST(test_incomplete_body_after_valid_prefix_aborts_instead_of_finalizing);
    RUN_TEST(test_prepare_requires_real_physical_confirm_and_never_touches_flash);
    RUN_TEST(test_unconfirmed_upload_cannot_publish_unvalidated_ui_id_or_begin_flash);
    return UNITY_END();
}
