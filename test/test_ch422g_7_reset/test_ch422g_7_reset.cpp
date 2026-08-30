#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "driver/i2c.h"
#include "../../third_party/ESP32_IO_Expander_7/src/port/esp_io_expander_ch422g.h"

namespace {

struct WriteEvent {
    uint8_t address = 0;
    uint8_t value = 0;
};

struct FakeTransport {
    WriteEvent events[8]{};
    size_t event_count = 0;
    size_t fail_call = 0;
    esp_err_t failure = ESP_FAIL;
};

FakeTransport *g_transport = nullptr;

void assertEvent(size_t index, uint8_t address, uint8_t value) {
    TEST_ASSERT_NOT_NULL(g_transport);
    TEST_ASSERT_LESS_THAN_UINT32(g_transport->event_count, index);
    TEST_ASSERT_EQUAL_HEX8(address, g_transport->events[index].address);
    TEST_ASSERT_EQUAL_HEX8(value, g_transport->events[index].value);
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
    TEST_ASSERT_LESS_THAN_UINT32(8U, g_transport->event_count);
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

void test_7_profile_preloads_d1_before_enabling_outputs() {
    FakeTransport transport;
    g_transport = &transport;
    esp_io_expander_handle_t handle = nullptr;

    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        esp_io_expander_new_i2c_ch422g(I2C_NUM_0, 0x24U, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL_UINT32(3U, transport.event_count);
    assertEvent(0U, 0x23U, 0x0FU);
    assertEvent(1U, 0x38U, 0xD1U);
    assertEvent(2U, 0x24U, 0x01U);
    for (size_t index = 0; index < transport.event_count; ++index) {
        TEST_ASSERT_FALSE(transport.events[index].address == 0x38U &&
                          transport.events[index].value == 0xFFU);
    }

    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        esp_io_expander_ch422g_set_all_output(handle));
    TEST_ASSERT_EQUAL_UINT32(4U, transport.event_count);
    assertEvent(3U, 0x24U, 0x01U);
    TEST_ASSERT_EQUAL_INT(ESP_OK, handle->del(handle));
}

void test_each_reset_failure_stops_without_retry() {
    for (size_t fail_call = 1U; fail_call <= 3U; ++fail_call) {
        FakeTransport transport;
        transport.fail_call = fail_call;
        transport.failure = ESP_ERR_TIMEOUT;
        g_transport = &transport;
        esp_io_expander_handle_t handle = nullptr;

        TEST_ASSERT_EQUAL_INT(
            ESP_ERR_TIMEOUT,
            esp_io_expander_new_i2c_ch422g(I2C_NUM_0, 0x24U, &handle));
        TEST_ASSERT_NULL(handle);
        TEST_ASSERT_EQUAL_UINT32(fail_call, transport.event_count);
        if (fail_call < 3U) {
            for (size_t index = 0; index < transport.event_count; ++index) {
                TEST_ASSERT_NOT_EQUAL(0x24U, transport.events[index].address);
            }
        }
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_7_profile_preloads_d1_before_enabling_outputs);
    RUN_TEST(test_each_reset_failure_stops_without_retry);
    return UNITY_END();
}
