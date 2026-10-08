// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// Host differential test of the exact renderer compiled into the firmware.
#include <lvgl.h>
#include "display/NativeRotation.h"
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include "ui/screens.h"
#include "ui/images.h"
#include "ui/fonts.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6 || LVGL_VERSION_PATCH != 0
#error Revalidate this experiment for the changed LVGL version.
#endif
#if LV_USE_OS != LV_OS_NONE || LV_DRAW_SW_DRAW_UNIT_CNT != 1 || LV_DRAW_TRANSFORM_USE_MATRIX || LV_USE_VECTOR_GRAPHIC || LV_USE_DRAW_SW_ASM
#error This experiment requires the inspected Aura software renderer configuration.
#endif

constexpr int W=800, H=480, N=W*H, GUARD=32;
static int stride=W*2, angle=180;
static bool native=false, bad_sync=false, reference_rotation=false;
static uint64_t sync_pixels=0;
static unsigned frames=0, flushes=0, syncs=0, candidate_rotations=0, reference_rotations=0, intermediate_draws=0;
static std::array<std::vector<uint16_t>,2> storage;
static std::array<uint16_t*,2> buffers;
static uint16_t *presented=nullptr;
static FILE *reference_file=nullptr;
static std::filesystem::path output;

extern "C" void __real_lv_draw_rotate(const void *,void *,int32_t,int32_t,int32_t,int32_t,lv_display_rotation_t,lv_color_format_t);
extern "C" void __wrap_lv_draw_rotate(const void *s,void *d,int32_t w,int32_t h,int32_t ss,int32_t ds,lv_display_rotation_t r,lv_color_format_t cf) {
    if(reference_rotation) ++reference_rotations; else ++candidate_rotations;
    __real_lv_draw_rotate(s,d,w,h,ss,ds,r,cf);
}

extern "C" void __real_lv_display_sync_ready(lv_display_t *);
extern "C" void __wrap_lv_display_sync_ready(lv_display_t *d) {
    ++syncs;
    __real_lv_display_sync_ready(d);
}

static bool is_root(const void *p) { return p==buffers[0] || p==buffers[1]; }
static void check(bool ok, const char *reason) {
    if(!ok) { std::fprintf(stderr,"FAIL frame=%u: %s\n",frames,reason); std::exit(2); }
}
// Observe real draw-unit buffer acquisition, including intermediate ARGB layers.
extern "C" void *__real_lv_draw_layer_alloc_buf(lv_layer_t *);
extern "C" void *__wrap_lv_draw_layer_alloc_buf(lv_layer_t *l) {
    void *p=__real_lv_draw_layer_alloc_buf(l);
    if(p && is_root(p)) check(p!=presented,"draw unit acquired the presented buffer");
    else if(p) ++intermediate_draws;
    return p;
}
static void guards() {
    for(const auto &b:storage) for(int i=0;i<GUARD;i++) {
        check(b[i]==0xa55a && b[b.size()-1-i]==0xa55a,"framebuffer guard overwritten");
    }
    for(auto b:buffers) for(int y=0;y<H;y++) for(int x=W;x<stride/2;x++)
        check(b[y*stride/2+x]==0xa55a,"row padding overwritten");
}

