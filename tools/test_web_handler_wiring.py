"""Check production HTTP entry points using boundary spies and real deferred state.

Windows/MinGW host test. No sockets, device access, broker, or UI integration.
The SDK server runner invokes this automatically; --source supports isolated mutations.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--compiler-dir', type=Path,
                        default=Path('C:/msys64/mingw64/bin'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = (args.output_dir or root / 'build/web-handler-wiring').resolve()
    out.mkdir(parents=True, exist_ok=True)
    lab = root / 'test/support/web_handler_wiring'
    env = dict(os.environ)
    env['PATH'] = str(args.compiler_dir) + os.pathsep + env.get('PATH', '')
    sources = [(args.source or root / 'src/web/WebHandlers.cpp').resolve(),
               root / 'src/web/WebDeferredActionsState.cpp', lab / 'main.cpp']
    # LTO discards unrelated entry points before PE unwind metadata retains them.
    # The tested entry points come directly from the unchanged production source.
    command = [str(args.compiler_dir / 'g++.exe'), '-std=gnu++17', '-O1', '-flto',
               '-ffunction-sections', '-fdata-sections', '-DUNIT_TEST']
    for include in [lab / 'shim', root / 'test/support/http_server/shim',
                    root / 'src', root / 'include']:
        command += ['-I', str(include)]
    executable = out / 'wiring.exe'
    command += [str(source) for source in sources]
    command += ['-Wl,--gc-sections', '-o', str(executable)]
    build = subprocess.run(command, cwd=root, env=env, capture_output=True,
                           text=True, timeout=60)
    (out / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    result = subprocess.run([str(executable)], env=env, capture_output=True,
                            text=True, timeout=10)
    (out / 'result.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    print(result.stdout, end='')
    inputs = sources + list((lab / 'shim').rglob('*.h')) + [Path(__file__).resolve()]
    report = {'platform': 'Windows host; real entry points, boundary spies',
              'exit_code': result.returncode, 'passed': result.stdout.count('[PASS]'),
              'failed': result.stdout.count('[FAIL]'), 'command': command,
              'sources': [{'path': str(source),
                           'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
                          for source in inputs]}
    (out / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
