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
      set id(id) { this.nodeId = id; nodes.set(id, this); }, get id() { return this.nodeId; }, setAttribute() {},
      append(...children) { (this.children ??= []).push(...children); },
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
    readable: true, writable: false, widget_roles: ["value_feedback", "number_feedback"], widget_types: ["sensor", "gauge", "graph"], live_feedback: true, value: 21.5,
  }, {
    id: "scene/evening", name: "Evening", group_address: "1/0/2", datapoint_type: "DPT-18.001",
    writable: true, widget_roles: ["action"], widget_types: ["scene_tile"], value_format: "learn,scene_number",
  }];
  globalThis.routing = "disabled";
  globalThis.paths = [];
  apiGet = async path => {
    paths.push(path);
    if (path === "/api/layout") return JSON.parse(saved);
    const offset = Number(new URLSearchParams(path.split("?")[1]).get("offset"));
    return {objects: objects.slice(offset, offset + 1), state: routing, generation: 3,
      has_more: offset + 1 < objects.length, next_offset: offset + 1};
  };
  globalThis.fetch = async (path, options) => {
    if (path !== "/api/layout" || options.method !== "PUT") throw Error(path);
    globalThis.saved = options.body;
    return {ok: true};
  };
  globalThis.choose = (field, id) => {
    const select = knxField(field);
    if (!select.options.some(option => option.value === id && !option.disabled)) throw Error(field + " · " + id);
    select.value = id;
    select.onchange();
  };
  globalThis.addSensor = () => {
    choose("knxTileType", "sensor");
    choose("knxRole_value_feedback", "sensor.room");
    knxField("knxTileApply").onclick();
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
  run('choose("knxTileType", "scene_tile"); choose("knxRole_action", "scene/evening")');
  assert.equal(run('knxField("knxTileApply").disabled'), true);
  run('knxField("knxTileValue").value = "false,3"; knxField("knxTileValue").oninput(); knxField("knxTileApply").onclick()');
  await run("saveLayout()");
  assert.equal(await run("loadLayout()"), true);
  const [ha, scene] = JSON.parse(run("layoutSnapshot()")).pages[0].widgets;
  assert.equal(ha.entity_id, "sensor.room");
  assert.equal(ha.binding, undefined);
  assert.equal(scene.entity_id, "");
  assert.deepEqual(scene.binding, {source: "knx", roles: {action: "scene/evening"}, value: "false,3"});
  run("editor.selectedWidgetId = selectedPage().widgets[1].id");
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxTileValue").value'), "false,3");
});

test("feedback preview isolates identical HA and KNX IDs through disable and recovery", async () => {
  const run = chooser();
  await run('startKnxWidgetPicker();');
  run('addSensor(); editor.states.set("sensor.room", "18")');
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
  run('addSensor(); routing = "running"');
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
  run('sensor.sensor_unit = ""; objects[0].display_precision = 2');
  assert.match(run("renderFeedback()"), />61\.96%<\/b>/);
  run('delete objects[0].display_precision; objects[0].live_feedback = false');
  assert.match(run("renderFeedback()"), />--<\/b>/);
  run('objects[0].live_feedback = true; objects[0].value = null');
  assert.match(run("renderFeedback()"), />--<\/b>/);
  run('objects[0].value = 61.75; objects[0].unit = "%"; objects[0].datapoint_type = "DPT-9.007"');
  assert.match(run("renderFeedback()"), />61\.75 %<\/b>/);
  run('editor.states.set("sensor.room", "61.25"); editor.entities = [{id: "sensor.room", unit: "%"}]');
  assert.match(run("tilePreviewMarkup(selectedPage().widgets[0])"), />61\.25 %<\/b>/);
});

function dimmerRegistry(run) {
  run(`objects = [
    {id: "A", name: "Write brightness", group_address: "2/0/1", datapoint_type: "DPT-5.001", writable: true,
      widget_roles: ["brightness_command", "action"]},
    {id: "B", name: "Actual brightness", group_address: "2/1/1", datapoint_type: "DPT-5.001", readable: true,
      widget_roles: ["brightness_feedback", "value_feedback", "number_feedback"]},
    {id: "AB", name: "Shared", group_address: "2/2/1", datapoint_type: "DPT-5.001", readable: true, writable: true,
      widget_roles: ["brightness_feedback", "brightness_command", "value_feedback", "number_feedback", "action"]},
    {id: "P", name: "Switch", group_address: "2/0/2", datapoint_type: "DPT-1.001", writable: true,
      widget_roles: ["switch_command", "action"]},
    {id: "S", name: "Switch state", group_address: "2/1/2", datapoint_type: "DPT-1.001", readable: true,
      widget_roles: ["switch_feedback", "value_feedback"]},
  ]; routing = "running";`);
}
const selectedRoles = run => JSON.parse(run("JSON.stringify(selectedWidget().binding.roles)"));
const roleOptions = (run, role) => [...run('knxField("knxRole_' + role + '").options')].map(o => o.value);

