"""Verify AP/HTTP ownership and accepted-body regressions with isolated edits.

Windows/MinGW only, like test_http_server_sdk.py. Copies sources into a new
evidence directory; never edits production files. A build error, timeout, or
unrelated failure is NOT evidence that a mutation was detected.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


SCAN = 'src/core/WifiScanSession.cpp'
HTTP = 'src/web/WebTransportEspHttpServer.cpp'
MUTATIONS = [
    ('scan-stop-without-lease', 's29', SCAN,
     '    if (!worker_activity_) return true; // Not started yet, or already done.\n', '',
     'outside-admission service cannot call SDK stop without a live worker lease'),
    ('scan-release-before-cleanup', 's30', SCAN,
     '    const esp_err_t cleanup = esp_wifi_clear_ap_list();',
     '    worker_activity_ = {};\n    const esp_err_t cleanup = esp_wifi_clear_ap_list();',
     'wake cannot become ready until driver cleanup returns'),
    ('scan-stop-without-mutex', 's31', SCAN,
     '    const std::lock_guard<std::mutex> lock(worker_mutex_);\n    if (!worker_activity_)',
     '    if (!worker_activity_)',
     'stop mutex prevents cleanup and lease release while SDK stop is still running'),
    ('http-no-reacquire', 's32', HTTP,
     '                bool unused = false;\n                activity = wait_for_web_activity_admission(unused);\n                progress_at += static_cast<uint32_t>(millis() - paused_at);',
     '                while (WakePowerGuard::backgroundPaused(millis())) delay(1);\n                progress_at += static_cast<uint32_t>(millis() - paused_at);',
     'drain reacquires its activity lease between the two wakes'),
]

BODY_MUTATIONS = [
    ('body-no-timeout-restore', 's38', HTTP,
     '    timeout.tv_sec = kHttpServerRecvWaitTimeoutS;\n    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {',
     '    timeout.tv_usec = kBodyRecvSliceMs * 1000;\n    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {',
     'keepalive header parsing restores the normal receive timeout'),
    ('body-no-wake-yield', 's38', HTTP,
     '        while (remaining > 0) {\n            if (WakePowerGuard::backgroundPaused(millis())) {',
     '        while (remaining > 0) {\n            if (false) {',
     'accepted body yields to the wake owner promptly'),
    ('body-no-progress-deadline', 's39', HTTP,
     '            if (total_timeout || static_cast<uint32_t>(now - progress_at) >= kBodyReadProgressTimeoutMs) {',
     '            if (total_timeout) {',
     'lack of useful body progress produces a bounded complete 408'),
    ('body-byte-trickle-resets-progress', 's39', HTTP,
     '            if (progress_bytes >= kBodyReadProgressBytes) {',
     '            if (progress_bytes > 0) {',
     'lack of useful body progress produces a bounded complete 408'),
    ('body-no-total-deadline', 's40', HTTP,
     '            const bool total_timeout = static_cast<uint32_t>(now - started) >= kBodyReadTimeoutMs;',
     '            const bool total_timeout = false;',
     'steady progress still stops at the aggregate body deadline'),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf-path', type=Path, required=True)
    parser.add_argument('--compiler-dir', type=Path, default=Path('C:/msys64/mingw64/bin'))
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--group', choices=['guard', 'body', 'all'], default='guard')
    args = parser.parse_args()
    mutations = ((MUTATIONS if args.group != 'body' else []) +
                 (BODY_MUTATIONS if args.group != 'guard' else []))
    root = Path(__file__).resolve().parents[1]
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out = (args.output_dir or root / 'build/http-guard-mutations' / stamp).resolve()
    # Never reuse old results or remove the user's evidence.
    out.mkdir(parents=True, exist_ok=False)
    base = out / 'baseline'
    inputs = list((root / 'src').rglob('*'))
    inputs += list((root / 'include').rglob('*'))
    inputs += list((root / 'test/support/http_server').rglob('*'))
    inputs += list((root / 'test/support/web_handler_wiring').rglob('*'))
    inputs += [root / 'test/mocks/Arduino.h']
    inputs += [root / 'tools/test_http_server_sdk.py', root / 'tools/test_web_handler_wiring.py']
    ard = root / 'managed_components/espressif__arduino-esp32/cores/esp32'
    inputs += [ard / n for n in ['WString.cpp', 'WString.h', 'stdlib_noniso.c', 'stdlib_noniso.h', 'pgmspace.h']]
    original = []
    for p in inputs:
        if not p.is_file():
            continue
        rel = p.relative_to(root)
        dest = base / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, dest)
        digest = sha(p)
        if sha(dest) != digest:
            raise RuntimeError(f'Source copy mismatch: {rel}')
        original.append({'file': rel.as_posix(), 'sha256': digest})
    (out / 'source-manifest.json').write_text(json.dumps(original, indent=2), encoding='utf-8')
    env = dict(os.environ)
    env['PATH'] = str(args.compiler_dir.resolve()) + os.pathsep + env.get('PATH', '')
    results = []

    def execute(name, work, cases, expected_failure=None):
        evidence = out / name
        command = [sys.executable, '-X', 'utf8', str(work / 'tools/test_http_server_sdk.py'),
                   '--idf-path', str(args.idf_path.resolve()), '--compiler-dir', str(args.compiler_dir.resolve()),
                   '--output-dir', str(evidence)]
        for case in cases:
            command += ['--case', case]
        timed_out = False
        try:
            proc = subprocess.run(command, cwd=work, env=env, capture_output=True,
                                  text=True, encoding='utf-8', timeout=240)
            code, output = proc.returncode, proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as exc:
            # The child runner has a shorter per-process timeout (120 s).
            # Preserve failure evidence rather than reporting a false catch.
            code, timed_out = None, True
            output = str(exc)
        evidence.mkdir(exist_ok=True)
        (evidence / 'runner.log').write_text(output, encoding='utf-8')
        result_file = evidence / 'result.json'
        report = json.loads(result_file.read_text(encoding='utf-8')) if result_file.exists() else {}
        actual = report.get('cases', [])
        correct_cases = [x['case'] for x in actual] == cases
        if expected_failure is None:
            passed = code == 0 and correct_cases and all(x['passed'] > 0 and x['failed'] == 0 and x['exit_code'] == 0 for x in actual)
        else:
            failures = [line.strip()[7:] for line in output.splitlines() if line.strip().startswith('[FAIL] ')]
            passed = (code == 1 and correct_cases and len(actual) == 1 and
                      actual[0]['exit_code'] == 1 and actual[0]['failed'] > 0 and
                      len(failures) == actual[0]['failed'] and
                      all(failure == expected_failure for failure in failures))
        results.append({'name': name, 'cases': actual, 'runner_exit': code, 'timeout': timed_out,
                        'expected_failure': expected_failure, 'verified': passed, 'command': command})
        (out / 'result.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(name + ': ' + ('PASS' if passed else 'FAIL'), flush=True)
        return passed

    if not execute('control', base, list(dict.fromkeys(m[1] for m in mutations))):
        return 1
    for name, case, relative, before, after, message in mutations:
        work = out / 'mutants' / name
        shutil.copytree(base, work)
        source = work / relative
        text = source.read_text(encoding='utf-8')
        if text.count(before) != 1:
            raise RuntimeError(f'{name}: mutation no longer matches one source location; review it')
        source.write_text(text.replace(before, after, 1), encoding='utf-8')
        (work / 'mutation.json').write_text(json.dumps({'file': relative, 'before': before, 'after': after}, indent=2), encoding='utf-8')
        execute(name, work, [case], message)
    unchanged = all(sha(root / x['file']) == x['sha256'] for x in original)
    (out / 'source-unchanged.json').write_text(json.dumps({'unchanged': unchanged}), encoding='utf-8')
    print('Evidence:', out)
    return int(not unchanged or not all(x['verified'] for x in results))


if __name__ == '__main__':
    raise SystemExit(main())
