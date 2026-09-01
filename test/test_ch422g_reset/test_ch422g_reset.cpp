#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "Ch422gBoardPolicy.h"
#include "driver/i2c.h"
#include "port/esp_io_expander_ch422g.h"

// Public vendor type constants, needed when compiling the board configuration
// without the ESP32-only display driver. Firmware builds check the real types.
#define ESP_PANEL_BACKLIGHT_TYPE_SWITCH_GPIO 0
#define ESP_PANEL_BACKLIGHT_TYPE_SWITCH_EXPANDER 1
#define ESP_PANEL_BACKLIGHT_TYPE_PWM_LEDC 2
#define ESP_PANEL_BACKLIGHT_TYPE_CUSTOM 3
#include "esp_panel_board_custom_conf.h"

namespace {

constexpr uint8_t kWriteOcAddress = 0x23U;
constexpr uint8_t kWriteIoAddress = 0x38U;
constexpr uint8_t kWriteSetAddress = 0x24U;
constexpr uint8_t kUsbSelMask = 0x20U;
#if defined(AURA_HARDWARE_PROFILE_7) && AURA_HARDWARE_PROFILE_7
constexpr uint8_t kExpectedInitialIo = 0xD1U;
#else
constexpr uint8_t kExpectedInitialIo = 0xDBU;
#endif
constexpr uint32_t kExpectedInitialOutput = 0x0F00U | kExpectedInitialIo;

struct WriteEvent {
    uint8_t address = 0;
    uint8_t value = 0;
};

struct FakeTransport {
    WriteEvent events[64]{};
    size_t event_count = 0;
    size_t read_count = 0;
    size_t fail_call = 0;
    esp_err_t failure = ESP_FAIL;
};

FakeTransport *g_transport = nullptr;

struct BacklightExpander {
    esp_io_expander_handle_t handle = nullptr;
    size_t write_calls = 0;

    bool digitalWrite(uint8_t pin, uint8_t level) {
        ++write_calls;
        return esp_io_expander_set_level(handle, 1U << pin, level) == ESP_OK;
    }
};

struct BacklightAdapter {
    BacklightExpander *base = nullptr;

    BacklightExpander *getBase() { return base; }
};

// Expands the production custom callback while routing writes into the real
// CH422G C driver above. No mock brightness implementation duplicates it.
struct Board {
    BacklightAdapter *adapter = nullptr;

