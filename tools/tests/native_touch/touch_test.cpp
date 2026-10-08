// Exercise unmodified managed Espressif C drivers with fault-injecting I2C.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "esp_lcd_touch.h"
#include <vector>
#include <mutex>
#define ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS() ((void)0)
#define ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS() ((void)0)
#define ESP_UTILS_LOGD(...) ((void)0)
#define ESP_UTILS_LOGW(...) ((void)0)
#define ESP_UTILS_CHECK_ERROR_RETURN(expr, result, ...) do { if ((expr) != ESP_OK) return (result); } while (0)
#define ESP_UTILS_CONF_LOG_LEVEL 0
#define ESP_UTILS_LOG_LEVEL_DEBUG 4
namespace utils { template<class T> using vector = std::vector<T>; }
struct Point {
    int x, y, strength;
    Point(int px, int py, int ps): x(px), y(py), strength(ps) {}
};
class Touch {
public:
    esp_lcd_touch_handle_t touch_panel;
    static constexpr int POINTS_MAX_NUM = 5;
    struct Attributes { int max_points_num; };
    Attributes getBasicAttributes() { return {5}; }
    bool readRawDataPoints(int points_num);
    std::mutex _resource_mutex;
    std::vector<Point> _points;
};
// Exact generated production method; the surrounding fixture provides only
// Display Panel logging, storage and locking, not a reimplementation of reading.
#include "wrapper_method.inc"

