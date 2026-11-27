// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
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
  run('closeAddTileDialog(); complete({objects, state: "running", generation: 1, has_more:false, next_offset:objects.length})');
  await run("pending");
  assert.equal(run("knxPickerObjects.length"), 0);
  run('let generation = 0; apiGet = async () => ({objects: [], has_more: true, generation: ++generation, next_offset: 1})');
  await assert.rejects(run("fetchKnxWidgetObjects()"));
});

test("picker and preview reach the last object of a full 26-page registry", async () => {
  const run=chooser();
  run(`objects=Array.from({length:512},(_,i)=>({...objects[0],id:"point-"+i,group_address:"1/"+(i>>8)+"/"+(i%256),value:i}));
    routing="running";
    apiGet=async path=>{
      paths.push(path);
      const offset=Number(path.split("=")[1]), next=Math.min(offset+20,objects.length);
      return {state:routing,generation:7,routing_epoch:2,objects:objects.slice(offset,next),has_more:next<objects.length,next_offset:next};
    }`);
  await run("startKnxWidgetPicker()");
  assert.equal(run("knxPickerObjects[511].id"),"point-511");
  assert.equal(run("paths.length"),26);assert.equal(run("paths.at(-1)"),"/api/knx/objects?offset=500");
  await run("refreshKnxWidgetPreview()");
  assert.equal(run('knxRegistry.get("point-511").value'),511);
  run("objects.push({...objects[0],id:'overflow'})");
  await assert.rejects(run("fetchKnxWidgetObjects()"));
});

test("a short final registry page cannot publish a partial snapshot; a complete retry works", async () => {
  const run = chooser();
  run(`let short = true;
    apiGet = async path => {
      const offset = Number(path.split("=")[1]);
      return {generation:1, routing_epoch:2, state:"running", has_more:offset === 0,
        next_offset:offset + 1, objects:offset && short ? [] : [objects[offset]]};
    };`);
  await assert.rejects(run("fetchKnxWidgetObjects()"));
  run("short = false");
  assert.equal((await run("fetchKnxWidgetObjects()")).objects.length, 2);
});

test("a reconnect between registry pages discards old commands and feedback, even if both pages say running", async () => {
  const run = chooser();
  run(`const requested = {type:"select_tile", binding:{source:"knx", roles:{preset_command:"P"}}};
    const reported = {type:"select_tile", binding:{source:"knx", roles:{preset_command:"P", preset_feedback:"F"}}};
    selectedPage().widgets.push(requested, reported);
    const cmd = {id:"P", writable:true, widget_roles:["preset_command"]};
    const status = {id:"F", readable:true, widget_roles:["preset_feedback"]};
    const pages = [
      {objects:[{...cmd,last_command:3},{...status,live_feedback:true,value:3}], routing_epoch:5, has_more:true, next_offset:2},
      {objects:[], routing_epoch:6, has_more:false, next_offset:2},
      {objects:[cmd,status], routing_epoch:6, has_more:false, next_offset:2}
    ];
    apiGet = async path => {paths.push(path); return {state:"running", generation:1, ...pages.shift()};};`);
  await run("refreshKnxWidgetPreview()");
  assert.deepEqual([...run("paths")], [0, 2, 0].map(n => "/api/knx/objects?offset=" + n));
  for (const tile of ["requested", "reported"]) {
    assert.match(run(`tilePreviewMarkup(${tile})`), /<b>--<\/b>/);
    assert.match(run(`tilePreviewMarkup(${tile})`), /tp-select-chevron/);
  }
});