    BacklightAdapter *getIO_Expander() { return adapter; }
};

bool setBacklight(int percent, void *user_data)
    ESP_PANEL_BOARD_BACKLIGHT_CUSTOM_FUNCTION(percent, user_data)

void assertEvent(size_t index, uint8_t address, uint8_t value) {
    TEST_ASSERT_NOT_NULL(g_transport);
    TEST_ASSERT_LESS_THAN_UINT32(g_transport->event_count, index);
    TEST_ASSERT_EQUAL_HEX8(address, g_transport->events[index].address);
    TEST_ASSERT_EQUAL_HEX8(value, g_transport->events[index].value);
}

void assertResetPrefix(size_t offset, size_t count) {
    const WriteEvent expected[] = {
        {kWriteOcAddress, 0x0FU},
        {kWriteIoAddress, kExpectedInitialIo},
        {kWriteSetAddress, 0x01U},
    };
    TEST_ASSERT_TRUE(count <= 3U);
    for (size_t index = 0; index < count; ++index) {
        assertEvent(offset + index, expected[index].address, expected[index].value);
    }
}

void assertUsbRemainsSelected() {
    TEST_ASSERT_NOT_NULL(g_transport);
    for (size_t index = 0; index < g_transport->event_count; ++index) {
        const auto &event = g_transport->events[index];
        if (event.address == kWriteIoAddress) {
            TEST_ASSERT_EQUAL_HEX8(0U, event.value & kUsbSelMask);
        }
    }
}

uint32_t outputShadow(esp_io_expander_handle_t handle) {
    uint32_t value = 0;
    TEST_ASSERT_EQUAL_INT(ESP_OK, handle->read_output_reg(handle, &value));
    return value;
}

esp_io_expander_handle_t createExpander() {
    esp_io_expander_handle_t handle = nullptr;
    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        esp_io_expander_new_i2c_ch422g(I2C_NUM_0, kWriteSetAddress, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    return handle;
}

} // namespace

esp_err_t aura_ch422g_test_write(i2c_port_t,
                                 uint8_t address,
                                 const uint8_t *data,
                                 size_t size,
                                 TickType_t) {
    TEST_ASSERT_NOT_NULL(g_transport);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT32(1U, size);
    TEST_ASSERT_LESS_THAN_UINT32(64U, g_transport->event_count);
    g_transport->events[g_transport->event_count++] = {address, data[0]};
    if (g_transport->fail_call != 0U &&
        g_transport->event_count == g_transport->fail_call) {
        return g_transport->failure;
    }
    return ESP_OK;
}

esp_err_t aura_ch422g_test_read(i2c_port_t,
                                uint8_t,
                                uint8_t *data,
                                size_t size,
                                TickType_t) {
    TEST_ASSERT_NOT_NULL(g_transport);
    ++g_transport->read_count;
    if (data == nullptr || size != 1U) {
        return ESP_ERR_INVALID_ARG;
    }
    data[0] = 0U;
    return ESP_OK;
}

void setUp() {
    g_transport = nullptr;
}

void tearDown() {}

void test_policy_keeps_usb_and_backlight_low_without_changing_other_outputs() {
    TEST_ASSERT_EQUAL_HEX8(kUsbSelMask, AURA_CH422G_USB_SEL_MASK);
    TEST_ASSERT_EQUAL_HEX8(kExpectedInitialIo, AURA_CH422G_INITIAL_IO_VALUE);
    TEST_ASSERT_EQUAL_HEX8(
        0U, AURA_CH422G_INITIAL_IO_VALUE & AURA_CH422G_USB_SEL_MASK);
    TEST_ASSERT_EQUAL_HEX8(0x04U, AURA_CH422G_BACKLIGHT_MASK);
    TEST_ASSERT_EQUAL_HEX8(
        0U, AURA_CH422G_INITIAL_IO_VALUE & AURA_CH422G_BACKLIGHT_MASK);
#if defined(AURA_HARDWARE_PROFILE_7) && AURA_HARDWARE_PROFILE_7
    TEST_ASSERT_EQUAL_HEX8(0xD1U, AURA_CH422G_INITIAL_IO_VALUE);
#else
    TEST_ASSERT_EQUAL_HEX8(
        AURA_CH422G_BACKLIGHT_MASK, 0xDFU ^ AURA_CH422G_INITIAL_IO_VALUE);
#endif
}

void test_board_uses_custom_backlight_with_initial_off_and_same_pin() {
    TEST_ASSERT_EQUAL_INT(
        ESP_PANEL_BACKLIGHT_TYPE_CUSTOM, ESP_PANEL_BOARD_BACKLIGHT_TYPE);
    TEST_ASSERT_EQUAL_INT(1, ESP_PANEL_BOARD_BACKLIGHT_IDLE_OFF);
    TEST_ASSERT_EQUAL_INT(2, ESP_PANEL_BOARD_BACKLIGHT_IO);
    TEST_ASSERT_EQUAL_INT(1, ESP_PANEL_BOARD_BACKLIGHT_ON_LEVEL);
    TEST_ASSERT_EQUAL_HEX8(
        AURA_CH422G_BACKLIGHT_MASK, 1U << ESP_PANEL_BOARD_BACKLIGHT_IO);
}

void test_custom_backlight_off_is_quiet_and_later_on_off_only_changes_exio2() {
    FakeTransport transport;
    g_transport = &transport;
    const auto handle = createExpander();
    BacklightExpander expander{handle};
    BacklightAdapter adapter{&expander};
    Board board{&adapter};
    const size_t initial_writes = transport.event_count;

    TEST_ASSERT_TRUE(setBacklight(0, &board));
    TEST_ASSERT_EQUAL_UINT32(initial_writes, transport.event_count);
    TEST_ASSERT_EQUAL_HEX32(kExpectedInitialOutput, outputShadow(handle));

    TEST_ASSERT_TRUE(setBacklight(100, &board));
    TEST_ASSERT_EQUAL_UINT32(initial_writes + 2U, transport.event_count);
    TEST_ASSERT_EQUAL_HEX32(
        kExpectedInitialOutput | AURA_CH422G_BACKLIGHT_MASK, outputShadow(handle));
    assertEvent(initial_writes, kWriteOcAddress, 0x0FU);
    assertEvent(initial_writes + 1U, kWriteIoAddress,
                kExpectedInitialIo | AURA_CH422G_BACKLIGHT_MASK);

    TEST_ASSERT_TRUE(setBacklight(0, &board));
    TEST_ASSERT_EQUAL_UINT32(initial_writes + 4U, transport.event_count);
    TEST_ASSERT_EQUAL_HEX32(kExpectedInitialOutput, outputShadow(handle));
    TEST_ASSERT_EQUAL_UINT32(3U, expander.write_calls);
    TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
    assertUsbRemainsSelected();
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
}

void test_custom_backlight_rejects_missing_board_adapter_or_base_without_writes() {
    FakeTransport transport;
    g_transport = &transport;
    Board board{};
    BacklightAdapter adapter{};
    TEST_ASSERT_FALSE(setBacklight(100, nullptr));
    TEST_ASSERT_FALSE(setBacklight(100, &board));
    board.adapter = &adapter;
    TEST_ASSERT_FALSE(setBacklight(100, &board));
    TEST_ASSERT_EQUAL_UINT32(0U, transport.event_count);
    TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
}

void test_custom_backlight_propagates_failed_write_without_retry() {
    FakeTransport transport;
    g_transport = &transport;
    const auto handle = createExpander();
    BacklightExpander expander{handle};
    BacklightAdapter adapter{&expander};
    Board board{&adapter};
    const size_t initial_writes = transport.event_count;
    transport.fail_call = initial_writes + 2U;

    TEST_ASSERT_FALSE(setBacklight(100, &board));
    TEST_ASSERT_EQUAL_UINT32(initial_writes + 2U, transport.event_count);
    TEST_ASSERT_EQUAL_UINT32(1U, expander.write_calls);
    TEST_ASSERT_EQUAL_HEX32(kExpectedInitialOutput, outputShadow(handle));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
}

void test_profile_image_is_preloaded_before_enabling_outputs() {
    FakeTransport transport;
    g_transport = &transport;
    const auto handle = createExpander();

    TEST_ASSERT_EQUAL_UINT32(3U, transport.event_count);
    assertResetPrefix(0U, 3U);
    TEST_ASSERT_EQUAL_HEX32(kExpectedInitialOutput, outputShadow(handle));

    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        esp_io_expander_ch422g_set_all_output(handle));
    TEST_ASSERT_EQUAL_UINT32(4U, transport.event_count);
    assertEvent(3U, kWriteSetAddress, 0x01U);
    TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
    assertUsbRemainsSelected();
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
}