test("type-first slider assigns independent directions, reloads and uses external feedback", async () => {
  const run = chooser();
  dimmerRegistry(run);
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType", "slider")');
  assert.deepEqual(roleOptions(run, "brightness_command"), ["", "A", "AB"]);
  assert.deepEqual(roleOptions(run, "brightness_feedback"), ["", "B", "AB"]);
  assert.match(run('knxField("knxRole_brightness_command").options[1].textContent'), /Write brightness · A · 2\/0\/1 · DPT-5.001 · W/);
  run('choose("knxRole_brightness_command", "A"); choose("knxRole_brightness_feedback", "B"); knxField("knxTileApply").onclick()');
  const id = run("selectedWidget().id");
  await run("saveLayout();"); await run("loadLayout()");
  run("editor.selectedWidgetId = " + JSON.stringify(id));
  assert.deepEqual(selectedRoles(run), {brightness_command: "A", brightness_feedback: "B"});
  run('objects[0].value = 80; objects[0].live_feedback = true');
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), />--</);
  run('objects[1].value = 61.960784912109375; objects[1].live_feedback = true');
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), />62 %</);
});

test("a shared R/W object can be used twice, then just its feedback can be replaced or cleared", async () => {
  const run = chooser(); dimmerRegistry(run);
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType", "slider"); choose("knxRole_brightness_command", "AB"); choose("knxRole_brightness_feedback", "AB"); knxField("knxTileApply").onclick()');
  const id = run("selectedWidget().id");
  await run("saveLayout()"); await run("loadLayout()"); run("editor.selectedWidgetId = " + JSON.stringify(id));
  assert.deepEqual(selectedRoles(run), {brightness_command: "AB", brightness_feedback: "AB"});
  run('objects[2].value = 27; objects[2].live_feedback = true');
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), />27 %</);
  for (const next of ["B", "", "B"]) {
    await run('knxField("knxInspectorChoose").onclick()');
    run('choose("knxRole_brightness_feedback", ' + JSON.stringify(next) + '); knxField("knxTileApply").onclick()');
    await run("saveLayout()"); await run("loadLayout()"); run("editor.selectedWidgetId = " + JSON.stringify(id));
    assert.equal(selectedRoles(run).brightness_command, "AB");
    assert.equal(selectedRoles(run).brightness_feedback, next || undefined);
  }
  run('objects = objects.filter(o => o.id !== "AB")');
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxRole_brightness_command").value'), "AB");
  run('choose("knxRole_brightness_feedback", ""); knxField("knxTileApply").onclick()');
  assert.deepEqual(selectedRoles(run), {brightness_command: "AB"});
});

test("four-role light projects power and brightness independently and preserves the type without candidates", async () => {
  const run = chooser(); dimmerRegistry(run); await run("startKnxWidgetPicker()");
  run('choose("knxTileType", "light_tile")');
  assert.deepEqual(roleOptions(run, "switch_command"), ["", "P"]);
  assert.deepEqual(roleOptions(run, "switch_feedback"), ["", "S"]);
  run('choose("knxRole_switch_command","P"); choose("knxRole_switch_feedback","S"); choose("knxRole_brightness_command","A"); choose("knxRole_brightness_feedback","B"); knxField("knxTileApply").onclick()');
  run('objects[1].value = 62; objects[1].live_feedback = true');
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /<span>--<\/span>.*62 %/);
  run('objects[4].value = false; objects[4].live_feedback = true');
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /<span>OFF<\/span>.*62 %/i);
  await run('knxField("knxInspectorChoose").onclick()');
  run('choose("knxRole_brightness_command",""); choose("knxRole_brightness_feedback",""); knxField("knxTileApply").onclick()');
  assert.deepEqual(selectedRoles(run), {switch_command:"P", switch_feedback:"S"});
  assert.doesNotMatch(run("tilePreviewMarkup(selectedWidget())"), /tp-bar-knob/);
  run('objects = []'); await run("startKnxWidgetPicker()"); run('choose("knxTileType", "slider")');
  assert.equal(run('knxField("knxTileType").value'), "slider");
  assert.match(run('knxField("knxRole_brightness_command_hint").textContent'), /writable.*DPT 5.001/i);
  assert.match(run('knxField("knxRole_brightness_feedback_hint").textContent'), /readable.*DPT 5.001/i);
  assert.equal(run('knxField("knxTileApply").disabled'), true);
});

test("Inspector retains a legacy fixed action and an offline single-object RGB light", async () => {
  const run = chooser();
  for (const [type, binding] of [["scene_tile",{source:"knx",object_id:"scene/evening",value:"false,3"}],
    ["light_tile",{source:"knx",object_id:"offline-rgb"}]]) {
    run('Object.assign(selectedPage().widgets[0], {entity_id:"", type:' + JSON.stringify(type) + ', binding:' + JSON.stringify(binding) + '}); editor.selectedWidgetId="old"');
    await run('knxField("knxInspectorChoose").onclick()'); run('knxField("knxTileApply").onclick()');
    assert.deepEqual(JSON.parse(run("JSON.stringify(selectedWidget().binding)")), binding);
  }
});