test("an older picker response cannot overwrite a fresh zero from the reconnected preview", async () => {
  const run = chooser();
  run(`const preset = {type:"select_tile", rect:{w:320,h:120}, binding:{source:"knx",roles:{preset_command:"P"}}};
    selectedPage().widgets.push(preset);
    const object = {id:"P",writable:true,widget_roles:["preset_command"]};
    apiGet = () => new Promise(resolve => globalThis.finishPicker = resolve);
    const pendingPicker = startKnxWidgetPicker();
    apiGet = async () => ({objects:[{...object,last_command:0}],state:"running",routing_epoch:2,has_more:false,next_offset:1});`);
  await run("refreshKnxWidgetPreview()");
  assert.match(run("tilePreviewMarkup(preset)"), /<b>Auto<\/b>/);
  run('finishPicker({objects:[object],state:"disabled",routing_epoch:1,has_more:false,next_offset:1})');
  await run("pendingPicker");
  assert.match(run("tilePreviewMarkup(preset)"), /<b>Auto<\/b>/);
  assert.equal(run('knxField("knxTileStatus").textContent'), run('t("knx.state.running")'));
  assert.equal(run("knxPickerObjects[0].last_command"), 0);
});

test("aliases on successive registry pages use the latest command and latest readable feedback independently", async () => {
  const run = chooser();
  run(`const shared = {group_address:"2/0/1", datapoint_type:"DPT-5.001", writable:true, readable:true,
      widget_roles:["brightness_command","brightness_feedback"], live_feedback:true};
    objects = [
      {...shared, id:"early", last_command:80, value:80},
      {...shared, id:"both", last_command:0, value:62},
      {...shared, id:"read", writable:false, widget_roles:["brightness_feedback"], value:62},
    ]; routing="running";
    const level = roles => tilePreviewMarkup({type:"slider", binding:{source:"knx",roles}, rect:{w:320,h:120}})
      .match(/<\\/i><span>([^<]+)<\\/span>/)[1];`);
  const refresh = () => run("fetchKnxWidgetObjects().then(page => rememberKnxPreview(page.objects,page.state))");
  await refresh();
  for (const id of ["early", "both"]) assert.equal(run(`level({brightness_command:"${id}"})`), "0 %");
  for (const id of ["early", "both", "read"]) assert.equal(run(`level({brightness_feedback:"${id}"})`), "62 %");
  run("objects[2].live_feedback = false");
  await refresh();
  assert.equal(run('level({brightness_command:"both",brightness_feedback:"early"})'), "--");
  assert.equal(run('level({brightness_command:"early"})'), "0 %");
});

