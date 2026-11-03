'use strict';
// Test the actual captive-page code without fetch, Promise, AbortController
// or NodeList.forEach in the browser context.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { spawnSync } = require('node:child_process');
const template = fs.readFileSync(path.join(__dirname, '../../src/web/WebTemplates.h'), 'utf8');
const page = template.slice(template.indexOf('static const char kWifiPage'), template.indexOf('static const char kWifiSavePage'));
const pageScript = page.match(/<script>([\s\S]*?)<\/script>/)[1].replace('{{SCAN_IN_PROGRESS}}', 'false');
// Node bundles Acorn; no additional firmware or host package is installed.
const syntax = spawnSync(process.execPath, ['--expose-internals', '-e',
    "require('internal/deps/acorn/acorn/dist/acorn').parse(require('fs').readFileSync(0,'utf8'),{ecmaVersion:5})"],
    { input: pageScript, encoding: 'utf8' });
assert.equal(syntax.status, 0, 'entire Wi-Fi page must parse as ES5: ' + syntax.stderr);
console.log('[PASS] entire captive page parses as ES5');
const start = pageScript.indexOf('        function watchScanProgress()');
const end = pageScript.indexOf("        document.addEventListener('DOMContentLoaded'", start);
assert(start >= 0 && end > start);
const source = pageScript.slice(start, end) + '\nwatchScanProgress();';

function scenario(kind) {
    let now = 0, nextId = 0, calls = 0, active = 0, maximumActive = 0, aborted = 0;
    const timers = new Map(), redirects = [];
    function schedule(fn, ms) { const id = ++nextId; timers.set(id, { fn, at: now + ms }); return id; }
    class XHR {
        constructor() { this.readyState = 0; this.status = 0; this.responseText = ''; }
        open(method, url, async) {
            assert.equal(method, 'GET'); assert.match(url, /^\/wifi\?scan_status=1&ts=/); assert.equal(async, true);
            if (kind === 'open-error' && calls === 0) { calls++; throw new Error('open'); }
            this.readyState = 1;
        }
        send(body) {
            assert.equal(body, null);
            calls++;
            if (kind === 'send-error' && calls === 1) throw new Error('send');
            this.pending = true; active++; maximumActive = Math.max(maximumActive, active);
            const first = calls === 1;
            if (first && kind === 'headers-hang') return;
            this.task = schedule(() => {
                if (!this.pending) return;
                const ready = this.onreadystatechange, error = this.onerror;
                this.status = first && kind === 'http-error' ? 503 : 200;
                this.readyState = 3; this.responseText = '{"success":true,';
                if (ready) ready(); // Partial JSON must never be consumed.
                if (first && kind === 'body-hang') return;
                this.pending = false; active--;
                if (first && kind === 'network-error') { this.status = 0; if (error) error(); }
                if (first && kind === 'external-abort') { if (this.onabort) this.onabort(); }
                this.responseText = first && kind === 'bad-json' ? '{' : JSON.stringify(
                    first && kind === 'invalid-payload' ? { success: true } :
                    first && kind === 'null-payload' ? null :
                    first && kind === 'failure-payload' ? { success: false, scan_in_progress: false } :
                    first && kind === 'wrong-type' ? { success: true, scan_in_progress: 0 } :
                    { success: true, scan_in_progress: first && kind === 'scanning' });
                this.readyState = 4;
                if (this.onreadystatechange) this.onreadystatechange();
                // Queued duplicate callbacks must not redirect/schedule twice.
                if (first && kind === 'late-events') { if (ready) ready(); if (error) error(); }
            }, 0);
        }
        abort() {
            if (this.pending) { this.pending = false; active--; aborted++; }
            timers.delete(this.task);
            // Require callbacks to be detached BEFORE abort().
            this.readyState = 4; this.status = 0;
            if (this.onreadystatechange) this.onreadystatechange();
            if (this.onabort) this.onabort();
            this.readyState = 0;
        }
    }
    vm.runInNewContext(source, {
        XMLHttpRequest: XHR, Promise: undefined, fetch: undefined, AbortController: undefined,
        Date: { now: () => now }, setTimeout: schedule, clearTimeout: id => timers.delete(id),
        window: { location: { replace: url => redirects.push(url) } }
    });
    function advance(ms) {
        const until = now + ms;
        for (let guard = 0; guard < 100; guard++) {
            const entry = [...timers.entries()].filter(([,t]) => t.at <= until).sort((a,b) => a[1].at-b[1].at)[0];
            if (!entry) { now = until; return; }
            const [id, timer] = entry; now = timer.at; timers.delete(id); timer.fn();
        }
        assert.fail('unbounded timer loop');
    }
    advance(1999); assert.equal(calls, 0);
    advance(1); assert.equal(calls, 1);
    const done = kind === 'complete' || kind === 'late-events';
    if (kind.endsWith('-hang')) {
        advance(14999); assert.equal(aborted, 0); assert.equal(calls, 1); assert.equal(redirects.length, 0);
        advance(1); assert.equal(aborted, 1); assert.equal(active, 0);
        advance(7999); assert.equal(calls, 1);
        advance(1);
    } else if (!done) {
        const retry = kind === 'scanning' ? 5000 : 8000;
        advance(retry-1); assert.equal(calls, 1);
        advance(1);
    }
    assert.equal(calls, done ? 1 : 2);
    assert.deepEqual(redirects, ['/wifi']);
    assert.equal(maximumActive, 1, 'poll attempts must not overlap');
    assert.equal(active, 0);
    assert.equal(timers.size, 0, 'completion clears deadline and retries');
    advance(60000); assert.equal(calls, done ? 1 : 2);
    console.log('[PASS]', kind);
}

function legacySelection() {
    const elements = new Map();
    function element(id) {
        if (!elements.has(id)) elements.set(id, {
            value: '', disabled: true, classes: new Set(), events: {},
            classList: {
                add(name) { element(id).classes.add(name); },
                remove(name) { element(id).classes.delete(name); },
                toggle(name, on) { if (on) this.add(name); else this.remove(name); }
            },
            addEventListener(name, fn) { this.events[name] = fn; },
            getAttribute(name) { return name === 'data-ssid' ? id : '0'; },
            focus() {}, scrollIntoView() {}
        });
        return elements.get(id);
    }
    const first = element('First'), second = element('Second');
    const list = { 0: first, 1: second, length: 2 }; // Old NodeList, no forEach.
    let ready;
    vm.runInNewContext(pageScript, {
        fetch: undefined, Promise: undefined, AbortController: undefined,
        document: {
            getElementById: element, querySelectorAll: () => list,
            addEventListener(name, fn) { assert.equal(name, 'DOMContentLoaded'); ready = fn; }
        },
        setTimeout(fn) { fn(); }
    });
    ready(); first.events.click.call(first); second.events.click.call(second);
    assert.equal(element('selected-ssid').value, 'Second');
    assert.equal(element('submit-btn').disabled, false);
    assert(!first.classes.has('selected')); assert(second.classes.has('selected'));
    console.log('[PASS] network selection with old NodeList and no modern network APIs');
}

for (const kind of ['headers-hang','body-hang','network-error','http-error','bad-json','invalid-payload',
    'null-payload','failure-payload','wrong-type','scanning','complete','open-error','send-error','external-abort','late-events']) scenario(kind);
legacySelection();
console.log('15 scan polling scenarios + ES5 syntax + legacy network selection passed');