#include <cstring>
#include "esp_lcd_touch_gt911.h"
#include "gt911_address_macro.inc"
#include "core/Gt911RuntimePolicy.h"
using namespace Gt911RuntimePolicy;
static uint8_t status_byte, records[40], keys[4];
static int fail_read_reg = -1, fail_write_reg = -1, reads, writes;
extern "C" esp_err_t esp_lcd_panel_io_rx_param(void *, int reg, void *out, size_t n) {
    ++reads;
    if (reg == fail_read_reg) return ESP_ERR_TIMEOUT;
    if (reg == 0x814e) { assert(n == 1); *(uint8_t *)out = status_byte; }
    else if (reg == 0x814f) { assert(n <= sizeof records); memcpy(out, records, n); }
    else if (reg == 0x8093) { assert(n <= sizeof keys); memcpy(out, keys, n); }
    else memset(out, 0, n); // Product/config reads during construction.
    return ESP_OK;
}
extern "C" esp_err_t esp_lcd_panel_io_tx_param(void *, int reg, const void *data, size_t n) {
    ++writes;
    if (reg == fail_write_reg) return ESP_ERR_TIMEOUT;
    if (reg == 0x814e) { assert(n == 1 && *(const uint8_t *)data == 0); status_byte = 0; }
    return ESP_OK;
}
static void frame(uint8_t count) {
    status_byte = 0x80 | count;
    for (int i = 0; i < 5; ++i) {
        const uint16_t x = 100 + i, y = 200 + i, strength = 300 + i;
        const uint8_t p[8] = {uint8_t(7+i), uint8_t(x), uint8_t(x>>8), uint8_t(y),
                             uint8_t(y>>8), uint8_t(strength), uint8_t(strength>>8), 0};
        memcpy(records + 8*i, p, 8);
    }
}
static esp_err_t track_error(esp_lcd_touch_handle_t, uint8_t *, uint8_t) { return ESP_FAIL; }
int main() {
    esp_lcd_touch_point_data_t data[5]{};
    uint8_t count = 0;
    assert(esp_lcd_touch_get_data(nullptr, data, &count, 1) == ESP_ERR_INVALID_ARG);
    unsigned cases = 1;
    // Both canonical profile addresses, same official code and 800x480 geometry.
    for (uint8_t address : {uint8_t(0x14), uint8_t(0x5d)}) {
        esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG_WITH_ADDR(address);
        assert(io_config.dev_addr == address && io_config.scl_speed_hz == 100000);
        assert(io_config.control_phase_bytes == 1 && io_config.lcd_cmd_bits == 16);
        assert(io_config.flags.disable_control_phase); ++cases;
        esp_lcd_touch_io_gt911_config_t driver = {address};
        esp_lcd_touch_config_t config{};
        config.x_max = 800; config.y_max = 480;
        config.rst_gpio_num = config.int_gpio_num = GPIO_NUM_NC;
        config.driver_data = &driver;
        esp_lcd_touch_handle_t tp = nullptr;
        assert(esp_lcd_touch_new_i2c_gt911((void*)1, &config, &tp) == ESP_OK);

        frame(1);
        assert(esp_lcd_touch_read_data(tp) == ESP_OK && status_byte == 0);
        assert(esp_lcd_touch_get_data(tp, data, &count, 5) == ESP_OK && count == 1);
        assert(data[0].x == 100 && data[0].y == 200 && data[0].strength == 300 && data[0].track_id == 7);
        Touch wrapper;
        wrapper.touch_panel = tp;
        assert(wrapper.readRawDataPoints(1) && wrapper._points.size() == 1);
        assert(wrapper._points[0].x == 100 && wrapper._points[0].y == 200);
        // Display Panel historically consumes a sample once. A repeated vendor
        // read with no fresh frame must not replay the preceding press.
        assert(wrapper.readRawDataPoints(1) && wrapper._points.empty());
        frame(1);
        assert(esp_lcd_touch_read_data(tp) == ESP_OK); // Refill for direct API tests.
        ++cases;
        // New API deliberately retains the sample; a non-ready frame is NOT release.
        assert(esp_lcd_touch_get_data(tp, data, &count, 5) == ESP_OK && count == 1);
        const int prior_reads = reads;
        assert(vendorReadAction(status_byte) == VendorReadAction::Skip);
        assert(reconcileFrame(status_byte, count).kind == ReconciledFrameKind::NoData);
        assert(reads == prior_reads); ++cases;

        for (unsigned flags = 0; flags < 8; ++flags) {
            tp->config.flags.mirror_x = flags & 1;
            tp->config.flags.mirror_y = (flags >> 1) & 1;
            tp->config.flags.swap_xy = (flags >> 2) & 1;
            assert(esp_lcd_touch_get_data(tp, data, &count, 1) == ESP_OK && count == 1);
            uint16_t x = flags & 1 ? 700 : 100, y = flags & 2 ? 280 : 200;
            assert(data[0].x == (flags & 4 ? y : x) && data[0].y == (flags & 4 ? x : y));
            assert(data[0].track_id == 7); ++cases;
        }
        tp->config.flags = {};
        frame(0);
        assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        count = 5;
        assert(esp_lcd_touch_get_data(tp, data, &count, 5) == ESP_OK && count == 0);
        assert(reconcileFrame(0x80, count).kind == ReconciledFrameKind::Released);
        assert(wrapper.readRawDataPoints(1) && wrapper._points.empty()); ++cases;

        frame(5);
        assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        assert(esp_lcd_touch_get_data(tp, data, &count, 5) == ESP_OK && count == 5);
        for (unsigned i=0; i<5; ++i) assert(data[i].track_id == 7+i && data[i].x == 100+i);
        assert(esp_lcd_touch_get_data(tp, data, &count, 1) == ESP_OK && count == 1);
        assert(wrapper.readRawDataPoints(100) && wrapper._points.size() == 5);
        frame(5); assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        assert(wrapper.readRawDataPoints(-1) && wrapper._points.size() == 5); ++cases;

        frame(6);
        assert(esp_lcd_touch_read_data(tp) == ESP_OK && status_byte == 0);
        assert(esp_lcd_touch_get_data(tp, data, &count, 5) == ESP_OK && count == 0);
        assert(reconcileFrame(0x86, count).kind == ReconciledFrameKind::Error); ++cases;

        // Each bus failure is an error, never a fabricated release/press.
        for (int reg : {0x814e, 0x814f}) {
            frame(1); fail_read_reg = reg;
            assert(esp_lcd_touch_read_data(tp) == ESP_ERR_TIMEOUT);
            assert(reconcileFrame(0x81, -1).kind == ReconciledFrameKind::Error);
            fail_read_reg = -1; ++cases;
        }
        frame(1); fail_write_reg = 0x814e;
        assert(esp_lcd_touch_read_data(tp) == ESP_ERR_TIMEOUT);
        fail_write_reg = -1;
        // Retry succeeds and returns this frame, including track ID.
        assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        assert(esp_lcd_touch_get_data(tp, data, &count, 1) == ESP_OK && count == 1); ++cases;
        auto get_track_id = tp->get_track_id; tp->get_track_id = track_error;
        assert(esp_lcd_touch_get_data(tp, data, &count, 1) == ESP_FAIL);
        assert(!wrapper.readRawDataPoints(1)); // New API errors propagate through the wrapper.
        tp->get_track_id = get_track_id; ++cases;
        assert(esp_lcd_touch_get_data(tp, data, &count, 0) == ESP_ERR_INVALID_ARG); ++cases;

        // Upstream key decoding now reads each key's own byte.
        frame(1); assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        assert(wrapper.readRawDataPoints(1) && wrapper._points.size() == 1);
        status_byte = 0x90; keys[0]=0; keys[1]=1; keys[2]=0; keys[3]=1;
        assert(esp_lcd_touch_read_data(tp) == ESP_OK);
        assert(wrapper.readRawDataPoints(1) && wrapper._points.empty());
        for (int i=0; i<4; ++i) { uint8_t key=9; assert(esp_lcd_touch_get_button_state(tp,i,&key)==ESP_OK && key==keys[i]); }
        ++cases;
        assert(esp_lcd_touch_del(tp) == ESP_OK);
    }
    printf("%u official touch API scenarios passed\n", cases);
}
