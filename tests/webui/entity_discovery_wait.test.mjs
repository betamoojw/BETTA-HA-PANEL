// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

const pending = { status: "pending", connected: true, pending: true, loaded: 0, items: [], wait_reason: "internal_memory" };
const ready = { status: "ready", connected: true, pending: false, loaded: 0, items: [] };
const drain = async () => { for (let i = 0; i < 30; ++i) await Promise.resolve(); };

function setup(responses = [pending]) {
  const editor = loadEditor();
  editor.run(`
    globalThis.paths = []; globalThis.cancels = []; globalThis.polls = [];
    globalThis.responses = ${JSON.stringify(responses)};
    globalThis.makeElement = () => ({textContent:'', value:'button', hidden:true,
      classList:{add(){}, remove(){}, toggle(){}}, appendChild(){}, addEventListener(){}});
    el.addTileSearch=makeElement(); el.addTileSearchStatus=makeElement(); el.addTileResults=makeElement();
    el.addTileSearchRetry=makeElement(); el.addTileOverlay=makeElement();
    el.addTileSearchRetry.classList={add(){el.addTileSearchRetry.hidden=true;},remove(){el.addTileSearchRetry.hidden=false;}};
    window.setTimeout=(callback,delay)=>{polls.push(delay);callback();return 1;}; window.clearTimeout=()=>{};
    fetch=async(path,options)=>{cancels.push({path,method:options.method});return {};};
    apiGet=async(path)=>{paths.push(path);return responses.length>1 ? responses.shift() : responses[0];};
  `);
  return editor;
}

test("low memory is explained and recovery finishes the same entity query", async () => {
  const { fn, run } = setup([pending, ready]);
  assert.match(fn("entityDiscoveryStatus")(pending), /memory/i);
  await fn("runAddTileSearch")();
  assert.equal(run("paths.length"), 2);
  assert.doesNotMatch(run("el.addTileSearchStatus.textContent"), /Waiting|timed out/);
  assert.equal(run("el.addTileSearchRetry.hidden"), true);
});

test("server timeout and error show a reason and Retry without more polling", async () => {
  for (const status of ["timeout", "error"]) {
    const { fn, run } = setup([{ ...pending, status, pending: false }]);
    await fn("runAddTileSearch")();
    assert.equal(run("paths.length"), 1);
    assert.match(run("el.addTileSearchStatus.textContent"), /memory.*Retry/);
    assert.equal(run("el.addTileSearchRetry.hidden"), false);
  }
});

test("legacy firmware pending response reaches a visible timeout and cancels work", async () => {
  const { fn, run } = setup([{ ...pending, wait_reason: undefined }]);
  await fn("runAddTileSearch")();
  await run("entityDiscoveryCancellation");
  assert.equal(run("paths.length"), 91);
  assert.match(run("el.addTileSearchStatus.textContent"), /timed out.*Retry/);
  assert.equal(run("el.addTileSearchRetry.hidden"), false);
  assert.equal(run("cancels[0].method"), "DELETE");
  assert.match(run("cancels[0].path"), /domain=any&search=button/);
});

test("progress extends the stalled-poll window but cannot run forever", async () => {
  const { fn, run } = setup();
  run(`apiGet=async(path)=>{paths.push(path);return {...responses[0],loaded:paths.length};};`);
  await fn("runAddTileSearch")();
  await run("entityDiscoveryCancellation");
  assert.equal(run("paths.length"), 271);
  assert.match(run("el.addTileSearchStatus.textContent"), /timed out/);
});

test("Retry button refreshes only the first poll and preserves the search and layout", async () => {
  const { fn, run } = setup([{ ...pending, status: "timeout", pending: false }]);
  run("editor.layout={pages:[{id:'kept',visible:false,widgets:[{id:'graph',type:'graph',entity_id:'sensor.power'}]}]}; globalThis.savedLayout=JSON.stringify(editor.layout);");
  fn("bindAddTileDialog")();
  await fn("runAddTileSearch")();
  run(`responses=${JSON.stringify([pending, ready])}; el.addTileSearchRetry.onclick();`);
  // The actual click handler starts an async operation; drain its microtasks.
  for (let i = 0; i < 20; i++) await Promise.resolve();
  assert.match(run("paths[1]"), /search=button&refresh=1/);
  assert.doesNotMatch(run("paths[2]"), /refresh=/);
  assert.equal(run("JSON.stringify(editor.layout) === savedLayout"), true);
  assert.equal(run("el.addTileSearch.value"), "button");
});

