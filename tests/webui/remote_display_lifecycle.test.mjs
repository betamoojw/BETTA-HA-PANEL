/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw; adapted by andre68723 from commit 57df6a3.
 */
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { boundEditor } from './bound_editor.mjs';
const require = createRequire(import.meta.url);
const __dirname = fileURLToPath(new URL('.', import.meta.url));
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../components/webui/www/js/remote-display.js'), 'utf8');

function harness(fetchImpl = async () => ({ok: true, json: async () => ({token: 'example'})})) {
  const elements = new Map(), timers = new Map(), windows = {}, documents = {}, sockets = [];
  let nextTimer = 0, downloads = 0;
  function element(id) {
    if (!elements.has(id)) {
      const classes = new Set(), listeners = {}, captured = new Set();
      elements.set(id, {id, listeners, disabled: false, checked: false, style: {setProperty() {}},
        classList: {toggle(k, value) { if (value) classes.add(k); else classes.delete(k); }, contains: k => classes.has(k)},
        getContext: () => ({putImageData() {}}), getBoundingClientRect: () => ({left: 0, top: 0, width: 480, height: 480}),
        addEventListener(k, fn) { listeners[k] = fn; },
        setPointerCapture(k) { captured.add(k); }, hasPointerCapture: k => captured.has(k),
        releasePointerCapture(k) { captured.delete(k); listeners.lostpointercapture?.(); },
        toBlob(fn) { fn({}); }, click() {
          if (id === 'download') downloads++;
          listeners.click?.();
          return this.onclick?.();
        },
      });
    }
    return elements.get(id);
  }
  class Socket {
    static OPEN = 1;
    constructor() { this.readyState = 1; this.bufferedAmount = 0; this.sent = []; sockets.push(this); }
    send(text) { this.sent.push(JSON.parse(text)); }
    close() { this.readyState = 3; }
    receive(message) { this.onmessage({data: message instanceof ArrayBuffer ? message : JSON.stringify(message)}); }
  }
  const document = {hidden: false, documentElement: {lang: 'en'}, getElementById: element,
    createElement: () => element('download'), addEventListener(k, fn) { documents[k] = fn; }};
  const context = {document, fetch: fetchImpl, WebSocket: Socket, ArrayBuffer, DataView, Uint8ClampedArray,
    location: {protocol: 'http:', host: 'panel.local'}, performance: {now: () => 1000},
    ImageData: class {constructor(data) { this.data = data; }}, MutationObserver: class {observe() {}},
    URL: {createObjectURL: () => 'blob:test', revokeObjectURL() {}},
    setTimeout(fn, ms) { timers.set(++nextTimer, {fn, ms}); return nextTimer; }, clearTimeout(id) { timers.delete(id); },
    setInterval(fn, ms) { timers.set(++nextTimer, {fn, ms, interval: true}); return nextTimer; }, clearInterval(id) { timers.delete(id); },
    addEventListener(k, fn) { windows[k] = fn; },
  };
  context.window = context;
  vm.runInNewContext(source, context);
  return {context, element, sockets, timers, windows, documents, downloads: () => downloads};
}
const settle = async () => { for (let i = 0; i < 8; i++) await Promise.resolve(); };
function hello(socket, epoch = 42) { socket.receive({type: 'hello', version: 1, epoch, width: 480, height: 480, format: 'rgb565le'}); }

function deferred() {
  let resolve;
  const promise = new Promise(r => { resolve = r; });
  return { promise, resolve };
}

function startWorkspace(h) {
  const tab = h.element('remoteTabBtn');
  tab.classList.toggle('hidden', true);
  const run = boundEditor({ elements: { remoteTabBtn: tab }, bind: false });
  const settings = deferred(), diagnostics = deferred();
  const readingSettings = deferred(), readingDiagnostics = deferred();
  let saved = JSON.stringify({pane: 'settings', section: 'settingsHardwareSection'});
  Object.assign(run('window'), {
    RemoteDisplay: h.context.RemoteDisplay, setInterval() {},
    sessionStorage: {getItem: () => saved, setItem: (_, value) => { saved = value; }},
    readSettings: () => { readingSettings.resolve(); return settings.promise; },
    readDiagnostics: () => { readingDiagnostics.resolve(); return diagnostics.promise; },
  });
  // Gate panel reads while running the actual bootstrap, tab bindings, capability
  // check, navigation, socket controller and frame decoder.
  run(`
    el.actionsPanel.children = []; el.settingsNavButtons = []; el.settingsContentSections = [];
    for (const name of ['initThemeSection', 'setStatus', 'renderCanvas', 'renderHaDiagnosticsBanner',
      'renderAppVersion', 'renderFirmwareInfo', 'renderBoardLabel', 'renderBuildTime',
      'applyCanvasGeometry', 'applyLayoutLimits', 'updateTextStepInfo', 'renderHaAuthBanner', 'renderCrashBanner', 'loadOtaStatus',
      'loadLayout', 'loadEntities', 'refreshStates', 'loadEnergyPreview']) globalThis[name] = () => {};
    apiGet = async () => ({version: 'test', remote_display_supported: true});
    loadSettingsForBootstrap = window.readSettings;
    loadHaDiagnostics = window.readDiagnostics;
  `);
  const done = run('bootstrap()');
  return {run, done, tab, readingSettings: readingSettings.promise, readingDiagnostics: readingDiagnostics.promise,
    finishSettings: () => settings.resolve({wifi: {configured: true}, ha: {configured: true}}),
    finishDiagnostics: diagnostics.resolve};
}