void test_repeated_reset_restores_profile_image_without_usb_high() {
    FakeTransport transport;
    g_transport = &transport;
    const auto handle = createExpander();

    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        esp_io_expander_set_level(handle, 0x1EU, 0U));
    TEST_ASSERT_EQUAL_HEX32(0x0FC1U, outputShadow(handle));

    for (size_t repeat = 0; repeat < 3U; ++repeat) {
        const size_t offset = transport.event_count;
        TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_reset(handle));
        TEST_ASSERT_EQUAL_UINT32(offset + 3U, transport.event_count);
        assertResetPrefix(offset, 3U);
        TEST_ASSERT_EQUAL_HEX32(kExpectedInitialOutput, outputShadow(handle));
    }

    TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
    assertUsbRemainsSelected();
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
}

void test_each_constructor_reset_failure_stops_at_failed_stage_without_retry() {
    const esp_err_t failures[] = {ESP_FAIL, ESP_ERR_TIMEOUT};
    for (const auto failure : failures) {
        for (size_t fail_call = 1U; fail_call <= 3U; ++fail_call) {
            FakeTransport transport;
            transport.fail_call = fail_call;
            transport.failure = failure;
            g_transport = &transport;
            esp_io_expander_handle_t handle = nullptr;

            TEST_ASSERT_EQUAL_INT(
                failure,
                esp_io_expander_new_i2c_ch422g(I2C_NUM_0, kWriteSetAddress, &handle));
            TEST_ASSERT_NULL(handle);
            TEST_ASSERT_EQUAL_UINT32(fail_call, transport.event_count);
            TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
            assertResetPrefix(0U, fail_call);
            assertUsbRemainsSelected();
        }
    }
}

