// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

const availabilityPath = "/api/ha/light_entities?status_only=1";
async function settle() {
  for (let i = 0; i < 30; ++i) await Promise.resolve();
}

function chooser() {
  const editor = loadEditor();
  const { run } = editor;
  run(`
    function control() {
      const classes = new Set(["hidden"]);
      return { value: "", textContent: "", hidden: false, dataset: {},
        classList: {
          add: name => classes.add(name), remove: name => classes.delete(name),
          contains: name => classes.has(name),
          toggle: (name, enabled) => enabled ? classes.add(name) : classes.delete(name),
        },
        addEventListener() {}, focus() { this.focused = true; },
      };
    }
    for (const name of ["addTileBtn", "addTileOverlay", "addTileCloseBtn", "addTileSearch",
      "addTileSearchStatus", "addTileSearchRetry", "addTileResults", "addTileHaStatus",
      "addTileHaActions", "addTileHaRetry", "addTileHaSettings", "settingsHaUrl"]) el[name] = control();
    editor.layout = { pages: [{ id: "page", widgets: [{ id: "graph", type: "graph" }] }] };
    editor.selectedPageId = "page";
    globalThis.paths = [];
    globalThis.chosen = [];
    globalThis.haStatus = "not_configured";
    globalThis.targets = ["addLightTileBtn", "addEmptyTileBtn"];
    globalThis.typeButtons = targets.map(addTarget => ({ dataset: { addTarget } }));
    document.querySelectorAll = selector => selector.endsWith("[data-add-target]") ? typeButtons : [];
    document.getElementById = id => targets.includes(id) ? { click: () => chosen.push(id) } : null;
    refreshInspectorEntityRows = () => {};
    window.clearTimeout = clearTimeout;
    globalThis.readHa = async () => ({status: haStatus});
    apiGet = (path, signal) => {
      paths.push(path);
      return path === ${JSON.stringify(availabilityPath)}
        ? readHa(signal) : Promise.resolve({status: "ready", connected: true, pending: false, items: []});
    };
    setActivePane = pane => chosen.push(pane);
    setActiveSettingsSection = section => chosen.push(section);
    bindAddTileDialog();
  `);
  return editor;
}

test("missing credentials give settings guidance and only local widgets can be added", async () => {
  const { run } = chooser();
  run("el.addTileBtn.onclick()");
  await settle();
  assert.match(run("el.addTileHaStatus.textContent"), /WebSocket URL and a long-lived access token/);
  assert.equal(run("el.addTileHaSettings.hidden"), false);
  await run("typeButtons[0].onclick()");
  assert.equal(run("chosen.length"), 0);
  assert.equal(run("el.addTileOverlay.classList.contains('hidden')"), false);

  run("el.addTileHaSettings.onclick()");
  assert.deepEqual(Array.from(run("chosen")), ["settings", "settingsHaSection"]);
  assert.equal(run("el.settingsHaUrl.focused"), true);
  run("el.addTileBtn.onclick(); typeButtons[1].onclick()");
  await settle();
  assert.equal(run("chosen.at(-1)"), "addEmptyTileBtn");
  assert.ok(run(`paths.every(path => path === "${availabilityPath}")`));
});

test("retry rechecks the live connection and then searches with the original text", async () => {
  const { run } = chooser();
  run("el.addTileBtn.onclick()");
  await settle();
  run("el.addTileSearch.value = 'bedroom'; haStatus = 'disconnected'; el.addTileHaRetry.onclick()");
  await settle();
  assert.match(run("el.addTileHaStatus.textContent"), /not connected.*WebSocket URL.*access token/);
  assert.equal(run("paths.length"), 2);
  run("haStatus = 'ready'; el.addTileHaRetry.onclick()");
  await settle();
  assert.equal(run("el.addTileSearch.value"), "bedroom");
  assert.equal(run("el.addTileHaStatus.textContent"), "");
  assert.match(run("paths.at(-1)"), /domain=any&search=bedroom&refresh=1/);
  await run("typeButtons[0].onclick()");
  assert.equal(run("chosen[0]"), "addLightTileBtn");
  assert.equal(run("el.addTileOverlay.classList.contains('hidden')"), true);
  assert.equal(run("editor.layout.pages[0].widgets[0].id"), "graph");
});

test("rejection after opening is shown to installers without exposing admin settings", async () => {
  const { run } = chooser();
  run("userAccess.role = 'installer'; haStatus = 'ready'; el.addTileBtn.onclick()");
  await settle();
  run("haStatus = 'token_rejected'");
  await run("typeButtons[0].onclick()");
  assert.match(run("el.addTileHaStatus.textContent"), /rejected.*access token.*administrator/);
  assert.equal(run("el.addTileHaSettings.hidden"), true);
  run("el.addTileHaSettings.onclick()");
  assert.equal(run("chosen.length"), 0);
});

test("closing a pending chooser cancels the check and suppresses its late result", async () => {
  const { run } = chooser();
  run(`
    readHa = signal => new Promise(resolve => { globalThis.finishOld = resolve; globalThis.oldSignal = signal; });
    el.addTileBtn.onclick();
  `);
  const oldClick = run("typeButtons[0].onclick()");
  assert.equal(run("paths.length"), 1);
  run(`
    el.addTileCloseBtn.onclick();
    readHa = async () => ({status: "forbidden"});
    el.addTileBtn.onclick();
    finishOld({status: "ready"});
  `);
  await oldClick;
  await settle();
  assert.equal(run("oldSignal.aborted"), true);
  assert.equal(run("chosen.length"), 0);
  assert.match(run("el.addTileHaStatus.textContent"), /IP address is blocked/);
  assert.equal(run("el.addTileOverlay.classList.contains('hidden')"), false);
});

test("typing a new search invalidates a type choice waiting for HA", async () => {
  const { run } = chooser();
  run(`
    window.setTimeout = () => 1;
    readHa = () => new Promise(resolve => { globalThis.finishChoice = resolve; });
    el.addTileBtn.onclick();
  `);
  const choice = run("typeButtons[0].onclick()");
  run(`
    el.addTileSearch.value = "sensor";
    el.addTileSearch.oninput();
    finishChoice({status: "ready"});
  `);
  await choice;
  assert.equal(run("chosen.length"), 0);
  assert.equal(run("el.addTileSearch.value"), "sensor");
  assert.equal(run("el.addTileOverlay.classList.contains('hidden')"), false);
});

test("a failed status read offers retry without reporting a missing token", async () => {
  const { run } = chooser();
  run("readHa = async () => { throw new Error('offline'); }; el.addTileBtn.onclick()");
  await settle();
  assert.match(run("el.addTileHaStatus.textContent"), /panel is reachable and retry/);
  assert.equal(run("el.addTileHaSettings.hidden"), true);
  assert.equal(run("el.addTileHaActions.classList.contains('hidden')"), false);
});

test("the status deadline aborts the read and offers retry", async () => {
  const { run } = chooser();
  run(`
    setTimeout = (callback, delay) => { globalThis.deadline = callback; globalThis.delay = delay; return 1; };
    clearTimeout = () => {};
    readHa = signal => new Promise((_, reject) => signal.addEventListener('abort', () => reject(new Error('aborted'))));
    el.addTileBtn.onclick();
  `);
  assert.equal(run("delay"), 5000);
  run("deadline()");
  await settle();
  assert.match(run("el.addTileHaStatus.textContent"), /panel is reachable and retry/);
  assert.equal(run("el.addTileHaActions.classList.contains('hidden')"), false);
});
