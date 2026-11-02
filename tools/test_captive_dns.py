"""Compile the production DNS socket lifecycle with deterministic I/O failures.
This is a host shim test, not real UDP/lwIP/radio; hardware DNS is tested separately.
"""
import argparse, os, pathlib, subprocess
from datetime import datetime, timezone
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output-dir',type=pathlib.Path)
a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1]
out=a.output_dir or r/'build/captive-dns-tests'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
out.mkdir(parents=True,exist_ok=False)
lab=r/'test/support/captive_dns';compiler=pathlib.Path('C:/msys64/mingw64/bin')
env=dict(os.environ);env['PATH']=str(compiler)+os.pathsep+env.get('PATH','')
cmd=[str(compiler/'g++.exe'),'-std=c++17','-O1','-Wall','-Wextra','-I',str(lab/'shim'),'-I',str(r/'src'),
     str(r/'src/core/CaptiveDnsServer.cpp'),str(r/'src/core/CaptiveDnsReply.cpp'),str(lab/'main.cpp'),'-o',str(out/'test.exe')]
q=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
(out/'build.log').write_text(q.stdout+q.stderr,encoding='utf-8')
if q.returncode:print(q.stderr);raise SystemExit(q.returncode)
q=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,env=env,timeout=30)
(out/'result.log').write_text(q.stdout+q.stderr,encoding='utf-8');print(q.stdout)
raise SystemExit(q.returncode)
