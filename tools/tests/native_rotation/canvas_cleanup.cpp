// Host-only minimal reproduction of a pre-existing LVGL 9.6 canvas cleanup
// error. Uses unmodified LVGL; no native rotation hook is installed.
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

static unsigned invalid_magic=0, other_errors=0;
static void log_message(lv_log_level_t level,const char *message) {
    std::fputs(message,stderr);
    if(level==LV_LOG_LEVEL_ERROR) {
        if(std::strstr(message,"lv_image_src_get_type") && std::strstr(message,"invalid magic")) ++invalid_magic;
        else ++other_errors;
    }
}
int main() {
    lv_init(); lv_log_register_print_cb(log_message);
    auto *d=lv_display_create(32,32);
    // Make the first byte of the stored pointer 0x10 deterministically. The
    // destructor passes the address of this pointer as if it were an image.
    alignas(256) static uint8_t descriptor[sizeof(lv_draw_buf_t)+256]{};
    alignas(64) static uint16_t pixels[64]{};
    auto *buf=reinterpret_cast<lv_draw_buf_t*>(descriptor+16);
    if((reinterpret_cast<uintptr_t>(buf)&255)!=16) return 2;
    if(lv_draw_buf_init(buf,8,8,LV_COLOR_FORMAT_RGB565,16,pixels,sizeof(pixels))!=LV_RESULT_OK) return 2;
    auto *canvas=lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas,buf);
    lv_obj_delete(canvas);
    lv_display_delete(d); lv_deinit();
    std::printf("stock_canvas_cleanup_invalid_magic=%u other_errors=%u native_hook=absent\n",invalid_magic,other_errors);
    return invalid_magic==2 && other_errors==0 ? 0 : 1;
}
