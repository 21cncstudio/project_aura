"""Integration regressions using production HTTP handlers and the complete selected
ESP-IDF server/session loop. Windows/MinGW loopback only; not ESP32/lwIP/radio.
Adapted from the independent AP/HTTP review lab supplied on 2026-11-01.
Arduino String runs with an allocator failure hook; OS/Wi-Fi/RTOS use host shims.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf-path', type=Path, required=True)
    parser.add_argument('--compiler-dir', type=Path, default=Path('C:/msys64/mingw64/bin'))
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--case', action='append', dest='cases', choices=['s'+str(i) for i in range(1,33)])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    lab = root / 'test/support/http_server'
    sdk = args.idf_path.resolve() / 'components'
    hs = sdk / 'esp_http_server'
    ard = root / 'managed_components/espressif__arduino-esp32/cores/esp32'
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out = (args.output_dir or root / 'build/http-server-sdk-tests' / stamp).resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'ard').mkdir(exist_ok=True)
    env = dict(os.environ)
    env['PATH'] = str(args.compiler_dir) + os.pathsep + env.get('PATH', '')
    sources = []
    objects = []
    commands = []

    def run(command):
        commands.append([str(x) for x in command])
        result = subprocess.run(commands[-1], cwd=out, env=env, capture_output=True, text=True, timeout=120)
        with (out / 'build.log').open('a', encoding='utf-8') as log:
            log.write(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def compile_source(source, includes, cxx=False):
        sources.append(source)
        obj = out / (source.stem + '.o')
        objects.append(obj)
        compiler = args.compiler_dir / ('g++.exe' if cxx else 'gcc.exe')
        flags = ['-std=gnu++17' if cxx else '-std=gnu11', '-O1', '-include', lab / 'shim/lab_prefix.h']
        flags += ['-DUNIT_TEST'] if cxx else ['-DLAB_C']
        for inc in includes:
            flags += ['-I', inc]
        run([compiler, *flags, '-c', source, '-o', obj])
        return obj

    c_inc = [lab / 'shim', hs / 'include', hs / 'src', hs / 'src/util', hs / 'src/port/esp32', sdk / 'http_parser']
    for name in ['httpd_main', 'httpd_parse', 'httpd_sess', 'httpd_txrx', 'httpd_uri']:
        compile_source(hs / 'src' / (name + '.c'), c_inc)
    compile_source(hs / 'src/util/ctrl_sock.c', c_inc)
    compile_source(sdk / 'http_parser/http_parser.c', c_inc)
    for name in ['WString.cpp', 'WString.h', 'stdlib_noniso.c', 'stdlib_noniso.h', 'pgmspace.h']:
        shutil.copy2(ard / name, out / 'ard' / name)
    cpp_inc = [lab / 'shim', lab, root / 'src', out / 'ard', hs / 'include', sdk / 'http_parser']
    obj = compile_source(out / 'ard/WString.cpp', cpp_inc, True)
    run([args.compiler_dir / 'objcopy.exe', '--redefine-sym', 'realloc=lab_realloc', obj])

    for name in ['web/WebTransportEspHttpServer', 'web/WebStreamWriter', 'web/WebStreamPolicy', 'web/WebStreamState',
                 'web/WebPortalHandlers', 'web/WebWifiPage', 'web/WebDashboardPage', 'web/WebWifiScanUtils', 'web/WebTextUtils',
                 'web/WebQueryString', 'web/WebMultipart', 'web/WebTemplatesDashboardAp', 'web/WebTemplatesDac',
                 'web/WebTemplatesTheme', 'core/WakePowerGuard', 'core/WifiScanSession', 'core/WifiPowerSaveGuard', 'web/WebResponseUtils']:
        compile_source(root / 'src' / (name + '.cpp'), cpp_inc, True)
    for name in ['lab_os.cpp', 'lab_main.cpp']:
        compile_source(lab / name, cpp_inc, True)
    exe = out / 'probe.exe'
    run([args.compiler_dir / 'g++.exe', *objects, '-lws2_32', '-lwinmm', '-pthread', '-o', exe])
    cases = []
    for case in args.cases or ['s' + str(i) for i in range(1, 33)]:
        result = subprocess.run([str(exe), case], cwd=out, env=env, capture_output=True, text=True, timeout=120)
        (out / (case + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        print(result.stdout, end='')
        cases.append({'case': case, 'exit_code': result.returncode, 'passed': result.stdout.count('[PASS]'), 'failed': result.stdout.count('[FAIL]')})
    sources += list((lab / 'shim').rglob('*.h')) + list((root / 'src/web').glob('*.h')) + [root / 'src/core' / name for name in ['WifiScanSession.h', 'WifiScanTransition.h', 'WifiPowerSaveGuard.h', 'WakePowerGuard.h']] + list((hs / 'include').rglob('*.h')) + [root / 'src/web/WebRouteRegistry.cpp', lab / 'lab.h', Path(__file__).resolve()]
    report = {'platform': 'Windows loopback, real SDK session lifecycle, OS and Wi-Fi shims', 'cases': cases,
              'commands': commands, 'sources': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources]}
    (out / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('Evidence:', out)
    return int(any(c['exit_code'] for c in cases))


if __name__ == '__main__':
    raise SystemExit(main())
