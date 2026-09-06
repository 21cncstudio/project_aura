import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { setImmediate as nextTurn } from 'node:timers/promises';
import vm from 'node:vm';

const header = readFileSync(new URL('../../src/web/WebTemplates.h', import.meta.url), 'utf8');
const template = header.match(/static const char kDiagPageTemplate\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/)?.[1];
assert.ok(template, 'Extract the real diagnostics page, not another embedded template');
const scripts = [...template.matchAll(/<script>\s*([\s\S]*?)<\/script>/g)];
assert.equal(scripts.length, 1, 'The diagnostics page has one executable script');
const script = scripts[0][1];

function deferred() {
  let resolve;
  let reject;
  const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
  return { promise, resolve, reject };
}

function diag(overrides = {}) {
  return {
    success: true,
    uptime_s: 1200,
    ota_busy: false,
    device: {
      firmware: '1.2.0-beta-test-7-dual-i2c',
      build_id: 'test-7-dual-i2c',
      hardware_profile: '7_dual_i2c',
      hardware_target: 'aura-aq-7-v1',
    },
    network: { mode: 'sta', wifi_enabled: true, wifi_ssid: 'Test network', hostname: 'aura-test', ip: '192.0.2.7', rssi: -42 },
    heap: { free: 100000, min_free: 90000 },
    boot: {
      reset_reason: 'SW', board_ready: true, lvgl_ready: true,
      i2c_status: 'sda_stuck_low', sda_high: false, scl_high: true,
      i2c_snapshot: { phase: 'before_board_init', live: false, port: 0, sda_gpio: 8, scl_gpio: 9 },
    },
    i2c_buses: {
      panel: { port: 0, sda_gpio: 8, scl_gpio: 9 },
      sensors: { port: 1, sda_gpio: 44, scl_gpio: 6, shared_with_panel: false },
    },
    last_errors: [{ ts_ms: 10, level: 'W', tag: 'GT911', message: 'saved diagnostic' }],
    web_stream: { ok_count: 3, last_abort_reason: 'none', last_uri: '/api/events' },
    unknown_future_field: { values: [null, 3, '<raw>&"value"'], nested: { untouched: true } },
    ...overrides,
  };
}

function events(overrides = {}) {
  return {
    success: true, uptime_s: 1201, count: 1,
    events: [{ ts_ms: 1199000, level: 'W', severity: 'warning', type: 'Sensors', message: 'CO2 elevated' }],
    another_future_field: ['preserve', null],
    ...overrides,
  };
}

const reply = (path, data, extra = {}) => ({ path, data, status: 200, ...extra });

