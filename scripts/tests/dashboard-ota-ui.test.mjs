import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import vm from "node:vm";

const source = readFileSync(
  new URL("../../src/web/WebTemplatesDashboardAp.cpp", import.meta.url),
  "utf8",
);
const script = source.match(/<script>\r?\n([\s\S]*?)<\/script>/)?.[1];
assert.ok(script, "The embedded dashboard script must be present");

function functionRange(startName, endDeclaration) {
  const start = script.indexOf(`function ${startName}(`);
  const end = script.indexOf(endDeclaration, start);
  assert.ok(start >= 0 && end > start, `Missing dashboard functions: ${startName}`);
  return script.slice(start, end);
}

// Execute the real callbacks, not a second implementation of the OTA UI policy.
const otaUiScript = functionRange("initOtaUI", "async function refreshState(");
const recoveryScript = functionRange(
  "stopOtaRecoveryWatcher",
  "function createOtaUploadTimeoutController(",
);

function createHarness() {
  const elements = new Map();
  for (const id of ["otaFile", "otaUploadBtn", "otaStatus", "otaProgress", "otaPrecheck"]) {
    elements.set(id, {
      disabled: false,
      files: [],
      value: "",
      textContent: "",
      className: "",
      style: { width: "0%" },
      listeners: new Map(),
      addEventListener(type, callback) { this.listeners.set(type, callback); },
    });
  }
  const requests = [];
  const timers = new Map();
  const counters = { reloads: 0, prepares: 0, probes: 0 };
  let timerId = 0;
  const context = {
    document: { getElementById: id => elements.get(id) || null },
    window: { location: { reload: () => { counters.reloads += 1; } } },
    AbortController,
    setTimeout: callback => { timers.set(++timerId, callback); return timerId; },
    clearTimeout: id => timers.delete(id),
    otaUploadInFlight: false,
    otaAwaitingPhysicalConfirm: false,
    otaAwaitingDeviceOutcome: false,
    otaRestartPending: false,
    otaReconnectGraceUntilMs: 0,
    otaRecoveryActive: false,
    otaRecoveryTimer: null,
    otaRecoveryProbeController: null,
    lastStateOtaBusy: false,
    lastStateOkAtMs: Date.now(),
    OTA_STALE_STATE_THRESHOLD_S: 10,
    OTA_RECONNECT_GRACE_MS: 20000,
    OTA_RECOVERY_PROBE_TIMEOUT_MS: 2500,
    safeStateNetwork: () => ({ mode: "sta", rssi: -50 }),
    secondsSince: () => 0,
    isNum: value => typeof value === "number" && Number.isFinite(value),
    updateNetStatusBanner: () => {},
    updateOtaPrecheck: () => {},
    cacheStatePayload: () => {},
    probePayload: null,
    prepareOtaUpload: async () => { counters.prepares += 1; return { confirm_id: 8 }; },
    probeLatestOtaState: async () => context.probePayload,
    fetch: async () => {
      counters.probes += 1;
      return { ok: true, json: async () => context.probePayload };
    },
    FormData: class {
      values = [];
      append(...args) { this.values.push(args); }
    },
    XMLHttpRequest: class {
      upload = {};
      status = 0;
      responseText = "";
      constructor() { requests.push(this); }
      open(method, url) { this.method = method; this.url = url; }
      send(body) { this.body = body; }
    },
    createOtaUploadTimeoutController: () => ({
      markSettled: () => {},
      noteProgress: () => {},
      noteUploadComplete: () => {},
      consumeAbortMessage: () => "",
    }),
  };
  vm.createContext(context);
  vm.runInContext(recoveryScript + "\n" + otaUiScript + "\ninitOtaUI();", context);

  return {
    context,
    elements,
    requests,
    counters,
    timers,
    async startUpload(name = "firmware.bin") {
      elements.get("otaFile").files = [{ name, size: 4096 }];
      await elements.get("otaUploadBtn").listeners.get("click")();
      return requests.at(-1);
    },
    async runNextTimer() {
      const [id, callback] = timers.entries().next().value;
      timers.delete(id);
      await callback();
    },
  };
}

function rejectUpload(xhr, message, errorCode = "HARDWARE_TARGET_MISMATCH") {
  xhr.status = 400;
  xhr.responseText = JSON.stringify({
    success: false,
    error: message,
    error_code: errorCode,
    written: 0,
    rebooting: false,
  });
  xhr.onload();
}

function assertRejectedUi(harness, message) {
  const { context, elements, counters, timers } = harness;
  assert.equal(elements.get("otaStatus").textContent, message);
  assert.equal(elements.get("otaStatus").className, "ota-status err");
  assert.equal(elements.get("otaProgress").style.width, "0%");
  assert.equal(elements.get("otaFile").disabled, false);
  assert.equal(elements.get("otaUploadBtn").disabled, false);
  assert.equal(context.otaUploadInFlight, false);
  assert.equal(context.otaAwaitingPhysicalConfirm, false);
  assert.equal(context.otaAwaitingDeviceOutcome, false);
  assert.equal(context.otaRestartPending, false);
  assert.equal(context.otaReconnectGraceUntilMs, 0);
  assert.equal(context.otaRecoveryActive, false);
  assert.equal(counters.reloads, 0);
  assert.equal(timers.size, 0, "A rejection must not schedule reload or status dismissal");
}