for (const when of ['settings', 'diagnostics']) {
  test(`opening Live Display while ${when} loads keeps the original socket through the first frame`, async () => {
    const h = harness(), boot = startWorkspace(h);
    await boot.readingSettings;
    assert.equal(boot.tab.classList.contains('hidden'), false);
    assert.equal(boot.run('editor.appVersion'), 'test');
    if (when === 'diagnostics') {
      boot.finishSettings();
      await boot.readingDiagnostics;
      assert.equal(boot.run('editor.activePane'), 'settings');
    }
    boot.tab.click();
    await settle();
    const first = h.sockets[0];
    assert.equal(boot.run('editor.activePane'), 'remote');
    assert.equal(boot.run('editor.activeSettingsSection'), 'settingsHardwareSection');
    assert.equal(h.element('remoteScreenshot').disabled, true);
    boot.finishSettings(); boot.finishDiagnostics();
    await boot.done;
    assert.equal(first.readyState, 1, 'startup closed the socket before its first frame');
    assert.equal(boot.run('editor.activePane'), 'remote');
    hello(first); paint(first);
    assert.equal(h.sockets.length, 1);
    assert.ok(first.sent.some(m => m.type === 'ack' && m.epoch === 42 && m.seq === 1));
    assert.equal(h.element('remoteScreenshot').disabled, false);
  });
}

test('Disconnect before the first frame survives startup and Connect opens one fresh viewing session', async () => {
  const h = harness(), boot = startWorkspace(h);
  await boot.readingSettings;
  boot.tab.click(); await settle();
  const old = h.sockets[0];
  const delayedClose = old.onclose;
  hello(old);
  h.element('remoteDisconnect').click();
  assert.equal(old.readyState, 3);
  boot.finishSettings(); boot.finishDiagnostics(); await boot.done;
  assert.equal(boot.run('editor.activePane'), 'remote');
  assert.equal(h.element('remoteStatus').textContent, 'Disconnected');
  assert.equal(h.timers.size, 0);
  assert.equal(h.sockets.length, 1);
  await h.element('remoteConnect').click();
  const fresh = h.sockets[1];
  delayedClose();
  hello(fresh, 43); paint(fresh, 43);
  assert.equal(h.sockets.length, 2);
  assert.equal(h.element('remoteStatus').textContent, 'Viewing');
  assert.equal(h.element('remoteControl').checked, false);
  assert.ok(fresh.sent.some(m => m.type === 'ack' && m.epoch === 43 && m.seq === 1));
});

test('switching back to Layout while the ticket is pending wins over saved Settings', async () => {
  const ticket = deferred(), h = harness(() => ticket.promise), boot = startWorkspace(h);
  await boot.readingSettings;
  boot.tab.click();
  boot.run('el.layoutTabBtn.onclick()');
  ticket.resolve({ok: true, json: async () => ({token: 'late'})});
  boot.finishSettings(); boot.finishDiagnostics(); await boot.done; await settle();
  assert.equal(boot.run('editor.activePane'), 'layout');
  assert.equal(h.sockets.length, 0);
  assert.equal(h.timers.size, 0);
});

test('reconnecting while the original ticket is in flight ignores its late reply', async () => {
  const tickets = [deferred(), deferred()];
  let requests = 0;
  const h = harness(() => tickets[requests++].promise), boot = startWorkspace(h);
  await boot.readingSettings;
  boot.tab.click();
  h.element('remoteDisconnect').click();
  const connecting = h.element('remoteConnect').click();
  tickets[1].resolve({ok: true, json: async () => ({token: 'current'})});
  await connecting;
  tickets[0].resolve({ok: true, json: async () => ({token: 'expired'})});
  boot.finishSettings(); boot.finishDiagnostics(); await boot.done; await settle();
  assert.equal(requests, 2);
  assert.equal(h.sockets.length, 1);
  hello(h.sockets[0], 51); paint(h.sockets[0], 51);
  assert.equal(h.element('remoteStatus').textContent, 'Viewing');
  assert.ok(h.sockets[0].sent.some(m => m.type === 'ack' && m.epoch === 51 && m.seq === 1));
});