async function saveAndReloadBinding(run) {
  const id = run("selectedWidget().id");
  await run("saveLayout()");
  assert.equal(await run("loadLayout()"), true);
  run("editor.selectedWidgetId = " + JSON.stringify(id));
  return JSON.parse(run("JSON.stringify(selectedWidget().binding)"));
}

test("legacy RGB Light rebinds to another compatible RGB object and survives an offline save", async () => {
  const run = chooser();
  run(`const rgb = {name:"RGB lamp", datapoint_type:"DPT-232.600", readable:true, writable:true,
         widget_roles:["value_feedback","action"], widget_types:["sensor","light_tile","scene_tile"]};
       objects = [
         {...rgb, id:"rgb_a", group_address:"1/0/1"},
         {...rgb, id:"rgb_b", group_address:"1/0/2", live_feedback:true, value:{red:0, green:255, blue:0}},
         {...rgb, id:"read_only", writable:false, widget_types:["sensor"]},
         {...rgb, id:"write_only", readable:false, widget_types:["scene_tile"]},
         {...rgb, id:"dimmer", datapoint_type:"DPT-5.001", widget_roles:["brightness_command","brightness_feedback"]}
       ];
       routing = "running";
       Object.assign(selectedPage().widgets[0], {type:"light_tile", entity_id:"",
         binding:{source:"knx", object_id:"rgb_a"}});
       editor.selectedWidgetId = "old";`);
  await run('knxField("knxInspectorChoose").onclick()');
  assert.deepEqual([...run('knxField("knxTileRgbObject").options || []')].map(o => o.value), ["", "rgb_a", "rgb_b"]);
  assert.equal(run('knxField("knxTileRgbObject").value'), "rgb_a");
  run('choose("knxTileRgbObject", "rgb_b"); knxField("knxTileApply").onclick()');
  const binding = {source: "knx", object_id: "rgb_b"};
  assert.deepEqual(await saveAndReloadBinding(run), binding);
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /100 %<i class="tp-wheel"/);
  run('objects = objects.filter(o => o.id !== "rgb_b")');
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxTileApply").disabled'), false);
  run('knxField("knxTileApply").onclick()');
  assert.deepEqual(await saveAndReloadBinding(run), binding);
  await run('knxField("knxInspectorChoose").onclick()');
  run('choose("knxTileRgbObject", "")');
  assert.equal(run('knxField("knxTileApply").disabled'), true);
  run('choose("knxRole_brightness_command", "dimmer"); knxField("knxTileApply").onclick()');
  assert.deepEqual(await saveAndReloadBinding(run), {source:"knx", roles:{brightness_command:"dimmer"}});
});

test("Scene target changes clear the old preset and preserve valid empty text values", async () => {
  const run = chooser();
  run(`objects.push({id:"relay", name:"Relay", group_address:"1/0/3", datapoint_type:"DPT-1.001",
         writable:true, widget_roles:["switch_command","action"], value_format:"on/off or true/false"});
       for (const family of [16,24,28]) objects.push({id:"text"+family, name:"Text", group_address:"1/1/"+family,
         datapoint_type:"DPT-"+family+".001", writable:true, widget_roles:["action"], value_format:"text"});
       Object.assign(selectedPage().widgets[0], {type:"scene_tile", entity_id:"",
         binding:{source:"knx", object_id:"scene/evening", value:"false,3"}});
       editor.selectedWidgetId = "old";`);
  await run('knxField("knxInspectorChoose").onclick()');
  run('choose("knxRole_action", "scene/evening")');
  assert.equal(run('knxField("knxTileValue").value'), "false,3");
  run('choose("knxRole_action", "relay")');
  assert.equal(run('knxField("knxTileValue").value'), "");
  assert.equal(run('knxField("knxTileApply").disabled'), true);
  run('knxField("knxTileApply").onclick()');
  assert.equal(run("selectedWidget().binding.object_id"), "scene/evening");
  run('knxField("knxTileValue").value = "on"; knxField("knxTileValue").oninput()');
  assert.equal(run('knxField("knxTileApply").disabled'), false);
  run('knxField("knxTileApply").onclick()');
  assert.deepEqual(await saveAndReloadBinding(run), {source:"knx", roles:{action:"relay"}, value:"on"});
  for (const id of ["text16", "text24", "text28"]) {
    await run('knxField("knxInspectorChoose").onclick()');
    run('choose("knxRole_action", ' + JSON.stringify(id) + ')');
    assert.equal(run('knxField("knxTileValue").value'), "");
    assert.equal(run('knxField("knxTileApply").disabled'), false);
    run('knxField("knxTileApply").onclick()');
    assert.deepEqual(await saveAndReloadBinding(run), {source:"knx", roles:{action:id}, value:""});
  }
  run('objects = objects.filter(o => o.id !== "text28")');
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxTileApply").disabled'), false);
  run('choose("knxRole_action", "relay"); objects = []');
  await run('knxField("knxTileReload").onclick()');
  assert.equal(run('knxField("knxTileApply").disabled'), true);
});
