// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

function setup() {
  const nodes = new Map();
  function control() {
    const classes = new Set();
    return {
      value: "", textContent: "", style: {}, classList: {
        toggle(key, flag) { if (flag) classes.add(key); else classes.delete(key); },
        contains: key => classes.has(key), add: key => classes.add(key), remove: key => classes.delete(key),
      },
      focus() {}, replaceChildren(...options) { this.options = options; this.value = options[0]?.value ?? ""; },
    };
  }
  document.getElementById = id => {
    if (!id.startsWith("knx") && !id.startsWith("addTile")) return null;
    if (!nodes.has(id)) nodes.set(id, control());
    return nodes.get(id);
  };
  document.createElement = control;
  window.clearTimeout = clearTimeout;
  window.setTimeout = () => 0;
  el.addTileOverlay = control();
  editor.layout = { version: 1, pages: [{ id: "room", widgets: [
    { id: "old", title: "HA sensor", type: "sensor", entity_id: "sensor.room", rect: {x: 0,y: 0,w: 200,h: 100} },
  ] }] };
  editor.selectedPageId = "room";
  renderAll = renderCanvas = setStatus = refreshLayoutLimits = loadHaDiagnostics = () => {};
  globalThis.objects = [{
    id: "sensor.room", name: "KNX room", group_address: "1/0/1", datapoint_type: "DPT-9.001",
    readable: true, writable: false, widget_types: ["sensor", "gauge", "graph"], live_feedback: true, value: 21.5,
  }, {
    id: "scene/evening", name: "Evening", group_address: "1/0/2", datapoint_type: "DPT-18.001",
    writable: true, widget_types: ["scene_tile"], value_format: "learn,scene_number",
  }];
  globalThis.routing = "disabled";
  globalThis.paths = [];
  apiGet = async path => {
    paths.push(path);
    if (path === "/api/layout") return JSON.parse(saved);
    const offset = Number(new URLSearchParams(path.split("?")[1]).get("offset"));
    return {objects: objects.slice(offset, offset + 1), state: routing, generation: 3,
      has_more: offset === 0, next_offset: offset + 1};
  };
  globalThis.fetch = async (path, options) => {
    if (path !== "/api/layout" || options.method !== "PUT") throw Error(path);
    globalThis.saved = options.body;
    return {ok: true};
  };
  bindKnxWidgetPicker();
}

function chooser() {
  const { run } = loadEditor();
  run("globalThis").Blob = Blob;
  run("(" + setup.toString() + ")()");
  return run;
}

test("KNX selection, fixed value and HA binding survive editor save and reload without HA", async () => {
  const run = chooser();
  await run('knxField("addTileSource").value = "knx"; knxField("addTileSource").onchange()');
  assert.deepEqual([...run("paths")], ["/api/knx/objects?offset=0", "/api/knx/objects?offset=1"]);
  run('knxField("knxTileObject").value = "1"; knxField("knxTileObject").onchange()');
  assert.equal(run('knxField("knxTileApply").disabled'), true);
  run('knxField("knxTileValue").value = "false,3"; knxField("knxTileValue").oninput(); knxField("knxTileApply").onclick()');
  await run("saveLayout()");
  assert.equal(await run("loadLayout()"), true);
  const [ha, scene] = JSON.parse(run("layoutSnapshot()")).pages[0].widgets;
  assert.equal(ha.entity_id, "sensor.room");
  assert.equal(ha.binding, undefined);
  assert.equal(scene.entity_id, "");
  assert.deepEqual(scene.binding, {source: "knx", object_id: "scene/evening", value: "false,3"});
  run("editor.selectedWidgetId = selectedPage().widgets[1].id");
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxTileValue").value'), "false,3");
});

test("feedback preview isolates identical HA and KNX IDs through disable and recovery", async () => {
  const run = chooser();
  await run('startKnxWidgetPicker();');
  run('knxField("knxTileApply").onclick(); editor.states.set("sensor.room", "18")');
  const key = 'knx:sensor:sensor.room';
  assert.equal(run("previewSourceState(" + JSON.stringify(key) + ")"), "unavailable");
  run('routing = "running"');
  await run("refreshKnxWidgetPreview()");
  assert.equal(run("previewSourceState(" + JSON.stringify(key) + ")"), "21.5");
  assert.equal(run('previewSourceState("sensor.room")'), "18");
  run('objects[0].live_feedback = false');
  await run("refreshKnxWidgetPreview()");
  assert.equal(run("previewSourceState(" + JSON.stringify(key) + ")"), "unavailable");
});

test("confirmed KNX text renders HA availability words as literal sensor values", async () => {
  const run = chooser();
  await run("startKnxWidgetPicker()");
  run('knxField("knxTileApply").onclick(); routing = "running"');
  for (const value of ["unknown", "unavailable"]) {
    run('objects[0].value = ' + JSON.stringify(value));
    await run("refreshKnxWidgetPreview()");
    const markup = run("tilePreviewMarkup(selectedPage().widgets[1])");
    assert.ok(markup.includes(value));
    assert.ok(!markup.includes(">--<"));
  }
});

test("closing a KNX picker rejects its late response and a changed registry cannot mix pages", async () => {
  const run = chooser();
  run('apiGet = () => new Promise(resolve => globalThis.complete = resolve); globalThis.pending = startKnxWidgetPicker()');
  run('closeAddTileDialog(); complete({objects, state: "running", generation: 1})');
  await run("pending");
  assert.equal(run("knxPickerObjects.length"), 0);
  run('let generation = 0; apiGet = async () => ({objects: [], has_more: true, generation: ++generation, next_offset: 1})');
  await assert.rejects(run("fetchKnxWidgetObjects()"));
});

test("scaling sensor renders whole percentages and a missing feedback placeholder without rounding the source", () => {
  const run = chooser();
  run(`Object.assign(objects[0], {group_address: "2/0/2", datapoint_type: "DPT-5.001"});
       globalThis.sensor = { ...selectedPage().widgets[0], binding: {source: "knx", object_id: "sensor.room"} };
       globalThis.renderFeedback = () => {
         rememberKnxPreview(objects, "running");
         return tilePreviewMarkup(sensor);
       };`);
  for (const [raw, expected] of [[0, "0%"], [61.960784912109375, "62%"], [100, "100%"]]) {
    run("objects[0].value = " + raw);
    assert.ok(run("renderFeedback()").includes(">" + expected + "</b>"));
    assert.equal(run("objects[0].value"), raw);
    assert.equal(run('previewSourceState("knx:sensor:sensor.room")'), String(raw));
    assert.equal(run('previewSourceState("knx:gauge:sensor.room")'), String(raw));
  }
  run('objects[0].value = 61.960784912109375; sensor.sensor_unit = "hide"');
  assert.match(run("renderFeedback()"), />62<\/b>/);
  run('sensor.sensor_unit = ""; objects[0].live_feedback = false');
  assert.match(run("renderFeedback()"), />--<\/b>/);
  run('objects[0].live_feedback = true; objects[0].value = null');
  assert.match(run("renderFeedback()"), />--<\/b>/);
  run('objects[0].value = 61.75; objects[0].unit = "%"; objects[0].datapoint_type = "DPT-9.007"');
  assert.match(run("renderFeedback()"), />61\.75 %<\/b>/);
  run('editor.states.set("sensor.room", "61.25"); editor.entities = [{id: "sensor.room", unit: "%"}]');
  assert.match(run("tilePreviewMarkup(selectedPage().widgets[0])"), />61\.25 %<\/b>/);
});