test('a transport close during a keyframe retries in the selected pane with a fresh keyframe', async () => {
  const h = harness(), boot = startWorkspace(h);
  await boot.readingSettings;
  boot.tab.click(); await settle();
  const old = h.sockets[0];
  hello(old);
  old.receive({type: 'begin', epoch: 42, seq: 1, base: 0, key: true, count: 225});
  old.readyState = 3;
  old.onclose();
  boot.finishSettings(); boot.finishDiagnostics(); await boot.done;
  assert.equal(boot.run('editor.activePane'), 'remote');
  assert.equal(h.element('remoteScreenshot').disabled, true);
  assert.equal(old.sent.some(m => m.type === 'ack'), false);
  const retry = [...h.timers.values()].find(t => t.ms === 500);
  assert.ok(retry);
  retry.fn(); await settle();
  hello(h.sockets[1], 52); paint(h.sockets[1], 52);
  assert.equal(h.sockets.length, 2);
  assert.equal(h.element('remoteStatus').textContent, 'Viewing');
  assert.ok(h.sockets[1].sent.some(m => m.type === 'ack' && m.epoch === 52 && m.seq === 1));
});

test('low panel memory explains the refusal and Connect can retry after recovery', async () => {
  let low = true;
  const h = harness(async () => low
    ? {ok: false, status: 503, text: async () => 'Insufficient PSRAM for live display'}
    : {ok: true, json: async () => ({token: 'recovered'})});
  h.context.RemoteDisplay.setVisible(true); await settle();
  assert.match(h.element('remoteStatus').textContent, /memory.*reconnect.*503/i);
  assert.equal(h.timers.size, 0);
  assert.equal(h.sockets.length, 0);
  low = false;
  await h.element('remoteConnect').onclick();
  hello(h.sockets[0]); paint(h.sockets[0]);
  assert.equal(h.element('remoteStatus').textContent, 'Viewing');
});

test('only temporary session failures schedule another ticket request', async () => {
  for (const [status, detail, message, retries] of [
    [409, '', /viewer is already connected/i, true],
    [503, 'Live display not ready or paused for OTA', /display is not ready/i, true],
    [503, 'Server busy', /Unavailable or busy/, true],
    [403, '', /HTTP 403/, false],
    [404, '', /HTTP 404/, false],
  ]) {
    const h = harness(async () => ({ok: false, status, text: async () => detail}));
    h.context.RemoteDisplay.setVisible(true); await settle();
    assert.match(h.element('remoteStatus').textContent, message);
    assert.equal([...h.timers.values()].some(timer => timer.ms === 500), retries);
    assert.equal(h.sockets.length, 0);
  }
});

test('closing the pane while a refusal body arrives leaves it disconnected', async () => {
  let finish;
  const h = harness(async () => ({ok: false, status: 503, text: () => new Promise(resolve => { finish = resolve; })}));
  h.context.RemoteDisplay.setVisible(true); await settle();
  h.context.RemoteDisplay.setVisible(false);
  finish('Live display not ready or paused for OTA'); await settle();
  assert.equal(h.element('remoteStatus').textContent, 'Disconnected');
  assert.equal(h.timers.size, 0);
});

test('missing server hello times out; post-handshake hello starts frame requests', async () => {
  const h = harness(); h.context.RemoteDisplay.setVisible(true); await settle();
  const first = h.sockets[0];
  assert.equal(first.sent.length, 0);
  const deadline = [...h.timers.values()].find(t => t.ms === 5000);
  assert.ok(deadline);
  deadline.fn();
  assert.equal(first.readyState, 3);
  const retry = [...h.timers.values()].find(t => t.ms === 500);
  assert.ok(retry);
  retry.fn(); await settle();
  const second = h.sockets[1]; hello(second);
  assert.equal([...h.timers.values()].some(t => t.ms === 5000), false);
  const request = [...h.timers.values()].find(t => t.ms === 0);
  assert.ok(request); request.fn();
  assert.equal(second.sent.at(-1).type, 'frame');
  assert.equal(second.sent.at(-1).epoch, 42);
});

function paint(socket, epoch = 42, seq = 1) {
  const key = seq === 1, count = key ? 225 : 1;
  socket.receive({type: 'begin', epoch, seq, base: seq - 1, key, count});
  for (let index = 0; index < count; ++index) {
    const b = new ArrayBuffer(2078), v = new DataView(b);
    v.setUint32(0, 0x31524442, true); v.setUint8(4, 1); v.setUint16(6, 30, true);
    v.setUint32(8, epoch, true); v.setUint32(12, seq, true); v.setUint16(16, index, true);
    v.setUint16(18, index % 15 * 32, true); v.setUint16(20, Math.floor(index / 15) * 32, true);
    v.setUint16(22, 32, true); v.setUint16(24, 32, true); v.setUint32(26, 2048, true);
    socket.receive(b);
  }
  socket.receive({type: 'end', seq});
}

