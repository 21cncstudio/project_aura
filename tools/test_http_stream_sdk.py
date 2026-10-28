"""Host loopback tests of production capture + the selected SDK's chunk implementation.
No physical addresses or devices. Requires Windows MinGW; SDK code is extracted
verbatim into ignored build output, compiled as C, and its SHA256 is recorded.
This covers HTTP framing/send boundaries, not lwIP/Wi-Fi scheduling on ESP32.
"""
import argparse
import hashlib
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess


def extract_function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf-path", type=Path, required=True)
    parser.add_argument("--compiler-dir", type=Path, default=Path("C:/msys64/mingw64/bin"))
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    out = (args.output_dir or root / "build/http-stream-sdk-probe" / stamp).resolve()
    out.mkdir(parents=True, exist_ok=True)
    sdk_file = args.idf_path / "components/esp_http_server/src/httpd_txrx.c"
    source = sdk_file.read_text(encoding="utf-8")
    functions = [extract_function(source, signature) for signature in
                 ("static esp_err_t httpd_send_all(", "esp_err_t httpd_resp_send_chunk(")]
    sdk_header = (args.idf_path / "components/esp_http_server/include/esp_http_server.h").read_text()
    error_defines = "\n".join(line for line in sdk_header.splitlines()
                              if re.match(r"#define ESP_ERR_HTTPD_", line))
    shim = r"""
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define HTTPD_RESP_USE_STRLEN -1
#define HTTP_SERVER_EVENT_SENT_DATA 6
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define LOG_FMT(x) x
typedef int esp_err_t;
struct sock_db { void *handle; int fd; int (*send_fn)(void *, int, const char *, size_t, int); };
struct httpd_header { const char *field; const char *value; };
struct httpd_req_aux { struct sock_db *sd; unsigned req_hdrs_count; bool first_chunk_sent;
    size_t max_req_hdr_len; const char *status; const char *content_type;
    unsigned resp_hdrs_count; struct httpd_header *resp_hdrs; };
struct httpd_data { int http_server_state; };
typedef struct { int fd; int data_len; } esp_http_server_event_data;
typedef struct { void *handle; void *aux; } httpd_req_t;
static inline bool httpd_valid_req(httpd_req_t *req) { return req && req->aux; }
static inline void esp_http_server_dispatch_event(int event, void *data, size_t size) {}
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len);
#ifdef __cplusplus
}
#endif
"""
    (out / "sdk_shim.h").write_text(shim + "\n" + error_defines)
    (out / "sdk_chunk.c").write_text('#include "sdk_shim.h"\n' + "\n\n".join(functions))
    child_env = dict(os.environ)
    child_env["PATH"] = str(args.compiler_dir) + os.pathsep + child_env.get("PATH", "")
    commands = [
        [str(args.compiler_dir / "gcc.exe"), "-std=c11", "-c", "sdk_chunk.c", "-o", "sdk_chunk.o"],
        [str(args.compiler_dir / "g++.exe"), "-std=c++17", "-O2", "-I", str(root / "src"), "-I", str(out),
         str(root / "test/support/http_stream_sdk_probe.cpp"), "sdk_chunk.o", "-lws2_32", "-o", "probe.exe"],
    ]
    for command in commands:
        subprocess.run(command, cwd=out, env=child_env, check=True, timeout=60)
    result = subprocess.run([str(out / "probe.exe")], cwd=out, env=child_env,
                            check=False, text=True, capture_output=True, timeout=40)
    (out / "probe-output.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    assert len(rows) == 5
    report = {"sdk_source": str(sdk_file), "sdk_sha256": hashlib.sha256(sdk_file.read_bytes()).hexdigest(),
              "extracted_functions_sha256": hashlib.sha256("\n\n".join(functions).encode()).hexdigest(),
              "platform": "Windows loopback; Winsock errors explicitly mapped to errno", "cases": rows}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