test("closing a search cancels it and ignores a late successful result", async () => {
  const { fn, run } = setup();
  run("apiGet=()=>new Promise(resolve=>globalThis.complete=resolve);");
  const work = fn("runAddTileSearch")();
  await drain();
  fn("closeAddTileDialog")();
  run(`el.addTileSearchStatus.textContent='closed';complete(${JSON.stringify(ready)});`);
  await work;
  await run("entityDiscoveryCancellation");
  assert.equal(run("el.addTileSearchStatus.textContent"), "closed");
  assert.equal(run("cancels.length"), 1);
});

test("typing invalidates in-flight results immediately before the debounce fires", async () => {
  const { fn, run } = setup();
  fn("bindAddTileDialog")();
  run("apiGet=()=>new Promise(resolve=>globalThis.complete=resolve); window.setTimeout=()=>1;");
  const work = fn("runAddTileSearch")();
  await drain();
  run(`el.addTileSearch.value='new';el.addTileSearch.oninput();el.addTileSearchStatus.textContent='new search';complete(${JSON.stringify(ready)});`);
  await work;
  await run("entityDiscoveryCancellation");
  assert.equal(run("el.addTileSearchStatus.textContent"), "new search");
  assert.match(run("cancels[0].path"), /search=button/);
});

test("retry waits for cancellation completion before starting the replacement query", async () => {
  const { fn, run } = setup([ready]);
  run("fetch=()=>new Promise(resolve=>globalThis.finishCancel=resolve);");
  fn("cancelEntityDiscovery")("any", "button");
  await Promise.resolve();
  const work = fn("runAddTileSearch")({ refresh: true });
  await Promise.resolve();
  assert.equal(run("paths.length"), 0);
  run("finishCancel({});");
  await work;
  assert.equal(run("paths.length"), 1);
  assert.match(run("paths[0]"), /refresh=1/);
});

test("transport failure and disconnected HA offer retry instead of empty results", async () => {
  const { fn, run } = setup([{ ...ready, status: "disconnected", connected: false }]);
  await fn("runAddTileSearch")();
  assert.match(run("el.addTileSearchStatus.textContent"), /not connected/);
  run("apiGet=async()=>{throw new Error('network');};");
  await fn("runAddTileSearch")();
  await run("entityDiscoveryCancellation");
  assert.match(run("el.addTileSearchStatus.textContent"), /stopped.*Retry/);
  assert.equal(run("el.addTileSearchRetry.hidden"), false);
});

test("the field picker stops polling on timeout and keeps previous items", async () => {
  const { fn, run } = setup([{ ...pending, status: "timeout", pending: false, items: [{ id: "light.kept" }] }]);
  run(`editor.lightPicker.widgetType='light_tile';editor.lightPicker.search='';
    renderLightEntityPicker=data=>{globalThis.lastRendered=data;};`);
  await fn("fetchLightEntityPicker")();
  assert.equal(run("editor.lightPicker.loading"), false);
  assert.equal(run("lastRendered.items[0].id"), "light.kept");
  assert.equal(run("polls.length"), 0);
  assert.equal(run("lastRendered.status"), "timeout");
});

test("the field picker also cancels legacy endless pending and permits explicit retry", async () => {
  const { fn, run } = setup();
  run(`editor.lightPicker.widgetType='light_tile';editor.lightPicker.search='';renderLightEntityPicker=data=>{globalThis.lastRendered=data;};`);
  await fn("fetchLightEntityPicker")({ pollCount: 91, stalled: 90, loaded: 0 });
  await run("entityDiscoveryCancellation");
  assert.equal(run("lastRendered.status"), "timeout");
  assert.equal(run("editor.lightPicker.loading"), false);
  assert.equal(run("polls.length"), 0);
  run(`responses=[${JSON.stringify(ready)}];`);
  await fn("fetchLightEntityPicker")({ refresh: true });
  assert.match(run("paths[1]"), /refresh=1/);
  assert.equal(run("lastRendered.status"), "ready");
});

