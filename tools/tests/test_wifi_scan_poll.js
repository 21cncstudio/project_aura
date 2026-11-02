'use strict';
// Exercise the actual embedded page, with deterministic time and abortable
// network boundaries. No copied implementation of the production poll loop.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const template = fs.readFileSync(path.join(__dirname, '../../src/web/WebTemplates.h'), 'utf8');
const start = template.indexOf('        function watchScanProgress()');
const end = template.indexOf("        document.addEventListener('DOMContentLoaded'", start);
assert(start >= 0 && end > start);
const source = template.slice(start, end) + '\nwatchScanProgress();';

async function scenario(kind) {
    let now = 0, nextId = 0, calls = 0, active = 0, maximumActive = 0, aborted = 0;
    const timers = new Map();
    const redirects = [];
    const flush = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };
    const hang = signal => new Promise((resolve, reject) => {
        signal.addEventListener('abort', () => {
            aborted++;
            reject(Object.assign(new Error('aborted'), { name: 'AbortError' }));
        }, { once: true });
    });
    const context = {
        AbortController, Date,
        setTimeout(fn, ms) { const id = ++nextId; timers.set(id, { fn, at: now + ms }); return id; },
        clearTimeout(id) { timers.delete(id); },
        window: { location: { replace(url) { redirects.push(url); } } },
        async fetch(url, options) {
            assert.match(url, /^\/wifi\?scan_status=1&ts=/);
            assert.equal(options.cache, 'no-store');
            assert(options.signal instanceof AbortSignal);
            calls++; active++; maximumActive = Math.max(maximumActive, active);
            const first = calls === 1;
            if (first && kind === 'headers-hang') {
                try { return await hang(options.signal); } finally { active--; }
            }
            if (first && kind === 'network-error') { active--; throw new Error('network'); }
            return {
                ok: !(first && kind === 'http-error'), status: 503,
                async json() {
                    try {
                        if (first && kind === 'body-hang') return await hang(options.signal);
                        if (first && kind === 'bad-json') throw new Error('JSON');
                        if (first && kind === 'invalid-payload') return { success: true };
                        return { success: true, scan_in_progress: first && kind === 'scanning' };
                    } finally { active--; }
                }
            };
        }
    };
    // An HTTP error has no JSON consumption, but its request has settled.
    if (kind === 'http-error') {
        const fetch = context.fetch;
        context.fetch = async (...args) => { const response = await fetch(...args); if (!response.ok) active--; return response; };
    }
    vm.runInNewContext(source, context);
    async function advance(ms) {
        const until = now + ms;
        while (true) {
            const entry = [...timers.entries()].filter(([,t]) => t.at <= until).sort((a,b) => a[1].at-b[1].at)[0];
            if (!entry) break;
            const [id, timer] = entry; now = timer.at; timers.delete(id); timer.fn(); await flush();
        }
        now = until; await flush();
    }
    await advance(1999); assert.equal(calls, 0);
    await advance(1); assert.equal(calls, 1);
    if (kind.endsWith('-hang')) {
        await advance(14999); assert.equal(aborted, 0); assert.equal(calls, 1);
        await advance(1); assert.equal(aborted, 1); assert.equal(active, 0);
        await advance(7999); assert.equal(calls, 1);
        await advance(1);
    } else if (kind !== 'complete') {
        const retry = kind === 'scanning' ? 5000 : 8000;
        await advance(retry-1); assert.equal(calls, 1);
        await advance(1);
    }
    assert.equal(calls, kind === 'complete' ? 1 : 2);
    assert.deepEqual(redirects, ['/wifi']);
    assert.equal(maximumActive, 1, 'poll attempts must not overlap');
    assert.equal(timers.size, 0, 'completion clears deadline and retries');
    await advance(60000); assert.equal(calls, kind === 'complete' ? 1 : 2);
    console.log('[PASS]', kind);
}
(async () => {
    for (const kind of ['headers-hang','body-hang','network-error','http-error','bad-json','invalid-payload','scanning','complete']) {
        await scenario(kind);
    }
    console.log('8 scan polling scenarios passed');
})().catch(err => { console.error(err); process.exitCode = 1; });