test("the entire embedded dashboard script parses", () => {
  assert.doesNotThrow(() => new vm.Script(script));
});

test("OTA file and persistent status are associated and accessible", () => {
  assert.match(source, /id="otaFile"[^>]*aria-describedby="otaFileHint otaStatus"/);
  assert.match(source, /id="otaStatus"[^>]*role="status"[^>]*aria-live="polite"[^>]*aria-atomic="true"/);
  assert.match(source, /id="otaFileHint">Use the OTA \.bin for this Aura model\./);
});

for (const message of [
  'This firmware is for Aura AQ 4.3". This device is Aura AQ 7". Choose the 7" firmware.',
  'This firmware is for Aura AQ 7". This device is Aura AQ 4.3". Choose the 4.3" firmware.',
]) {
  test(`device rejection remains visible without a reboot: ${message}`, async () => {
    const harness = createHarness();
    const xhr = await harness.startUpload("renamed.bin");
    xhr.upload.onprogress({ lengthComputable: true, loaded: 4096, total: 4096 });
    assert.equal(harness.elements.get("otaProgress").style.width, "100%");
    rejectUpload(xhr, message);
    assertRejectedUi(harness, message);

    // Browser upload notifications and onloadend can arrive after an early rejection.
    xhr.upload.onprogress({ lengthComputable: true, loaded: 4096, total: 4096 });
    xhr.upload.onload();
    await xhr.onloadend();
    assertRejectedUi(harness, message);
    assert.equal(harness.requests.length, 1, "Do not automatically retry a rejected file");
  });
}

test("unlabelled legacy image rejection is displayed unchanged", async () => {
  const harness = createHarness();
  const xhr = await harness.startUpload();
  const message = "This file has no supported hardware label. Use a current OTA firmware file.";
  rejectUpload(xhr, message, "HARDWARE_TARGET_MISSING");
  assertRejectedUi(harness, message);
});

test("regular device-state precheck updates do not dismiss an OTA rejection", async () => {
  const harness = createHarness();
  const xhr = await harness.startUpload();
  const message = "Wrong hardware model. Choose the correct firmware.";
  rejectUpload(xhr, message);
  vm.runInContext(
    functionRange("updateOtaPrecheck", "function setOtaGlobalOverlay(") +
      "\nupdateOtaPrecheck({mode: 'sta', rssi: -50});\n" +
      "updateOtaPrecheck({mode: 'ap'});",
    harness.context,
  );
  assert.match(harness.elements.get("otaPrecheck").textContent, /AP mode active/);
  assertRejectedUi(harness, message);
});

test("selecting another file clears the old error and allows a successful retry", async () => {
  const harness = createHarness();
  const first = await harness.startUpload("wrong-model.bin");
  rejectUpload(first, "Wrong hardware model. Choose the correct firmware.");
  const fileInput = harness.elements.get("otaFile");
  fileInput.files = [{ name: "correct-model.bin", size: 4096 }];
  fileInput.listeners.get("change")();
  assert.equal(harness.elements.get("otaStatus").className, "ota-status");
  assert.match(harness.elements.get("otaStatus").textContent, /check compatibility/);

  await harness.elements.get("otaUploadBtn").listeners.get("click")();
  assert.equal(harness.requests.length, 2);
  assert.equal(harness.counters.prepares, 2);
  const second = harness.requests.at(-1);
  second.status = 200;
  second.responseText = JSON.stringify({ success: true, rebooting: true });
  second.onload();
  assert.equal(harness.context.otaRestartPending, true);
  assert.equal(harness.elements.get("otaStatus").className, "ota-status ok");
  assert.equal(harness.elements.get("otaProgress").style.width, "100%");
  assert.equal(harness.elements.get("otaUploadBtn").disabled, true);
  assert.equal(harness.context.otaRecoveryActive, true);
});

test("file change cannot erase feedback during an active upload", async () => {
  const harness = createHarness();
  await harness.startUpload();
  const message = harness.elements.get("otaStatus").textContent;
  harness.elements.get("otaFile").listeners.get("change")();
  assert.equal(harness.elements.get("otaStatus").textContent, message);
  assert.equal(harness.elements.get("otaFile").disabled, true);
});

test("failed-state reconciliation displays the rejection when the direct response is lost", async () => {
  const harness = createHarness();
  const xhr = await harness.startUpload();
  const message = "Wrong hardware model. No firmware was written.";
  harness.context.probePayload = { ota_busy: false, ota: { status: "failed", error: message } };
  await xhr.onerror();
  assertRejectedUi(harness, message);
});

test("recovery watcher rejection restores the file selector and clears stale progress", async () => {
  const harness = createHarness();
  await harness.startUpload();
  harness.elements.get("otaProgress").style.width = "100%";
  const message = "Firmware has no supported hardware label. Choose a current file.";
  harness.context.probePayload = { ota_busy: false, ota: { status: "failed", error: message } };
  vm.runInContext("setOtaAwaitingDeviceOutcome();", harness.context);
  assert.equal(harness.elements.get("otaFile").disabled, true);
  await harness.runNextTimer();
  assertRejectedUi(harness, message);
  assert.equal(harness.counters.probes, 1);
});
