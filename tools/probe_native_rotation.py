#!/usr/bin/env python3
"""Host-only differential experiment on actual Aura UI and managed LVGL 9.6.

Compiles the firmware renderer with actual EEZ UI. No device or dependency mutation. Outputs stay in a
dedicated ignored build directory. Compiles with local GCC; no downloads.
"""
from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
import time

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path)
    p.add_argument('--jobs',type=int,default=4)
    args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    fixture=root/'tools/tests/native_rotation'
    lvgl=root/'managed_components/lvgl__lvgl'
    out=(args.output or root/'build-idf-native-rotation-integration-20261008/host').resolve()
    if args.jobs<1 or args.jobs>8: p.error('jobs must be 1..8')
    if not (lvgl/'lvgl.h').is_file(): p.error('Resolve the existing managed LVGL dependency first')
    cc,cxx=shutil.which('gcc'),shutil.which('g++')
    if not cc or not cxx: p.error('Local GCC/G++ required')
    out.mkdir(parents=True,exist_ok=True)
    obj=out/'obj'; obj.mkdir(exist_ok=True)
    flags=['-O2','-g0','-DAURA_NATIVE_ROTATION_HOST_TEST','-DLV_CONF_INCLUDE_SIMPLE','-DLV_KCONFIG_IGNORE',
           '-I'+str(fixture),'-I'+str(lvgl),'-I'+str(lvgl/'include'),'-I'+str(root/'src')]
    sources=sorted((lvgl/'src').rglob('*.c'))+sorted((root/'src/ui').glob('ui_font_*.c'))
    sources += [f for f in sorted((root/'src/ui').glob('ui_image_*.c')) if f.name!='ui_image_project_aura_logo.c']
    sources += [root/'src/ui'/n for n in ['screens.c','styles.c','images.c']]
    sources += [root/'src/display/NativeRotation.cpp',fixture/'probe.cpp',fixture/'canvas_cleanup.cpp']
    headers=sorted(lvgl.rglob('*.h'))+sorted((root/'src/ui').glob('*.h'))
    headers += sorted((root/'src/display').glob('*.h'))
    headers += [fixture/'lv_conf.h',root/'tools/tests/lvgl9_ui/lv_conf.h',root/'include/lv_conf_idf9.h']
    versions={c:subprocess.check_output([c,'--version'],text=True).splitlines()[0] for c in [cc,cxx]}
    context=hashlib.sha256(json.dumps({'flags':flags,'compilers':versions,'headers':{str(h):digest(h) for h in headers}},sort_keys=True).encode()).hexdigest()
    def compile_one(src):
        key=hashlib.sha256((str(src)+digest(src)+context).encode()).hexdigest()
        target=obj/(key+'.o')
        if target.exists(): return target,None
        compiler=cxx if src.suffix=='.cpp' else cc
        extra=['-std=c++17'] if src.suffix=='.cpp' else ['-std=c11']
        r=subprocess.run([compiler,*flags,*extra,'-c',str(src),'-o',str(target)],capture_output=True,text=True,timeout=120)
        return target,(r.stdout+r.stderr) if r.returncode else None
    start=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results=list(pool.map(compile_one,sources))
    errors=[e for _,e in results if e is not None]
    (out/'compile.log').write_text('\n'.join(errors),encoding='utf-8')
    print(f'compile units={len(sources)} errors={len(errors)} elapsed={time.monotonic()-start:.1f}s',flush=True)
    if errors: print('\n'.join(errors[:3])); return 1
    rsp=out/'objects.rsp'; rsp.write_text('\n'.join('"'+s.as_posix()+'"' for src,(s,_) in zip(sources,results) if src.name!='canvas_cleanup.cpp'),encoding='utf-8')
    exe=out/'probe.exe'
    r=subprocess.run([cxx,'@'+str(rsp),'-Wl,--wrap=lv_draw_rotate','-Wl,--wrap=lv_draw_layer_alloc_buf','-Wl,--wrap=lv_display_sync_ready','-o',str(exe),'-lm'],capture_output=True,text=True,timeout=120)
    (out/'link.log').write_text(r.stdout+r.stderr,encoding='utf-8')
    if r.returncode: print(r.stdout+r.stderr); return r.returncode
    cleanup_rsp=out/'cleanup-objects.rsp'
    cleanup_rsp.write_text('\n'.join('"'+s.as_posix()+'"' for src,(s,_) in zip(sources,results) if src.name!='probe.cpp'),encoding='utf-8')
    cleanup_exe=out/'canvas_cleanup.exe'
    c=subprocess.run([cxx,'@'+str(cleanup_rsp),'-o',str(cleanup_exe),'-lm'],capture_output=True,text=True,timeout=120)
    if c.returncode: print(c.stdout+c.stderr); return c.returncode
    c=subprocess.run([str(cleanup_exe)],capture_output=True,text=True,timeout=30)
    (out/'stock-canvas-cleanup.log').write_text(c.stdout+c.stderr,encoding='utf-8')
    if c.returncode: print(c.stdout+c.stderr); return c.returncode
    print('Unmodified LVGL canvas cleanup control: reproduced separately (no rotation hook)',flush=True)
    reference=out/'reference.bin'
    commands=[('reference',0,'direct'),('native',0,'direct'),('native',180,'full'),('native',180,'direct'),('native',180,'padded'),('native',180,'bad-sync'),('native',180,'bad-draw')]
    runs=[]
    for mode,angle,render in commands:
        name=f'{mode}-{angle}-{render}'; dest=out/name
        before=time.monotonic()
        r=subprocess.run([str(exe),mode,str(angle),render,str(reference),str(dest)],capture_output=True,text=True,timeout=120)
        (out/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
        # Negative control must reach a pixel mismatch, not crash or fail to run.
        ok=(r.returncode==3 and 'MISMATCH' in r.stderr) if render=='bad-sync' else ((r.returncode==2 and 'Native draw targets active scanout' in r.stderr) if render=='bad-draw' else r.returncode==0)
        stats=json.loads((dest/'stats.json').read_text()) if r.returncode==0 else None
        runs.append({'name':name,'returncode':r.returncode,'expected_failure':render in ('bad-sync','bad-draw'),'passed':ok,'seconds':time.monotonic()-before,'stats':stats})
        print(f'{name}: {"PASS" if ok else "FAIL"} returncode={r.returncode} seconds={runs[-1]["seconds"]:.2f}',flush=True)
        if not ok:
            print((r.stdout+r.stderr)[-3000:]); break
    report={'scope':'exact firmware renderer on host; no hardware performance claim',
            'render_fixture_teardown_validated':False,'stock_canvas_cleanup_reproduced':True,
            'compilers':versions,'flags':flags,'context_sha256':context,
            'sources':{str(s.relative_to(root)):digest(s) for s in sources},
            'configurations':{str(h.relative_to(root)):digest(h) for h in headers if not h.is_relative_to(lvgl)},
            'executable_sha256':digest(exe),'runner_sha256':digest(Path(__file__)),'runs':runs,
            'passed':len(runs)==len(commands) and all(r['passed'] for r in runs)}
    (out/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return 0 if report['passed'] else 1

if __name__=='__main__': raise SystemExit(main())
