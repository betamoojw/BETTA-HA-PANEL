// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import test from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

function setup() {
  const { run } = loadEditor();
  run(`
    globalThis.nodes = {};
    document.getElementById = id => nodes[id] ||= {value:'', textContent:'', hidden:false, disabled:false,
      files:[], addEventListener(){}, showModal(){this.open=true;}, close(){this.open=false;}, select(){this.selected=true;}};
    globalThis.saved = {device_id:'a'.repeat(64), key_state:'missing', trial_seconds:3600, status_only:true};
    globalThis.requests = []; globalThis.fail = false;
    userAccess = {authenticated:true, role:'administrator'};
    window.confirm = () => true; window.setInterval = () => 1;
    licenseRequest = async (method='GET', body) => {
      requests.push({method,body});
      if (fail) throw new Error('save failed');
      if (method==='POST') saved.key_state='valid';
      if (method==='DELETE') saved.key_state='missing';
      return {...saved};
    };
    licenseBind();
    licenseNode('licenseFile');
  `);
  return run;
}

test("license events import, reload and remove; no change to account or panel controls on expiry", async () => {
  const run = setup();
  await run("Promise.resolve()");
  run(`nodes.licenseOpen.onclick();`); await run("Promise.resolve()");
  assert.equal(run("nodes.licenseDialog.open"), true);
  run(`nodes.licenseFile.files=[{size:200,text:async()=> 'BETTA1:'+'a'.repeat(64)+':'+'b'.repeat(128)}];`);
  await run("nodes.licenseImport.onclick()");
  assert.equal(run("saved.key_state"), "valid");
  assert.equal(run("nodes.licenseFile.value"), "");
  assert.equal(run("requests.find(r=>r.method==='POST').body.length"), 200);
  run("licenseState=null"); await run("licenseLoad()");
  assert.equal(run("licenseState.key_state"), "valid");
  await run("nodes.licenseRemove.onclick()");
  run("saved.trial_seconds=0"); await run("licenseLoad()");
  assert.equal(run("licenseState.key_state"), "missing");
  assert.equal(run("userAccess.role"), "administrator");
  assert.equal(run("userAccess.authenticated"), true);
  assert.match(run("nodes.licenseTrial.textContent"), /all features remain available/);
});

test("reject oversized, truncated and malformed files before sending; failed save preserves displayed state", async () => {
  const run = setup(); await run("Promise.resolve()");
  for (const size of [0, 199, 203, 1024*1024]) {
    run(`nodes.licenseFile.files=[{size:${size},text:async()=>{throw new Error('must not read');}}];`);
    await run("licenseChange()");
  }
  run(`nodes.licenseFile.files=[{size:200,text:async()=> 'x'.repeat(200)}];`);
  await run("licenseChange()");
  assert.equal(run("requests.filter(r=>r.method==='POST').length"), 0);
  run(`nodes.licenseFile.files=[{size:200,text:async()=> 'BETTA1:'+'a'.repeat(64)+':'+'b'.repeat(128)}]; fail=true;`);
  await run("licenseChange()");
  assert.equal(run("licenseState.key_state"), "missing");
  assert.equal(run("licenseBusy"), false);
  assert.equal(run("nodes.licenseMessage.textContent"), "save failed");
});

test("viewer sees status only; local preview, cancelled removal and concurrent operations do not write", async () => {
  const run = setup(); await run("Promise.resolve()");
  run("userAccess.role='viewer'; licenseRender()"); await run("licenseChange(true)");
  assert.equal(run("nodes.licenseActions.hidden"), true);
  run("userAccess.role='administrator'; window.confirm=()=>false"); await run("licenseChange(true)");
  run("window.confirm=()=>true; licenseBusy=true"); await run("licenseChange(true)");
  run("licenseBusy=false; userAccess.local=true"); await run("licenseChange(true)");
  assert.equal(run("requests.filter(r=>r.method!=='GET').length"), 0);
});