test("all seven languages provide memory, timeout, permission and retry explanations", () => {
  const { fn, run } = setup();
  for (const language of ["en", "de", "es", "fr", "pl", "zh-cn", "zh-tw"]) {
    run(`editor.i18nMap=WEB_I18N_BUILTIN[${JSON.stringify(language)}];`);
    for (const reason of ["internal_memory", "external_memory", "memory", "busy", "permission", "response", "http_retry", "replaced"]) {
      const text = fn("entityDiscoveryStatus")({ status: "timeout", wait_reason: reason });
      assert.ok(text.includes(fn("t")("entity_picker.retry")));
      assert.doesNotMatch(text, /entity_picker\.|\{reason\}/);
    }
  }
});

test("a hung entity HTTP fetch is aborted and exposes Retry", async () => {
  const { fn, run } = setup();
  run(`globalThis.expirations=[];setTimeout=callback=>{expirations.push(callback);return 1;};clearTimeout=()=>{};
    apiGet=(path,signal)=>new Promise((resolve,reject)=>signal.addEventListener('abort',()=>reject(new Error('aborted'))));`);
  const work = fn("runAddTileSearch")();
  await drain();
  run("expirations.shift()();");
  await work;
  await run("entityDiscoveryCancellation");
  assert.match(run("el.addTileSearchStatus.textContent"), /stopped.*Retry/);
  assert.equal(run("el.addTileSearchRetry.hidden"), false);
});

test("a lost cancellation response is bounded and releases the retry queue", async () => {
  const { fn, run } = setup([ready]);
  run(`globalThis.expirations=[];setTimeout=callback=>{expirations.push(callback);return 1;};clearTimeout=()=>{};
    fetch=(path,options)=>new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(new Error('aborted'))));`);
  fn("cancelEntityDiscovery")("any", "button");
  await Promise.resolve();
  const work = fn("runAddTileSearch")({ refresh: true });
  run("expirations.shift()();");
  await work;
  assert.equal(run("paths.length"), 1);
  assert.match(run("paths[0]"), /refresh=1/);
});

test("elapsed time bounds slow responses even before the poll count is exhausted", async () => {
  const { fn, run } = setup();
  run(`globalThis.clock=0;performance={now:()=>clock};
    apiGet=async(path)=>{paths.push(path);if(paths.length>1)clock=50000;return responses[0];};`);
  await fn("runAddTileSearch")();
  await run("entityDiscoveryCancellation");
  assert.equal(run("paths.length"), 2);
  assert.match(run("el.addTileSearchStatus.textContent"), /timed out/);
  run(`editor.lightPicker.widgetType='light_tile';editor.lightPicker.search='';renderLightEntityPicker=data=>{globalThis.lastRendered=data;};`);
  await fn("fetchLightEntityPicker")({ started: 0, progress: 0, loaded: 0 });
  await run("entityDiscoveryCancellation");
  assert.equal(run("lastRendered.status"), "timeout");
});

test("cached items cannot hide progress and elapsed total time still bounds a refresh", async () => {
  const { fn, run } = setup();
  run(`globalThis.clock=0;performance={now:()=>clock};
    editor.lightPicker.widgetType='light_tile';editor.lightPicker.search='';
    window.setTimeout=callback=>{globalThis.nextPoll=callback;return 1;};
    renderLightEntityPicker=data=>{globalThis.lastRendered=data;};
    apiGet=async()=>({...responses[0],status:'refreshing',count:640,loaded:12,items:[{id:'light.kept'}]});`);
  await fn("fetchLightEntityPicker")({ started: 0, progress: 0, loaded: 0, stalled: 89 });
  assert.equal(run("lastRendered.pending"), true);
  assert.equal(run("typeof nextPoll"), "function");
  run(`clock=180001;apiGet=async()=>({...responses[0],status:'refreshing',count:640,loaded:24,items:[{id:'light.kept'}]});`);
  await fn("fetchLightEntityPicker")({ started: 0, progress: 180000, loaded: 12 });
  await run("entityDiscoveryCancellation");
  assert.equal(run("lastRendered.status"), "timeout");
  assert.equal(run("lastRendered.items[0].id"), "light.kept");
});