void test_each_repeated_reset_failure_stops_at_failed_stage_without_retry() {
    const esp_err_t failures[] = {ESP_FAIL, ESP_ERR_TIMEOUT};
    for (const auto failure : failures) {
        for (size_t fail_call = 1U; fail_call <= 3U; ++fail_call) {
            FakeTransport transport;
            g_transport = &transport;
            const auto handle = createExpander();
            transport = FakeTransport{};
            transport.fail_call = fail_call;
            transport.failure = failure;

            TEST_ASSERT_EQUAL_INT(failure, esp_io_expander_reset(handle));
            TEST_ASSERT_EQUAL_UINT32(fail_call, transport.event_count);
            TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
            assertResetPrefix(0U, fail_call);
            assertUsbRemainsSelected();
            TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
        }
    }
}

void test_masked_writes_to_other_exio_preserve_usb_and_unselected_bits() {
    FakeTransport transport;
    g_transport = &transport;
    const auto handle = createExpander();
    const uint32_t masks[] = {0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x40U, 0x80U};

    for (const auto mask : masks) {
        const uint32_t before = outputShadow(handle);
        const uint8_t original_level = (before & mask) != 0U ? 1U : 0U;
        const uint32_t changed = before ^ mask;
        const size_t offset = transport.event_count;

        TEST_ASSERT_EQUAL_INT(
            ESP_OK, esp_io_expander_set_level(handle, mask, 1U - original_level));
        TEST_ASSERT_EQUAL_UINT32(offset + 2U, transport.event_count);
        assertEvent(offset, kWriteOcAddress, 0x0FU);
        assertEvent(offset + 1U, kWriteIoAddress, static_cast<uint8_t>(changed));
        TEST_ASSERT_EQUAL_HEX32(changed, outputShadow(handle));
        TEST_ASSERT_EQUAL_HEX32(0U, (before ^ outputShadow(handle)) & ~mask);

        TEST_ASSERT_EQUAL_INT(
            ESP_OK, esp_io_expander_set_level(handle, mask, original_level));
        TEST_ASSERT_EQUAL_UINT32(offset + 4U, transport.event_count);
        assertEvent(offset + 2U, kWriteOcAddress, 0x0FU);
        assertEvent(offset + 3U, kWriteIoAddress, static_cast<uint8_t>(before));
        TEST_ASSERT_EQUAL_HEX32(before, outputShadow(handle));
    }

    TEST_ASSERT_EQUAL_UINT32(0U, transport.read_count);
    assertUsbRemainsSelected();
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_io_expander_del(handle));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_policy_keeps_usb_and_backlight_low_without_changing_other_outputs);
    RUN_TEST(test_board_uses_custom_backlight_with_initial_off_and_same_pin);
    RUN_TEST(test_custom_backlight_off_is_quiet_and_later_on_off_only_changes_exio2);
    RUN_TEST(test_custom_backlight_rejects_missing_board_adapter_or_base_without_writes);
    RUN_TEST(test_custom_backlight_propagates_failed_write_without_retry);
    RUN_TEST(test_profile_image_is_preloaded_before_enabling_outputs);
    RUN_TEST(test_repeated_reset_restores_profile_image_without_usb_high);
    RUN_TEST(test_each_constructor_reset_failure_stops_at_failed_stage_without_retry);
    RUN_TEST(test_each_repeated_reset_failure_stops_at_failed_stage_without_retry);
    RUN_TEST(test_masked_writes_to_other_exio_preserve_usb_and_unselected_bits);
    return UNITY_END();
}
