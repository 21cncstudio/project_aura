#!/usr/bin/env python3
"""Pre-integration audit of pinned Espressif LVGL adapter; never opens a device.

Extracts and executes the real upstream RGB565 rotation and wait functions.
Cross-compiles upstream translation units using Aura's existing S3 compile DB.
This is a compatibility probe, not a firmware installation or performance claim.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_SHA256 = "a2dbe6f26d4a28e7ef53ca01428f4f1a67e7ab947f85457cfa121f25cca81957"

def function(source, name):
    pattern = r"^(?:static\s+)?(?:inline\s+)?(?:void|bool|int|size_t|uint32_t|struct display_pipeline_buf\s*\*)\s*(?:IRAM_ATTR\s*)?" + re.escape(name) + r"\([^;{}]*\)\s*\{"
    matches = list(re.finditer(pattern, source, re.M))
    if len(matches) != 1:
        raise ValueError((name, len(matches)))
    start = matches[0].start()
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"

PREFIX = r"""
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>
#include <random>
#include <stdexcept>
#include <vector>
#define IRAM_ATTR
#define ESP_LOGW(...)
#define pdTRUE 1
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
enum esp_lv_adapter_rotation_t { ESP_LV_ADAPTER_ROTATE_0=0,
 ESP_LV_ADAPTER_ROTATE_90=90, ESP_LV_ADAPTER_ROTATE_180=180, ESP_LV_ADAPTER_ROTATE_270=270 };
