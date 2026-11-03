"""Build the real-manager stand. Usage: build.py <fw_root> <out_dir>."""
import os, pathlib, shutil, subprocess, sys
here = pathlib.Path(__file__).resolve().parents[1] / "test/support/network_manager"
fw = pathlib.Path(sys.argv[1]).resolve()
out = pathlib.Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
bin_dir = pathlib.Path('C:/msys64/mingw64/bin')
env = dict(os.environ); env['PATH'] = str(bin_dir) + os.pathsep + env['PATH']
ard = out / 'ard'; ard.mkdir(exist_ok=True)
acore = fw / 'managed_components/espressif__arduino-esp32/cores/esp32'
for n in ['WString.cpp', 'WString.h', 'stdlib_noniso.c', 'stdlib_noniso.h', 'pgmspace.h']:
    shutil.copy2(acore / n, ard / n)
lab = fw / 'test/support/http_server/shim'
inc = [here / 'shim', lab, fw / 'src', ard]
flags = ['-std=gnu++17', '-O1', '-DUNIT_TEST', '-include', str(lab / 'lab_prefix.h')]
for i in inc:
    flags += ['-I', str(i)]
srcs = [ard / 'WString.cpp', fw / 'src/modules/NetworkManager.cpp', fw / 'src/core/WifiScanSession.cpp', fw / 'src/core/WakePowerGuard.cpp',
        fw / 'src/core/NetworkHostname.cpp', fw / 'src/web/WebWifiScanUtils.cpp', fw / 'src/web/WebTextUtils.cpp', fw / 'src/web/WebDeferredActionsState.cpp', here / 'mgr_main.cpp']
objs = []
for s in srcs:
    o = out / (s.stem + '.o')
    objs.append(o)
    r = subprocess.run([str(bin_dir / 'g++.exe'), *flags, '-c', str(s), '-o', str(o)], env=env, capture_output=True, text=True, cwd=out, timeout=120)
    if r.returncode:
        print('COMPILE FAIL', s, r.stderr[-3000:]); sys.exit(2)
r = subprocess.run([str(bin_dir / 'g++.exe'), *map(str, objs), '-lws2_32', '-lwinmm', '-pthread', '-o', str(out / 'mgr.exe')], env=env, capture_output=True, text=True, cwd=out, timeout=120)
if r.returncode:
    print('LINK FAIL', r.stderr[-3000:]); sys.exit(2)
print('built', out / 'mgr.exe')

result = subprocess.run([str(out / 'mgr.exe'), *sys.argv[3:]], env=env, cwd=out, capture_output=True, text=True, timeout=180)
(out / 'result.log').write_text(result.stdout + result.stderr, encoding='utf-8')
print(result.stdout)
raise SystemExit(result.returncode)