test("a KNX button retains its switch for unknown, false and read-only states", async () => {
  const run = chooser();
  dimmerRegistry(run);
  run(`const power = {type:"button",rect:{w:240,h:180},binding:{source:"knx",roles:{switch_command:"P"}}};
    selectedPage().widgets.push(power);`);
  async function check(label, style) {
    await run("refreshKnxWidgetPreview()");
    const html = run("tilePreviewMarkup(power)");
    assert.match(html, /tp-switch-big/);
    assert.match(html, new RegExp('tp-button-state">' + label + '<', 'i'));
    assert.match(html, new RegExp(style));
  }
  await check("--", "control-action");
  run('objects.find(o=>o.id==="P").last_command=false');
  await check("off", "control-action");
  run('power.binding.roles={switch_feedback:"S"}; Object.assign(objects.find(o=>o.id==="S"),{value:true,live_feedback:true})');
  await check("on", "control-disabled");
  run('routing="disabled"');
  await check("Unavailable", "control-disabled");
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

test("command-only and feedback sliders agree with the KNX worker after save and reload",
  {skip: !process.env.BETTA_KNX_SERVICE_TEST}, async t => {
    const run = chooser();
    dimmerRegistry(run);
    const dir = mkdtempSync(join(tmpdir(), "betta-knx-slider-"));
    t.after(() => rmSync(dir, {recursive: true, force: true}));
    await run("startKnxWidgetPicker()");
    run('choose("knxTileType","slider"); choose("knxRole_brightness_command","A"); knxField("knxTileApply").onclick()');
    assert.deepEqual((await saveAndReloadBinding(run)).roles, {brightness_command:"A"});
    const slider = JSON.parse(run("JSON.stringify(selectedWidget())"));
    const commandFile = join(dir,"command.json"), feedbackFile = join(dir,"feedback.json"), traceFile = join(dir,"trace.json");
    writeFileSync(commandFile,run("saved"));
    await run('knxField("knxInspectorChoose").onclick()');
    run('choose("knxRole_brightness_feedback","B"); knxField("knxTileApply").onclick()');
    assert.deepEqual((await saveAndReloadBinding(run)).roles, {brightness_command:"A",brightness_feedback:"B"});
    writeFileSync(feedbackFile,run("saved"));
    execFileSync(process.env.BETTA_KNX_SERVICE_TEST, [commandFile,feedbackFile,traceFile]);
    const trace = JSON.parse(readFileSync(traceFile,"utf8"));
    const frames = [];
    const restoredSlider = run("JSON.stringify(selectedWidget())");
    for (const [name,requested,confirmed] of [
      ["queued","--","--"], ["eighty","80 %","--"], ["zero","0 %","--"], ["failed","0 %","--"],
      ["feedback","80 %","62 %"], ["peer","20 %","62 %"], ["queue_full","20 %","62 %"], ["replaced","--","--"],
    ]) {
      const page = trace[name];
      run(`rememberKnxPreview(${JSON.stringify(page.objects)},${JSON.stringify(page.state)})`);
      const labels = [JSON.stringify(slider), restoredSlider].map(widget =>
        run(`tilePreviewMarkup(${widget})`).match(/<\/i><span>([^<]+)<\/span>/)?.[1]);
      assert.deepEqual(labels, [requested, confirmed], name);
      frames.push([page.panel_command_state, labels[0], page.panel_feedback_state, labels[1]]);
    }
    if (process.env.BETTA_KNX_SIMULATOR) {
      const output = execFileSync(process.env.BETTA_KNX_SIMULATOR, ["--regression", "controls"], {
        cwd: new URL("../../", import.meta.url), encoding: "utf8",
        env: {...process.env, SDL_VIDEODRIVER: "dummy", BETTA_SIM_NOW: "1792233000",
          BETTA_KNX_PANEL_STATES: JSON.stringify(frames)},
      });
      assert.match(output, /16 native slider labels matched the editor/);
    }
  });

test("nullable feedback, shared command values, angle and HVAC preset round trips", async () => {
  const run = chooser();
  dimmerRegistry(run);
  run(`objects.push({id:"angle",widget_roles:["angle_command"],writable:true,datapoint_type:"DPT-5.003"},
    {id:"preset",widget_roles:["preset_command"],writable:true,datapoint_type:"DPT-20.102"})`);
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType","slider"); choose("knxRole_angle_command","angle"); knxField("knxTileApply").onclick()');
  assert.deepEqual((await saveAndReloadBinding(run)).roles,{angle_command:"angle"});
  for (const feedback of [null, "", undefined]) {
    run(`selectedWidget().binding.roles.angle_feedback=${JSON.stringify(feedback)};
      objects.find(o=>o.id==="angle").last_command=270; rememberKnxPreview(objects,"running")`);
    const markup=run("tilePreviewMarkup(selectedWidget())");
    assert.match(markup,/>270°</);
    assert.match(markup,/75%/);
  }
  run('selectedWidget().binding.roles.angle_feedback="removed"');
  assert.match(run("tilePreviewMarkup(selectedWidget())"),/>--</);
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType","select_tile"); choose("knxRole_preset_command","preset"); knxField("knxTileApply").onclick()');
  assert.deepEqual((await saveAndReloadBinding(run)).roles,{preset_command:"preset"});
  for(const [code,label] of [[0,"Auto"],[3,"Economy"],[4,"Building Protection"]]) {
    run(`objects.find(o=>o.id==="preset").last_command=${code}; rememberKnxPreview(objects,"running")`);
    assert.ok(run("tilePreviewMarkup(selectedWidget())").includes(">"+label+"<"));
  }
  run('rememberKnxPreview(objects,"waiting_for_network")');
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /<b>Unavailable<\/b>/);
});

test("KNX presets distinguish offline, rejoined and fresh zero for each binding", async () => {
  const run = chooser();
  run(`const command = {id:"P", writable:true, widget_roles:["preset_command"], last_command:3};
       const feedback = {id:"F", readable:true, widget_roles:["preset_feedback"], live_feedback:true, value:3};
       objects = [command, feedback];
       const requested = {id:"requested", type:"select_tile", rect:{x:0,y:0,w:320,h:120},
         binding:{source:"knx", roles:{preset_command:"P"}}};
       const reported = {...requested, id:"reported", binding:{source:"knx", roles:{preset_command:"P", preset_feedback:"F"}}};
       selectedPage().widgets.push(requested, reported);
       editor.i18nMap = WEB_I18N_BUILTIN["zh-cn"];
       routing = "running"`);
  async function expectPresets(requestedLabel, reportedLabel, writable) {
    await run("refreshKnxWidgetPreview()");
    for (const [tile, label] of [["requested", requestedLabel], ["reported", reportedLabel]]) {
      const html = run(`tilePreviewMarkup(${tile})`);
      assert.equal(html.match(/<b>([^<]*)<\/b>/)?.[1], label, tile);
      assert.equal(html.includes("tp-select-chevron"), writable, tile);
    }
  }
  await expectPresets("节能", "节能", true);
  run('routing = "disabled"');
  await expectPresets("不可用", "不可用", false);
  run('routing = "running"; delete command.last_command; feedback.live_feedback = false');
  await expectPresets("--", "--", true);
  run("command.last_command = 0");
  await expectPresets("自动", "--", true);
  run("feedback.value = 0; feedback.live_feedback = true");
  await expectPresets("自动", "自动", true);
});

test("Home Assistant select previews keep literal option names and their existing placeholder", () => {
  const run = chooser();
  run('editor.i18nMap = WEB_I18N_BUILTIN["zh-cn"]');
  for (const id of ["select.preset", "input_select.preset"]) {
    for (const [state, expected] of [["Economy", "Economy"], ["0", "0"], ["unavailable", "--"], ["unknown", "--"]]) {
      run(`editor.states.set("${id}", "${state}")`);
      const html = run(`tilePreviewMarkup({type:"select_tile", entity_id:"${id}", rect:{x:0,y:0,w:320,h:120}})`);
      assert.equal(html.match(/<b>([^<]*)<\/b>/)?.[1], expected, id);
    }
  }
});

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

function controlRegistry(run) {
  run(`objects = [
    {id:"move", datapoint_type:"DPT-1.008", writable:true, widget_roles:["move_command"]},
    {id:"stop", datapoint_type:"DPT-1.010", writable:true, widget_roles:["stop_command"]},
    {id:"position_cmd", datapoint_type:"DPT-5.001", writable:true, widget_roles:["position_command"]},
    {id:"position", datapoint_type:"DPT-5.001", readable:true, widget_roles:["position_feedback"]},
    {id:"position_alt", datapoint_type:"DPT-5.001", readable:true, widget_roles:["position_feedback"]},
    {id:"target_cmd", datapoint_type:"DPT-9.001", writable:true, widget_roles:["target_temperature_command"]},
    {id:"target", datapoint_type:"DPT-9.001", readable:true, widget_roles:["target_temperature_feedback"]},
    {id:"current", datapoint_type:"DPT-9.001", readable:true, widget_roles:["current_temperature_feedback"]},
    {id:"mode_cmd", datapoint_type:"DPT-20.105", writable:true, widget_roles:["hvac_mode_command"]},
    {id:"mode", datapoint_type:"DPT-20.105", readable:true, widget_roles:["hvac_mode_feedback"]},
    {id:"speed_cmd", datapoint_type:"DPT-5.001", writable:true, widget_roles:["fan_speed_command"]},
    {id:"speed", datapoint_type:"DPT-5.001", readable:true, widget_roles:["fan_speed_feedback"]},
    {id:"power_cmd", datapoint_type:"DPT-1.001", writable:true, widget_roles:["switch_command"]},
    {id:"power", datapoint_type:"DPT-1.001", readable:true, widget_roles:["switch_feedback"]},
    {id:"step", datapoint_type:"DPT-1.007", writable:true, widget_roles:["action"]},
    {id:"preset", datapoint_type:"DPT-20.102", writable:true, widget_roles:["action"]},
    {id:"delta", datapoint_type:"DPT-9.002", writable:true, widget_roles:["action"]},
    {id:"literal_percent", datapoint_type:"DPT-5.004", readable:true, writable:true, widget_roles:["number_feedback","action"]},
  ].map((o, i) => ({name:o.id, group_address:"3/0/" + (i + 1), ...o})); routing = "running";
  globalThis.feedback = (id, value) => Object.assign(objects.find(o => o.id === id), {live_feedback:true, value});
  globalThis.view = () => knxPreview.get(knxWidgetPreviewKey(selectedWidget()));`);
}

async function addControl(run, type, roles) {
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType", ' + JSON.stringify(type) + ')');
  for (const [role, id] of Object.entries(roles)) run('choose(' + JSON.stringify("knxRole_" + role) + ', ' + JSON.stringify(id) + ')');
  run('knxField("knxTileApply").onclick()');
}

test("KNX cover direction and independent roles survive creation, replacement, clearing and reload", async () => {
  const run = chooser(); controlRegistry(run);
  await run("startKnxWidgetPicker()");
  run('choose("knxTileType", "cover_tile")');
  assert.deepEqual(roleOptions(run, "move_command"), ["", "move"]);
  assert.deepEqual(roleOptions(run, "stop_command"), ["", "stop"]);
  assert.deepEqual(roleOptions(run, "position_command"), ["", "position_cmd"]);
  assert.deepEqual(roleOptions(run, "position_feedback"), ["", "position", "position_alt"]);
  assert.match(run('knxField("knxRole_stop_command_hint").textContent'), /1\.007/);
  run('choose("knxRole_move_command", "move"); choose("knxRole_stop_command", "stop"); choose("knxRole_position_feedback", "position")');
  run('knxField("knxPositionOpenAt100").checked = true; knxField("knxPositionOpenAt100").onchange(); knxField("knxTileApply").onclick()');
  const original = {source:"knx", roles:{move_command:"move", stop_command:"stop", position_feedback:"position"}, position_open_at_100:true};
  assert.deepEqual(await saveAndReloadBinding(run), original);
  for (const next of ["position_alt", "", "position"]) {
    await run('knxField("knxInspectorChoose").onclick()');
    assert.equal(run('knxField("knxPositionOpenAt100").checked'), true);
    run('choose("knxRole_position_feedback", ' + JSON.stringify(next) + '); knxField("knxTileApply").onclick()');
    const binding = await saveAndReloadBinding(run);
    assert.equal(binding.roles.move_command, "move");
    assert.equal(binding.roles.stop_command, "stop");
    assert.equal(binding.roles.position_feedback, next || undefined);
    assert.equal(binding.position_open_at_100, true);
  }
  await run('knxField("knxInspectorChoose").onclick()');
  run('knxField("knxPositionOpenAt100").checked = false; knxField("knxPositionOpenAt100").onchange(); knxField("knxTileApply").onclick()');
  assert.equal((await saveAndReloadBinding(run)).position_open_at_100, false);
});

test("cover preview converts real feedback after inversion, never a command echo", async () => {
  const run = chooser(); controlRegistry(run);
  await addControl(run, "cover_tile", {position_command:"position_cmd", position_feedback:"position"});
  run('feedback("position_cmd", 70); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().current_position"), null);
  assert.equal(run("view().canMove"), true);
  assert.equal(run("view().canStop"), false);
  assert.doesNotMatch(run("tilePreviewMarkup(selectedWidget())"), /70|■/);
  for (const inverted of [false, true]) {
    run('selectedWidget().binding.position_open_at_100 = ' + inverted);
    for (const raw of [0, 0.5, 50, 61.9607849, 100]) {
      run('feedback("position", ' + raw + '); rememberKnxPreview(objects, "running")');
      const expected = Math.round(inverted ? raw : 100 - raw);
      assert.equal(run("view().current_position"), expected);
      assert.match(run("tilePreviewMarkup(selectedWidget())"), new RegExp("<small>" + expected + " %</small>"));
    }
  }
  run('rememberKnxPreview(objects, "disabled")');
  assert.equal(run("view().state"), "unavailable");
  assert.equal(run("view().current_position"), null);
  assert.equal(run("view().canMove"), false);
});

test("climate role selectors keep target, measured temperature and control mode distinct through reload", async () => {
  const run = chooser(); controlRegistry(run);
  await run("startKnxWidgetPicker()"); run('choose("knxTileType", "climate_tile")');
  assert.deepEqual(roleOptions(run, "target_temperature_command"), ["", "target_cmd"]);
  assert.deepEqual(roleOptions(run, "target_temperature_feedback"), ["", "target"]);
  assert.deepEqual(roleOptions(run, "current_temperature_feedback"), ["", "current"]);
  assert.deepEqual(roleOptions(run, "hvac_mode_command"), ["", "mode_cmd"]);
  assert.deepEqual(roleOptions(run, "hvac_mode_feedback"), ["", "mode"]);
  const roles = {target_temperature_command:"target_cmd", target_temperature_feedback:"target", current_temperature_feedback:"current", hvac_mode_command:"mode_cmd", hvac_mode_feedback:"mode"};
  for (const [role, id] of Object.entries(roles)) run('choose(' + JSON.stringify("knxRole_" + role) + ', ' + JSON.stringify(id) + ')');
  run('knxField("knxTileApply").onclick()');
  assert.deepEqual(await saveAndReloadBinding(run), {source:"knx", roles});
  run('feedback("target_cmd", 30); feedback("mode_cmd", 1); feedback("current", 21.5); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().temperature"), null);
  assert.equal(run("view().current_temperature"), 21.5);
  assert.equal(run("view().canTemperature"), false);
  assert.equal(run("view().canMode"), true);
  assert.equal(run("view().state"), "unknown");
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /21\.5°C/);
  assert.doesNotMatch(run("tilePreviewMarkup(selectedWidget())"), /30\.0|Fan:|Swing:/);
  run('feedback("target", 23); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().temperature"), 23);
  assert.equal(run("view().canTemperature"), true);
  for (const [code, mode] of [[0,"auto"],[1,"heat"],[3,"cool"],[6,"off"],[9,"fan_only"],[14,"dry"],[2,"unknown"]]) {
    run('feedback("mode", ' + code + '); rememberKnxPreview(objects, "running")');
    assert.equal(run("view().state"), mode);
  }
  await run('knxField("knxInspectorChoose").onclick()');
  run('choose("knxRole_hvac_mode_command", ""); knxField("knxTileApply").onclick()');
  const updated = await saveAndReloadBinding(run);
  assert.deepEqual(updated, {source:"knx", roles:Object.fromEntries(Object.entries(roles).filter(([role]) => role !== "hvac_mode_command"))});
  assert.equal(run("view().canMode"), false);
  assert.equal(run("view().current_temperature"), 21.5);
});

test("fan percentage controls use confirmed speed independently from switch power", async () => {
  const run = chooser(); controlRegistry(run);
  await addControl(run, "fan_tile", {switch_command:"power_cmd", switch_feedback:"power", fan_speed_command:"speed_cmd", fan_speed_feedback:"speed"});
  const saved = await saveAndReloadBinding(run);
  assert.equal(saved.position_open_at_100, undefined);
  assert.equal(saved.roles.fan_speed_command, "speed_cmd");
  run('selectedWidget().rect.h = 260; feedback("speed_cmd", 100); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().state"), "unknown");
  assert.equal(run("view().percentage"), null);
  assert.equal(run("view().canSpeed"), true);
  assert.equal(run("view().canStep"), false);
  assert.equal(run("view().canPower"), false);
  let markup = run("tilePreviewMarkup(selectedWidget())");
  assert.match(markup, /control-action/);
  assert.doesNotMatch(markup, /60|100 %|Auto|Sleep|Breeze/);
  run('feedback("speed", 62); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().percentage"), 62);
  assert.equal(run("view().state"), "unknown");
  assert.equal(run("view().canStep"), true);
  assert.equal(run("view().canPower"), false);
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /<small>62 %<\/small>/);
  assert.doesNotMatch(run("tilePreviewMarkup(selectedWidget())"), /<i class="control-active">/);
  run('feedback("speed", 50); rememberKnxPreview(objects, "running")');
  markup = run("tilePreviewMarkup(selectedWidget())");
  assert.deepEqual([...markup.matchAll(/<i class="control-active">([^<]+)<\/i>/g)].map(match => match[1]), ["50%"]);
  run('selectedWidget().rect.w = 240');
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /--knx-speed-columns:2/);
  run('selectedWidget().rect.w = 500');
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /--knx-speed-columns:4/);
  run('feedback("speed", 62); rememberKnxPreview(objects, "running")');
  run('feedback("power", false); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().state"), "off");
  assert.equal(run("view().canPower"), true);
  assert.match(run("tilePreviewMarkup(selectedWidget())"), /OFF · 62 %/i);
  run('objects.find(o => o.id === "speed").live_feedback = false; rememberKnxPreview(objects, "running")');
  assert.equal(run("view().percentage"), null);
  assert.equal(run("view().canStep"), false);
  run('rememberKnxPreview(objects, "waiting_for_network")');
  for (const capability of ["canSpeed", "canStep", "canPower"]) assert.equal(run("view()." + capability), false);
});

test("composite KNX previews isolate shared IDs and preserve missing bindings for recovery", async () => {
  const run = chooser(); controlRegistry(run);
  await addControl(run, "cover_tile", {move_command:"move", position_feedback:"position"});
  run('feedback("position", 25); feedback("position_alt", 80); rememberKnxPreview(objects, "running")');
  const normalKey = run("knxWidgetPreviewKey(selectedWidget())");
  run('globalThis.other = JSON.parse(JSON.stringify(selectedWidget())); other.binding.position_open_at_100 = true');
  const reversedKey = run("knxWidgetPreviewKey(other)");
  assert.notEqual(normalKey, reversedKey);
  assert.equal(run("knxPreview.get(" + JSON.stringify(normalKey) + ").current_position"), 75);
  assert.equal(run("knxPreview.get(" + JSON.stringify(reversedKey) + ").current_position"), 25);
  run('objects = objects.filter(o => o.id !== "move")');
  await run('knxField("knxInspectorChoose").onclick()');
  assert.equal(run('knxField("knxRole_move_command").value'), "move");
  run('choose("knxRole_position_feedback", "position_alt"); knxField("knxTileApply").onclick()');
  assert.equal((await saveAndReloadBinding(run)).roles.move_command, "move");
  assert.equal(run("view().canMove"), false);
  assert.equal(run("view().current_position"), 20);
  run('objects.push({id:"move", name:"Restored", group_address:"4/0/1", datapoint_type:"DPT-1.009", writable:true, widget_roles:["move_command"]}); rememberKnxPreview(objects, "running")');
  assert.equal(run("view().canMove"), true);
});

test("new KNX type choices keep requirement hints without eligible registry objects", async () => {
  const run = chooser(); controlRegistry(run); run("objects = []");
  await run("startKnxWidgetPicker()");
  for (const [type, role, dpt] of [["cover_tile","move_command","1.008"],["climate_tile","hvac_mode_command","20.105"],["fan_tile","fan_speed_feedback","5.001"]]) {
    run('choose("knxTileType", ' + JSON.stringify(type) + ')');
    assert.equal(run('knxField("knxTileApply").disabled'), true);
    assert.ok(run('knxField(' + JSON.stringify("knxRole_" + role + "_hint") + ').textContent').includes(dpt));
  }
});
