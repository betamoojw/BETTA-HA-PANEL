// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

const drain = async () => { for (let i = 0; i < 40; ++i) await Promise.resolve(); };

function setup() {
  const editor = loadEditor();
  editor.run(`
    globalThis.calls=[];globalThis.jobs=new Map();globalThis.nextId=0;globalThis.events={};
    globalThis.element=()=>({value:'',textContent:'',classList:{add(){},remove(){},toggle(){}},
      appendChild(){},addEventListener(){},focus(){},select(){}});
    for(const key of ['addTileSearch','addTileSearchStatus','addTileResults','addTileSearchRetry',
      'addTileOverlay','lightEntityPickerOverlay','lightEntityPickerSearch','lightEntityPickerRefreshBtn']) el[key]=element();
    window.addEventListener=(name,handler)=>{events[name]=handler;};
    window.setTimeout=(callback,delay)=>{const id=++nextId;jobs.set(id,{callback,delay});return id;};
    window.clearTimeout=id=>jobs.delete(id);
    globalThis.fire=delay=>{
      const entry=[...jobs].find(([,job])=>job.delay===delay);
      if(!entry)throw new Error('No timer: '+delay);
      jobs.delete(entry[0]);return entry[1].callback();
    };
    globalThis.reply={status:'pending',pending:true,loaded:0,items:[],wait_reason:'internal_memory',generation:41};
    apiGet=async path=>{calls.push({method:'GET',path});return {...reply};};
    fetch=async(path,options)=>{calls.push({method:options.method,path});return {};};
    renderLightEntityPicker=data=>{globalThis.rendered=data;};
    refreshInspectorEntityRows=()=>{};
    editor.layout={pages:[{id:'kept',widgets:[{id:'g',type:'graph',entity_id:'sensor.power'}]}]};
    editor.selectedPageId='kept';globalThis.savedLayout=JSON.stringify(editor.layout);
  `);
  editor.fn("bindAddTileDialog")();
  editor.fn("bindLightEntityPicker")();
  return editor;
}

async function close(editor) {
  editor.fn("closeEntitySearchDialogs")();
  await drain();
  // Resume the sleeping add-widget loop; it must observe its invalidated sequence.
  editor.run("for(const [id,job] of [...jobs]){jobs.delete(id);job.callback();}");
  await drain();
  assert.equal(editor.run("JSON.stringify(editor.layout)===savedLayout"), true);
}

test("button to light during poll sleep never restarts the old query", async () => {
  const e = setup();
  e.run("el.addTileSearch.value='button';");
  const old = e.fn("runAddTileSearch")(); await drain();
  e.run("el.addTileSearch.value='light';el.addTileSearch.oninput();"); await drain();
  e.fn("fire")(350); await drain();
  const count = e.run("calls.length");
  e.fn("fire")(700); await old;
  assert.equal(e.run("calls.length"), count);
  for (let i = 0; i < 3; ++i) { e.fn("fire")(700); await drain(); }
  assert.equal(e.run(`calls.slice(${count}).every(c=>c.path.includes('search=light'))`), true);
  assert.match(e.run("calls[1].path"), /search=button&generation=41/);
  await close(e);
});

test("GET completion precedes DELETE and same-keyword reopen; cancellation uses the late generation", async () => {
  const e = setup();
  e.run("el.addTileSearch.value='button';apiGet=path=>{calls.push({method:'GET',path});return new Promise(resolve=>globalThis.finish=resolve);};");
  const old = e.fn("runAddTileSearch")(); await drain();
  e.fn("closeAddTileDialog")();
  e.fn("openAddTileDialog")();
  e.run("el.addTileSearch.value='button';");
  const next = e.fn("runAddTileSearch")({ refresh: true }); await drain();
  assert.equal(e.run("calls.length"), 1);
  e.run("apiGet=async path=>{calls.push({method:'GET',path});return {status:'ready',pending:false,items:[],generation:42};};finish(reply);");
  await old; await next;
  assert.deepEqual(Array.from(e.run("calls.map(c=>c.method)")), ["GET", "DELETE", "GET"]);
  assert.match(e.run("calls[1].path"), /generation=41/);
  assert.doesNotMatch(e.run("calls[2].path"), /generation=/);
  await close(e);
});