function harness(steps) {
  const queue = [...steps];
  const requests = [];
  const timers = new Map();
  const blobs = new Map();
  const downloads = [];
  const revoked = [];
  const elements = new Map();
  const unexpected = [];
  let nextTimer = 0;
  let nextUrl = 0;
  for (const id of ['downloadReport', 'reportStatus', 'stamp', 'networkRows', 'systemRows', 'otaRows', 'webRows', 'bootI2cRows', 'errors']) {
    elements.set(id, {
      textContent: '', innerHTML: '', disabled: false, listeners: new Map(),
      addEventListener(type, listener) { this.listeners.set(type, listener); },
    });
  }
  const abortPromise = signal => new Promise((_, reject) => {
    const abort = () => { const error = new Error('aborted'); error.name = 'AbortError'; reject(error); };
    if (signal.aborted) abort();
    else signal.addEventListener('abort', abort, { once: true });
  });
  const context = {
    AbortController,
    Blob: class {
      constructor(parts, options) { this.text = parts.join(''); this.type = options.type; }
    },
    URL: {
      createObjectURL(blob) { const url = `blob:mock-${++nextUrl}`; blobs.set(url, blob); return url; },
      revokeObjectURL(url) { revoked.push(url); },
    },
    document: {
      getElementById: id => elements.get(id) || null,
      body: { appendChild(link) { link.attached = true; } },
      createElement(tag) {
        assert.equal(tag, 'a');
        return {
          href: '', download: '', attached: false, removed: false,
          click() {
            assert.equal(this.attached, true, 'Download anchor is attached before clicking');
            downloads.push({ filename: this.download, blob: blobs.get(this.href), link: this });
          },
          remove() { this.removed = true; },
        };
      },
    },
    setTimeout(callback, delay) { const id = ++nextTimer; timers.set(id, { callback, delay }); return id; },
    clearTimeout(id) { timers.delete(id); },
    async fetch(path, options) {
      requests.push({ path, options });
      assert.ok(options.method === undefined || options.method === 'GET', 'Only read-only API requests');
      assert.equal(options.body, undefined);
      assert.equal(options.cache, 'no-store');
      const step = queue.shift();
      if (!step || step.path !== path) {
        unexpected.push({ expected: step?.path, actual: path });
        throw new Error(`Unexpected API request ${path}`);
      }
      if (step.stallHeaders) return abortPromise(options.signal);
      return {
        ok: step.status >= 200 && step.status < 300,
        status: step.status,
        async json() {
          if (step.stallBody) return abortPromise(options.signal);
          if (step.bodyGate) return step.bodyGate.promise;
          if (step.jsonError) throw new SyntaxError('Malformed JSON');
          return structuredClone(step.data);
        },
      };
    },
  };
  vm.createContext(context);
  vm.runInContext(script, context, { filename: 'WebTemplates.h:kDiagPageTemplate' });
  return {
    context, elements, requests, timers, downloads, revoked, unexpected,
    settle: () => nextTurn(),
    click: () => elements.get('downloadReport').listeners.get('click')(),
    report(index = -1) { return JSON.parse(downloads.at(index).blob.text); },
    assertConsumed() { assert.deepEqual(unexpected, []); assert.equal(queue.length, 0); },
    async fireTimer(delay) {
      const entries = [...timers].filter(([, timer]) => timer.delay === delay);
      assert.equal(entries.length, 1, `Exactly one active ${delay}ms timer`);
      const [id, timer] = entries[0];
      timers.delete(id);
      await timer.callback();
      await nextTurn();
    },
  };
}

test('download keeps complete fresh API payloads and identity, then releases browser resources', async () => {
  const fresh = diag();
  const eventData = events();
  const h = harness([
    reply('/api/diag', diag({ uptime_s: 1, stale_poll_only: true })),
    reply('/api/diag', fresh), reply('/api/events', eventData),
  ]);
  await h.settle();
  await h.click();
  const report = h.report();
  assert.equal(report.schema, 'aura-diag-report');
  assert.equal(report.schema_version, 1);
  assert.equal(report.complete, true);
  assert.deepEqual(report.diag.data, fresh);
  assert.deepEqual(report.events.data, eventData);
  assert.deepEqual(report.diag.data.device, fresh.device);
  assert.equal(report.diag.data.stale_poll_only, undefined);
  assert.ok(Date.parse(report.started_at_browser) <= Date.parse(report.finished_at_browser));
  assert.match(report.notes.join(' '), /not.*atomic|sequential/i);
  assert.match(report.notes.join(' '), /not live/i);
  assert.equal(h.downloads.length, 1);
  assert.equal(h.downloads[0].blob.type, 'application/json');
  assert.match(h.downloads[0].filename, /^Aura_diag_aura-test_[0-9TZ]+\.json$/);
  assert.equal(h.downloads[0].link.removed, true);
  assert.equal(h.elements.get('downloadReport').disabled, false);
  assert.deepEqual(h.requests.map(r => r.path), ['/api/diag', '/api/diag', '/api/events']);
  await h.fireTimer(1000);
  assert.deepEqual(h.revoked, ['blob:mock-1']);
  assert.equal([...h.timers.values()].filter(t => t.delay === 3000).length, 1);
  h.assertConsumed();
});