static void sync(lv_display_t *d,const lv_area_t *logical) {
    auto *dst=(uint16_t*)lv_display_get_buf_active(d)->data;
    check(presented && dst!=presented && is_root(dst),"invalid sync ownership");
    lv_area_t a=*logical;
    if(angle==180 && !bad_sync) a={W-1-logical->x2,H-1-logical->y2,W-1-logical->x1,H-1-logical->y1};
    check(a.x1>=0 && a.y1>=0 && a.x2<W && a.y2<H,"sync outside frame");
    for(int y=a.y1;y<=a.y2;y++) {
        auto n=(a.x2-a.x1+1);
        std::memcpy(dst+y*stride/2+a.x1,presented+y*stride/2+a.x1,n*2);
        sync_pixels+=n;
    }
    lv_display_sync_ready(d);
}
static void native_fault(const char *reason) { check(false,reason); }
static void flush(lv_display_t *d,const lv_area_t *,uint8_t *pixels) {
    if(lv_display_flush_is_last(d)) {
        check(is_root(pixels),"flush did not submit a display buffer");
        if(native) {
            check(NativeRotation::canSubmit(pixels),"invalid submitted buffer");
            check(!NativeRotation::acknowledge(nullptr),"accepted foreign acknowledgement");
            check(NativeRotation::acknowledge(pixels),"acknowledgement rejected");
            check(!NativeRotation::acknowledge(pixels),"accepted duplicate acknowledgement");
        }
        presented=(uint16_t*)pixels; ++flushes;
    }
    lv_display_flush_ready(d);
}
static void ppm(const char *name,const uint16_t *p) {
    FILE *f=std::fopen((output/name).string().c_str(),"wb"); check(f,"cannot save capture");
    std::fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<N;i++) {
        uint16_t c=p[i]; uint8_t rgb[]={(uint8_t)(((c>>11)&31)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};
        check(std::fwrite(rgb,1,3,f)==3,"capture write failed");
    }
    std::fclose(f);
}
static void frame(lv_display_t *d,const char *name) {
    lv_refr_now(d); guards(); check(presented,"nothing presented");
    std::vector<uint16_t> actual(N), expected(N), rotated(N);
    for(int y=0;y<H;y++) std::memcpy(actual.data()+y*W,presented+y*stride/2,W*2);
    if(!native) check(std::fwrite(actual.data(),2,N,reference_file)==N,"reference write failed");
    else {
        check(std::fread(expected.data(),2,N,reference_file)==N,"reference read failed");
        const uint16_t *want=expected.data();
        if(angle==180) {
            reference_rotation=true;
            lv_draw_rotate(expected.data(),rotated.data(),W,H,W*2,W*2,LV_DISPLAY_ROTATION_180,LV_COLOR_FORMAT_RGB565);
            reference_rotation=false; want=rotated.data();
        }
        unsigned differences=0;
        for(int i=0;i<N;i++) differences+=want[i]!=actual[i];
        if(differences) {
            ppm("mismatch-expected.ppm",want); ppm("mismatch-actual.ppm",actual.data());
            std::fprintf(stderr,"MISMATCH case=%s frame=%u pixels=%u\n",name,frames,differences);
            std::exit(3);
        }
        check(candidate_rotations==0,"candidate called a post-render rotation");
    }
    if(frames==2) ppm("main.ppm",actual.data());
    std::printf("frame=%u case=%s ok\n",frames++,name);
}
static void log_message(lv_log_level_t level,const char *message) {
    std::fputs(message,stderr);
    if(level>=LV_LOG_LEVEL_WARN && !std::strstr(message,"Deprecated:")) std::abort();
}
int main(int argc,char **argv) {
    if(argc!=6) return 1;
    native=std::string(argv[1])!="reference";
    angle=std::atoi(argv[2]); check(angle==0 || angle==180,"bad angle");
    const bool full=std::string(argv[3])=="full";
    bad_sync=std::string(argv[3])=="bad-sync";
    stride=W*2+(std::string(argv[3])=="padded" ? 32 : 0);
    output=argv[5]; std::filesystem::create_directories(output);
    reference_file=std::fopen(argv[4],native?"rb":"wb"); check(reference_file,"cannot open reference");
    for(int i=0;i<2;i++) {
        storage[i].assign(stride/2*H+GUARD*2,0xa55a); buffers[i]=storage[i].data()+GUARD;
        for(int y=0;y<H;y++) std::fill_n(buffers[i]+y*stride/2,W,0);
    }
    lv_init(); lv_log_register_print_cb(log_message);
    auto *d=lv_display_create(W,H); check(d,"cannot create display");
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers_with_stride(d,buffers[0],buffers[1],stride*H,stride,full?LV_DISPLAY_RENDER_MODE_FULL:LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush);
    if(native) {
        presented=buffers[1];
        check(!NativeRotation::begin(d,buffers[0],angle==180,native_fault),"accepted active renderer as initial scanout");
        check(NativeRotation::begin(d,buffers[1],angle==180,native_fault),"cannot install firmware renderer");
        check(!NativeRotation::begin(d,buffers[1],angle==180,native_fault),"accepted a second initialization");
        check(!NativeRotation::canSubmit(buffers[1]),"allowed writes into initial scanout");
        if(std::string(argv[3])=="bad-draw") {
            lv_draw_buf_t scanout=*lv_display_get_buf_active(d);
            scanout.data=reinterpret_cast<uint8_t *>(buffers[1]);
            lv_layer_t layer{}; layer.draw_buf=&scanout;
            layer.buf_area={0,0,W-1,H-1}; layer.color_format=LV_COLOR_FORMAT_RGB565;
            lv_draw_task_t task{}; task.target_layer=&layer; task.clip_area={0,0,3,3};
            lv_area_t a{0,0,3,3}; lv_draw_sw_blend_dsc_t blend{};
            blend.blend_area=&a; blend.opa=LV_OPA_COVER;
            lv_draw_sw_get_blend_handler(LV_COLOR_FORMAT_RGB565)(&task,&blend);
            check(false,"active-scanout write was not rejected");
        }
        if(bad_sync) lv_display_set_sync_cb(d,sync); // Deliberately wrong mapping control.

    }
    create_screens();
    lv_obj_t *screens[]={objects.page_boot_logo,objects.page_boot_diag,objects.page_main_pro,objects.page_settings,
        objects.page_wifi,objects.page_theme,objects.page_clock,objects.page_co2_calib,objects.page_auto_night_mode,
        objects.page_backlight,objects.page_mqtt,objects.page_sensors_info,objects.page_dac_settings,objects.page_fw_update,objects.page_diag};
    for(int i=0;i<15;i++) {
        lv_screen_load(screens[i]); frame(d,("generated-"+std::to_string(i)).c_str());
    }
    lv_screen_load(objects.page_main_pro); frame(d,"main-return");
    for(int i=0;i<20;i++) {
        std::string s=std::to_string(400+i*79); lv_label_set_text(objects.label_co2_value_1,s.c_str());
        lv_label_set_text(objects.label_time_value_2,i&1?"12:34":"09:07");
        if(i%3==0) lv_obj_add_flag(objects.label_co2_unit_1,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(objects.label_co2_unit_1,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(objects.card_co2_pro,lv_color_hex(0x123028+i*71),0);
        frame(d,"main-incremental");
    }
    lv_obj_remove_flag(objects.label_co2_warmup,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.label_co2_value_1,LV_OBJ_FLAG_HIDDEN); frame(d,"warmup");
    lv_obj_add_flag(objects.label_co2_warmup,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(objects.label_co2_value_1,LV_OBJ_FLAG_HIDDEN); frame(d,"warmup-end");
    auto *s=lv_obj_create(nullptr); lv_obj_set_style_bg_color(s,lv_color_hex(0x123456),0); lv_screen_load(s);
    auto *list=lv_obj_create(s); lv_obj_set_pos(list,13,17); lv_obj_set_size(list,353,419); lv_obj_set_scroll_dir(list,LV_DIR_VER);
    std::vector<lv_obj_t*> labels;
    for(int i=0;i<9;i++) {
        auto *card=lv_obj_create(list); lv_obj_set_pos(card,0,i*83); lv_obj_set_size(card,313,77);
        lv_obj_set_style_radius(card,13,0); lv_obj_set_style_shadow_width(card,9,0);
        lv_obj_set_style_bg_color(card,lv_color_hex(0x446655+i*71),0);
        lv_obj_set_style_bg_grad_color(card,lv_color_hex(0x889977+i*17),0); lv_obj_set_style_bg_grad_dir(card,LV_GRAD_DIR_HOR,0);
        auto *l=lv_label_create(card); lv_label_set_text(l,"Aura 123 / CO2"); labels.push_back(l);
        if(i==2) lv_obj_set_style_opa(card,153,0);
        if(i==4) lv_obj_set_style_transform_rotation(card,170,0);
    }
    auto *moving=lv_label_create(s); lv_label_set_text(moving,"Small dirty region"); lv_obj_set_pos(moving,450,33);
    auto *arc=lv_arc_create(s); lv_obj_set_pos(arc,450,140); lv_obj_set_size(arc,157,157); lv_arc_set_value(arc,63);
    auto *slider=lv_slider_create(s); lv_obj_set_pos(slider,432,377); lv_obj_set_size(slider,237,19);
    frame(d,"effects-initial");
    uint32_t seed=0x20261008;
    for(int i=0;i<60;i++) {
        seed=1664525u*seed+1013904223u;
        lv_obj_scroll_to_y(list,(seed>>16)%330,LV_ANIM_OFF);
        lv_obj_set_pos(moving,397+(seed%190),11+((seed>>8)%90));
        lv_label_set_text(labels[i%9],std::to_string(seed%100000).c_str());
        lv_arc_set_value(arc,seed%101); lv_slider_set_value(slider,(seed>>8)%101,LV_ANIM_OFF);
        lv_obj_set_style_text_opa(moving,31+seed%225,0);
        frame(d,"effects-incremental");
    }
    lv_screen_load(objects.page_settings); frame(d,"settings-return");
    lv_screen_load(objects.page_main_pro); frame(d,"main-final");
    frame(d,"no-dirty-update");
    guards();
    if(native) {
        check(std::fgetc(reference_file)==EOF,"reference has unconsumed frames");
        check(intermediate_draws>0,"intermediate layers were not exercised");
        if(!full) check(syncs>0,"production sync callback was not exercised");
    }
    std::fclose(reference_file);
    auto path=output/"stats.json"; FILE *stats=std::fopen(path.string().c_str(),"wb"); check(stats,"cannot save statistics");
    std::fprintf(stats,"{\"frames\":%u,\"flushes\":%u,\"syncs\":%u,\"candidate_rotation_calls\":%u,\"reference_rotation_calls\":%u,\"display_framebuffers\":2,\"stride\":%d,\"angle\":%d,\"intermediate_draws\":%u}\n",frames,flushes,syncs,candidate_rotations,reference_rotations,stride,angle,intermediate_draws);
    std::fclose(stats);
    // This process-lifetime rendering fixture deliberately leaves LVGL alive.
    // The separate unmodified-LVGL canvas_cleanup test reproduces the 9.6
    // destructor's pointer-to-pointer cache-drop error without this hook.
    // Do not claim lifecycle validation from the differential rendering run.
    std::printf("PASS frames=%u candidate_rotations=%u syncs=%u\n",frames,candidate_rotations,syncs);
}