test('capture contention retries on the same socket before a keyframe and between deltas', async () => {
  const h = harness();
  h.context.RemoteDisplay.setVisible(true); await settle();
  const ws = h.sockets[0], epoch = 7;
  hello(ws, epoch);
  for (const seq of [1, 2, 3]) {
    ws.receive({type: 'idle', retry: 200});
    ws.receive({type: 'pong', control: false});
    assert.equal(h.element('remoteScreenshot').disabled, seq === 1);
    const retry = [...h.timers.values()].find(t => t.ms === 200);
    assert.ok(retry);
    retry.fn();
    assert.deepEqual(ws.sent.at(-1), {type: 'frame', epoch});
    paint(ws, epoch, seq);
    assert.deepEqual(ws.sent.at(-1), {type: 'ack', epoch, seq});
  }
  assert.equal(h.sockets.length, 1);
  assert.equal(ws.readyState, 1);
  assert.equal(h.element('remoteStatus').textContent, 'Viewing');
});

test('leaving the pane cancels control, closes its socket and clears its timers', async () => {
  const h = harness();
  h.context.RemoteDisplay.setVisible(true); await settle();
  const ws = h.sockets[0]; hello(ws); paint(ws);
  h.element('remoteControl').onchange({target: {checked: true}});
  ws.receive({type: 'control', enabled: true});
  h.element('remoteCanvas').listeners.pointerdown({isPrimary: true, button: 0, pointerId: 1, clientX: 120, clientY: 80, preventDefault() {}});
  assert.equal(ws.sent.at(-1).kind, 0);
  h.context.RemoteDisplay.setVisible(false);
  assert.ok(ws.sent.some(m => m.type === 'pointer' && m.kind === 3));
  assert.equal(ws.sent.at(-1).enabled, false);
  assert.equal(ws.readyState, 3);
  assert.equal(h.timers.size, 0);
  assert.equal(h.element('remoteControl').disabled, true);
  assert.equal(h.element('remoteScreenshot').disabled, true);
});

test('hiding and showing the page reconnects with control disabled and no stale screenshot', async () => {
  const h = harness(); h.context.RemoteDisplay.setVisible(true); await settle();
  hello(h.sockets[0]); paint(h.sockets[0]);
  h.element('remoteScreenshot').onclick(); assert.equal(h.downloads(), 1);
  h.context.document.hidden = true; h.documents.visibilitychange();
  assert.equal(h.sockets[0].readyState, 3);
  h.context.document.hidden = false; h.documents.visibilitychange(); await settle();
  assert.equal(h.sockets.length, 2);
  assert.equal(h.element('remoteScreenshot').disabled, true);
  assert.equal(h.element('remoteControl').checked, false);
});

test('a late token response cannot reconnect a pane that was closed', async () => {
  let resolve;
  const h = harness(() => new Promise(r => { resolve = r; }));
  h.context.RemoteDisplay.setVisible(true);
  h.context.RemoteDisplay.setVisible(false);
  resolve({ok: true, json: async () => ({token: 'late'})}); await settle();
  assert.equal(h.sockets.length, 0); assert.equal(h.timers.size, 0);
});

test('incomplete frame never enables controls or screenshot download', async () => {
  const h = harness(); h.context.RemoteDisplay.setVisible(true); await settle();
  const ws = h.sockets[0]; hello(ws);
  ws.receive({type: 'begin', epoch: 42, seq: 1, base: 0, key: true, count: 225});
  ws.receive({type: 'end', seq: 1});
  assert.equal(ws.readyState, 3);
  assert.equal(h.element('remoteControl').disabled, true);
  assert.equal(h.element('remoteScreenshot').disabled, true);
});

test('window blur cancels a held remote pointer and revokes control', async () => {
  const h = harness(); h.context.RemoteDisplay.setVisible(true); await settle();
  const ws = h.sockets[0]; hello(ws); paint(ws); ws.receive({type: 'control', enabled: true});
  h.element('remoteCanvas').listeners.pointerdown({isPrimary: true, button: 0, pointerId: 7, clientX: 479, clientY: 479, preventDefault() {}});
  h.windows.blur();
  assert.equal(ws.sent.at(-2).kind, 3); assert.equal(ws.sent.at(-1).enabled, false);
  assert.equal(h.element('remoteControl').checked, false);
});