test('a later events 503 produces an explicit partial report without reusing the previous events', async () => {
  const current = diag({ uptime_s: 1300, current_capture: true });
  const h = harness([
    reply('/api/diag', diag()),
    reply('/api/diag', diag()), reply('/api/events', events({ old_capture: true })),
    reply('/api/diag', current), reply('/api/events', null, { status: 503 }),
  ]);
  await h.settle();
  await h.click();
  assert.equal(h.report().events.data.old_capture, true);
  await h.click();
  const report = h.report();
  assert.equal(report.complete, false);
  assert.equal(report.diag.ok, true);
  assert.deepEqual(report.diag.data, current);
  assert.equal(report.events.ok, false);
  assert.equal(report.events.error, 'HTTP 503');
  assert.equal(Object.hasOwn(report.events, 'data'), false);
  assert.ok(report.warnings.length > 0);
  assert.match(h.downloads.at(-1).filename, /_partial\.json$/);
  assert.match(h.elements.get('reportStatus').textContent, /Partial report/i);
  h.assertConsumed();
});

test('when both endpoints fail no report is downloaded and retry controls are restored', async () => {
  const h = harness([
    reply('/api/diag', diag()),
    reply('/api/diag', null, { status: 500 }), reply('/api/events', null, { status: 503 }),
  ]);
  await h.settle();
  await h.click();
  assert.equal(h.downloads.length, 0);
  assert.equal(h.elements.get('downloadReport').disabled, false);
  assert.match(h.elements.get('reportStatus').textContent, /not downloaded.*HTTP 500.*HTTP 503/i);
  assert.equal([...h.timers.values()].filter(t => t.delay === 3000).length, 1);
  h.assertConsumed();
});

test('double clicks and ordinary polls cannot overlap a report; events waits for the diagnostic body', async () => {
  const bodyGate = deferred();
  const h = harness([
    reply('/api/diag', diag()), reply('/api/diag', null, { bodyGate }), reply('/api/events', events()),
  ]);
  await h.settle();
  const firstClick = h.click();
  await h.settle();
  assert.equal(h.elements.get('downloadReport').disabled, true);
  await h.click();
  await h.context.refreshDiag();
  assert.equal(h.requests.length, 2, 'Neither duplicate click nor poll started another request');
  assert.equal(h.downloads.length, 0);
  bodyGate.resolve(diag());
  await firstClick;
  assert.equal(h.downloads.length, 1);
  assert.deepEqual(h.requests.map(r => r.path), ['/api/diag', '/api/diag', '/api/events']);
  h.assertConsumed();
});

test('an already running poll finishes before the report takes its own fresh diagnostic capture', async () => {
  const pollBody = deferred();
  const fresh = diag({ fresh_capture: true });
  const h = harness([
    reply('/api/diag', null, { bodyGate: pollBody }), reply('/api/diag', fresh), reply('/api/events', events()),
  ]);
  const click = h.click();
  await h.settle();
  assert.equal(h.requests.length, 1, 'Report waits while initial poll body is pending');
  pollBody.resolve(diag({ stale_poll_only: true }));
  await click;
  assert.deepEqual(h.report().diag.data, fresh);
  assert.equal(h.report().diag.data.stale_poll_only, undefined);
  assert.deepEqual(h.requests.map(r => r.path), ['/api/diag', '/api/diag', '/api/events']);
  h.assertConsumed();
});

for (const stage of ['Headers', 'Body']) {
  test(`request timeout also covers a stalled ${stage.toLowerCase()} read and leaves an explicit partial result`, async () => {
    const h = harness([
      reply('/api/diag', diag()),
      reply('/api/diag', null, { [`stall${stage}`]: true }), reply('/api/events', events()),
    ]);
    await h.settle();
    const click = h.click();
    await h.settle();
    assert.equal(h.requests.length, 2);
    assert.equal(h.elements.get('downloadReport').disabled, true);
    await h.fireTimer(8000);
    await click;
    assert.equal(h.report().complete, false);
    assert.equal(h.report().diag.ok, false);
    assert.equal(h.report().diag.error, 'Request timed out');
    assert.equal(Object.hasOwn(h.report().diag, 'data'), false);
    assert.equal(h.report().events.ok, true);
    assert.equal(h.elements.get('downloadReport').disabled, false);
    assert.equal([...h.timers.values()].filter(t => t.delay === 8000).length, 0);
    h.assertConsumed();
  });
}