test("opening a field picker replaces the add-widget loop before an empty or single-letter search", async () => {
  const e = setup();
  e.run("el.addTileSearch.value='button';");
  const old = e.fn("runAddTileSearch")(); await drain();
  e.fn("openLightEntityPicker")("button", { resetSearch:true });
  await drain();
  const count = e.run("calls.length");
  e.fn("fire")(700); await old;
  assert.equal(e.run("calls.length"), count);
  e.run("el.lightEntityPickerSearch.value='b';el.lightEntityPickerSearch.oninput();");
  await drain(); e.fn("fire")(350); await drain();
  assert.match(e.run("calls.at(-1).path"), /search=b&refresh=1/);
  assert.equal(e.run(`calls.slice(2).some(c=>c.method==='GET'&&c.path.includes('search=button'))`), false);
  await close(e);
});

test("opening add-widget stops field polling, and pagehide invalidates cached-page timers", async () => {
  const e = setup();
  e.fn("openLightEntityPicker")("light_tile"); await drain();
  e.run("globalThis.oldTimer=[...jobs.values()][0].callback;");
  e.fn("openAddTileDialog")(); await drain();
  const count = e.run("calls.length");
  e.run("oldTimer();"); await drain();
  assert.equal(e.run("calls.length"), count);
  e.run("el.addTileSearch.value='light';");
  const next = e.fn("runAddTileSearch")(); await drain();
  e.run("events.pagehide();"); await drain();
  const closed = e.run("calls.length");
  e.fn("fire")(700); await next;
  assert.equal(e.run("calls.length"), closed);
  await close(e);
});

test("opening an idle cached field picker invalidates its previous in-flight result", async () => {
  const e = setup();
  e.run("apiGet=path=>{calls.push({method:'GET',path});return new Promise(resolve=>globalThis.finish=resolve);};");
  e.fn("openLightEntityPicker")("light_tile"); await drain();
  e.fn("openLightEntityPicker")("sensor", {resetSearch:true, configOverride:{domain:"sensor",minSearch:2,liveSearch:false}});
  e.run("finish({...reply,items:[{id:'light.stale'}]});"); await drain();
  assert.equal(e.run("rendered.status"), "idle");
  assert.equal(e.run("jobs.size"), 0);
  assert.match(e.run("calls.at(-1).path"), /generation=41/);
  await close(e);
});

test("field Enter cancels its debounce and retry waits for the prior query cancellation", async () => {
  const e = setup();
  e.fn("openLightEntityPicker")("light_tile"); await drain();
  e.run("el.lightEntityPickerSearch.value='light';el.lightEntityPickerSearch.oninput();");
  e.run("el.lightEntityPickerSearch.onkeydown({key:'Enter',preventDefault(){}});"); await drain();
  assert.equal(e.run("[...jobs.values()].some(j=>j.delay===350)"), false);
  assert.deepEqual(Array.from(e.run("calls.map(c=>c.method)")), ["GET", "DELETE", "GET"]);
  e.fn("fire")(700); await drain();
  assert.match(e.run("calls.at(-1).path"), /search=light&generation=41/);
  await close(e);
});

test("a replaced generation becomes a terminal retry message, never another first request", async () => {
  const e = setup();
  e.run("el.addTileSearch.value='button';");
  const work = e.fn("runAddTileSearch")(); await drain();
  e.run("reply={status:'error',pending:false,wait_reason:'replaced',items:[]};");
  e.fn("fire")(700); await work;
  assert.match(e.run("calls.at(-1).path"), /generation=41/);
  assert.match(e.run("el.addTileSearchStatus.textContent"), /newer search.*Retry/);
  assert.equal(e.run("jobs.size"), 0);
  await close(e);
});

test("unknown ownership after a failed first GET cannot cancel another window's query", async () => {
  const e = setup();
  e.run("el.addTileSearch.value='button';apiGet=async path=>{calls.push({method:'GET',path});throw new Error('lost reply');};");
  await e.fn("runAddTileSearch")(); await drain();
  assert.equal(e.run("calls.length"), 1);
  assert.match(e.run("el.addTileSearchStatus.textContent"), /Retry/);
  await close(e);
});

test("clearing or shortening add-widget search sends no empty or one-letter GET", async () => {
  const e = setup();
  for (const text of ["", "b"]) {
    e.run(`el.addTileSearch.value=${JSON.stringify(text)};el.addTileSearch.oninput();`);
    e.fn("fire")(350); await drain();
  }
  assert.equal(e.run("calls.length"), 0);
  await close(e);
});