struct display_pipeline_buf { int value; };
struct esp_lv_adapter_display_pipeline_t {};
static display_pipeline_buf buffer;
static bool ready = false, release_on_wait = false;
static uint32_t wait_ticks = 0;
void ulTaskNotifyValueClear(void*, unsigned long) {}
display_pipeline_buf *display_bridge_pipeline_take_free_buf(void*) { return ready ? &buffer : nullptr; }
void ulTaskNotifyTake(int, uint32_t ticks) {
 wait_ticks = ticks;
 if (release_on_wait) { ready = true; return; }
 throw std::runtime_error("missing completion");
}
"""

CASES = r"""
int main() {
 std::mt19937 rng(0xA07A);
 constexpr int W=800, H=480;
 unsigned cases=0;
 auto check=[&](int x, int y, int w, int h, int pad) {
  const int stride=w+pad;
  std::vector<uint16_t> src(stride*h);
  for(auto &v:src) v=static_cast<uint16_t>(rng());
  std::vector<uint16_t> result(W*H+32,0xdead), expected(result);
  for(int yy=0;yy<h;++yy) for(int xx=0;xx<w;++xx)
   expected[16+(H-1-y-yy)*W+(W-1-x-xx)]=src[yy*stride+xx];
  display_rotate_copy_region(src.data(),result.data()+16,x,y,x+w-1,y+h-1,
                             stride,W,H,ESP_LV_ADAPTER_ROTATE_180,2,16,32);
  assert(result==expected);
  ++cases;
 };
 check(0,0,W,H,0);
 check(0,0,1,1,0); check(W-1,H-1,1,1,7);
 check(0,H-1,W,1,3); check(W-1,0,1,H,5);
 for(int i=0;i<1000;++i) {
  int x=rng()%W,y=rng()%H;
  int w=1+rng()%std::min(W-x,150),h=1+rng()%std::min(H-y,60);
  check(x,y,w,h,rng()%9);
 }
 // Real incremental stripes must preserve every unchanged pixel across frames.
 std::vector<uint16_t> logical(W*H), physical(W*H), oracle(W*H);
 for(int frame=0;frame<300;++frame) {
  int x=rng()%(W-64),y=rng()%(H-32),w=64,h=32;
  std::vector<uint16_t> stripe(w*h);
  for(int yy=0;yy<h;++yy) for(int xx=0;xx<w;++xx) {
   uint16_t value=static_cast<uint16_t>(rng());
   stripe[yy*w+xx]=value; logical[(y+yy)*W+x+xx]=value;
  }
  display_rotate_copy_region(stripe.data(),physical.data(),x,y,x+w-1,y+h-1,
                             w,W,H,ESP_LV_ADAPTER_ROTATE_180,2,16,32);
  std::reverse_copy(logical.begin(),logical.end(),oracle.begin());
  assert(physical==oracle); ++cases;
 }
 esp_lv_adapter_display_pipeline_t pipeline;
 ready=true;
 assert(display_bridge_pipeline_wait_free_buf(&pipeline)==&buffer);
 ready=false; release_on_wait=true;
 assert(display_bridge_pipeline_wait_free_buf(&pipeline)==&buffer);
 ready=false; release_on_wait=false; wait_ticks=0;
 bool caught=false;
 try { display_bridge_pipeline_wait_free_buf(&pipeline); }
 catch(const std::runtime_error&) { caught=true; }
 assert(caught);
 printf("{\"rotation_cases\":%u,\"healthy_wait_cases\":2,\"missing_completion_wait_ticks\":%u,"
        "\"unbounded_wait\":%s,\"full_frame_pixels\":384000,\"sample_partial_pixels\":2048}\n",
        cases,wait_ticks,wait_ticks==portMAX_DELAY?"true":"false");
}
"""

def run(args, cwd, log):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors="replace")
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compile-db", type=Path)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    options = parser.parse_args()
    vendor, output = options.source.resolve(), options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    common = vendor / "src/display/bridge/common/display_bridge_common.c"
    bridge = vendor / "src/display/bridge/v9/lvgl_bridge_v9.c"
    source = common.read_text(encoding="utf-8")
    cpp = output / "upstream_rotation_wait.cpp"
    cpp.write_text(PREFIX + function(source, "display_copy_pixel") +
                   function(source, "display_rotate_copy_region") +
                   function(source, "display_bridge_pipeline_wait_free_buf") + CASES, encoding="utf-8")
    exe = output / ("upstream_rotation_wait.exe" if os.name == "nt" else "upstream_rotation_wait")
    result = run([options.cxx, "-std=c++17", "-O2", "-Wall", "-Wextra", "-o", str(exe), str(cpp)], ROOT, output/"host-build.log")
    if result.returncode:
        raise RuntimeError((output/"host-build.log").read_text())
    result = run([str(exe)], ROOT, output/"host-results.log")
    if result.returncode:
        raise RuntimeError((output/"host-results.log").read_text())
    report = {
        "upstream_version": "0.7.2",
        "upstream_commit": "88152bff24d5fbc05ff2ffed3d5208c247711ded",
        "source_sha256": {str(p.relative_to(vendor)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in (common, bridge, vendor/"idf_component.yml")},
        "host": json.loads(result.stdout),
        "limits": ["No device or optical test", "No ESP32-S3 runtime timing",
                   "Standalone rotation tests do not validate full framebuffer ownership",
                   "Cross-compile is not link/integration validation"],
        "cross_compile": []
    }
    archive = vendor.parent.parent / "esp_lvgl_adapter-0.7.2.zip"
    if archive.is_file():
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != ARCHIVE_SHA256:
            raise ValueError("Unexpected upstream archive hash")
        report["archive_sha256"] = digest
    if options.compile_db:
        db = json.loads(options.compile_db.read_text())
        item = next(x for x in db if x["file"].replace("\\", "/").endswith("/src/ui/screens.c"))
        if os.name == "nt":
            split = ctypes.windll.shell32.CommandLineToArgvW
            split.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
            split.restype = ctypes.POINTER(ctypes.c_wchar_p)
            count = ctypes.c_int()
            argv = split(item["command"], ctypes.byref(count))
            if not argv:
                raise ctypes.WinError()
            try:
                args = [argv[i] for i in range(count.value)]
            finally:
                ctypes.windll.kernel32.LocalFree(ctypes.cast(argv, ctypes.c_void_p))
        else:
            args = shlex.split(item["command"])
        # Preserve all compiler/target/config options; only source and output change.
        for flag in ("-o", "-c"):
            index = args.index(flag)
            del args[index:index+2]
        args = [a for a in args if not a.startswith("-fdiagnostics-color=")]
        args += ["-fdiagnostics-color=never"]
        for relative in ("include","src/adapter","src/display","src/display/ports",
                         "src/display/bridge","src/display/bridge/common","src/display/bridge/v9","src/input"):
            args += ["-I" + str(vendor/relative)]
        sdk = ROOT.parent/"tools/idf61-sdk/esp-idf"
        for relative in ("components/esp_lcd/priv_include", "components/esp_mm/include",
                         "components/esp_psram/include", "components/esp_pm/include",
                         "components/efuse/include", "components/efuse/esp32s3/include"):
            args += ["-I" + str(sdk/relative)]
        args += ["-I" + str(ROOT/"managed_components/espressif__esp_lcd_touch/include")]

        units = ("src/adapter/esp_lv_adapter.c", "src/display/display_manager.c",
                 "src/display/display_te_sync.c", "src/display/bridge/common/display_bridge_common.c",
                 "src/display/bridge/v9/lvgl_bridge_v9.c", "src/display/bridge/v9/lvgl_ppa_accel_v9.c",
                 "src/input/esp_lv_adapter_input_touch.c")
        for relative in units:
            path = vendor/relative
            log = output/(path.stem+".log")
            command = args + ["-o",str(output/(path.stem+".o")),"-c",str(path)]
            result = run(command, item["directory"], log)
            report["cross_compile"].append({"source":relative,"exit_code":result.returncode,"log":str(log),
                                             "source_sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
            print(("PASS " if result.returncode==0 else "FAIL ") + relative, flush=True)
    (output/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report["host"]))
    print("Evidence: " + str(output/"report.json"))
    if any(unit["exit_code"] != 0 for unit in report["cross_compile"]):
        raise SystemExit(1)

if __name__ == "__main__":
    main()