test('uptime decreasing between API sections preserves both sections but marks the report partial', async () => {
  const h = harness([
    reply('/api/diag', diag()), reply('/api/diag', diag({ uptime_s: 500 })), reply('/api/events', events({ uptime_s: 2 })),
  ]);
  await h.settle();
  await h.click();
  const report = h.report();
  assert.equal(report.diag.ok, true);
  assert.equal(report.events.ok, true);
  assert.equal(report.complete, false);
  assert.equal(report.diag.data.uptime_s, 500);
  assert.equal(report.events.data.uptime_s, 2);
  assert.match(report.warnings.join(' '), /uptime decreased.*different boots/i);
  assert.match(h.downloads[0].filename, /_partial\.json$/);
  h.assertConsumed();
});

test('rendered GPIO routing distinguishes separate and shared profiles and never turns unknown levels into LOW', async () => {
  const separate = diag();
  const shared = diag({
    device: { firmware: 'test-4_3', hardware_profile: '4_3', hardware_target: 'aura-aq-v1' },
    boot: { i2c_status: 'unknown', sda_high: null },
    i2c_buses: {
      panel: { port: 0, sda_gpio: 8, scl_gpio: 9 },
      sensors: { port: 0, sda_gpio: 8, scl_gpio: 9, shared_with_panel: true },
    },
  });
  const h = harness([reply('/api/diag', separate), reply('/api/diag', shared)]);
  await h.settle();
  const initial = h.elements.get('bootI2cRows').innerHTML;
  assert.match(initial, /I2C1 \/ SDA GPIO44 \/ SCL GPIO6/);
  assert.match(initial, /Separate from panel/);
  assert.match(initial, /Before board initialization \(not live\)/);
  assert.match(initial, /LOW \/ HIGH/);
  await h.context.refreshDiag();
  const updated = h.elements.get('bootI2cRows').innerHTML;
  assert.match(updated, /I2C0 \/ SDA GPIO8 \/ SCL GPIO9/);
  assert.match(updated, /Shared with panel/);
  assert.doesNotMatch(updated, /GPIO44|GPIO6|Separate from panel|LOW|HIGH/);
  assert.match(updated, /Saved SDA \/ SCL<\/div><div class="v">-- \/ --/);
  assert.match(updated, /Snapshot metadata unavailable/);
  h.assertConsumed();
});

test('untrusted diagnostic text is escaped in HTML, literal in log text and JSON, and safe in the filename', async () => {
  const attack = '<img src=x onerror="oops()">&<script>alert(1)</script>';
  const data = diag();
  data.network.hostname = attack;
  data.network.wifi_ssid = attack;
  data.device.firmware = attack;
  data.boot.i2c_status = attack;
  data.last_errors[0].message = attack;
  const h = harness([reply('/api/diag', data), reply('/api/diag', data), reply('/api/events', events())]);
  await h.settle();
  for (const id of ['networkRows', 'systemRows', 'bootI2cRows']) {
    assert.doesNotMatch(h.elements.get(id).innerHTML, /<img|<script>/);
    assert.match(h.elements.get(id).innerHTML, /&lt;img|&lt;script/);
  }
  assert.match(h.elements.get('errors').textContent, /<img src=x/);
  assert.equal(h.elements.get('errors').innerHTML, '', 'Log messages were assigned as text, not HTML');
  await h.click();
  assert.equal(h.report().diag.data.last_errors[0].message, attack);
  assert.equal(h.report().diag.data.network.hostname, attack);
  assert.match(h.downloads[0].filename, /^[a-zA-Z0-9_.-]+\.json$/);
  assert.doesNotMatch(h.downloads[0].filename, /[<>/\\":]/);
  h.assertConsumed();
});

test('invalid events payload is not mistaken for a successful section', async () => {
  const h = harness([
    reply('/api/diag', diag()), reply('/api/diag', diag()), reply('/api/events', events({ events: { not: 'an array' } })),
  ]);
  await h.settle();
  await h.click();
  assert.equal(h.report().complete, false);
  assert.equal(h.report().events.ok, false);
  assert.equal(h.report().events.error, 'Invalid payload');
  assert.equal(Object.hasOwn(h.report().events, 'data'), false);
  h.assertConsumed();
});
