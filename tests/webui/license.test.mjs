// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import { createHash } from "node:crypto";
import { loadEditor } from "./load_editor.mjs";

function setup(role = "administrator", local = false) {
  const { run } = loadEditor();
  run(`
    globalThis.nodes = {}; globalThis.clock = 1000;
    globalThis.performance = {now:()=>clock};
    document.getElementById = id => nodes[id] ||= {id, textContent:'', hidden:false, disabled:false,
      dataset:{}, attrs:{}, files:[], _value:'', checked:false,
      set value(v){this._value=v;if(id==='licenseFile'&&!v)this.files=[];}, get value(){return this._value;},
      setAttribute(k,v){this.attrs[k]=v;}, appendChild(n){this.child=n;}, focus(){this.focused=true;}, select(){this.selected=true;}};
    globalThis.storage = new Map();
    window.localStorage = {getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)};
    globalThis.saved = {device_id:'a'.repeat(64), key_state:'missing', trial_seconds:3600, status_only:true};
    globalThis.requests = []; globalThis.status = 200;
    userAccess = {authenticated:true, role:${JSON.stringify(role)}, username:'owner', local:${local}};
    editor.activePane='settings'; editor.activeSettingsSection='settingsLicenseSection';
    window.confirm = () => true;
    window.setInterval = callback => {globalThis.tick=callback;return 1;}; window.clearInterval = ()=>{};
    window.setTimeout = callback => {globalThis.deadline=callback;return 2;}; window.clearTimeout = ()=>{};
    globalThis.fetch = async (url, options) => {
      requests.push({url,...options});
      if (status===200 && options.method==='POST') saved.key_state='valid';
      if (status===200 && options.method==='DELETE') saved.key_state='missing';
      return {ok:status===200, status, json:async()=>({...saved})};
    };
    licenseBind();
  `);
  return run;
}
const settle = run => run("Promise.resolve().then(()=>{}).then(()=>{})");
const key = "BETTA1:" + "a".repeat(64) + ":" + "b".repeat(128);

test("settings license events paste, activate, reload, remove and expire without gating", async () => {
  const run = setup(); await settle(run);
  assert.match(run("nodes.licenseTrial.textContent"), /01:00:00/);
  run(`nodes.licenseKey.value=${JSON.stringify(key)}; nodes.licenseKey.oninput();`);
  await run("nodes.licenseImport.onclick()");
  assert.equal(run("saved.key_state"), "valid");
  assert.equal(run("nodes.licenseActivation.hidden"), true);
  assert.equal(run("nodes.licenseKey.value"), "");
  assert.equal(run("requests.find(r=>r.method==='POST').body.length"), 200);
  assert.equal(run("requests.find(r=>r.method==='POST').headers['Content-Type']"), "application/octet-stream");
  run("licenseState=null"); await run("nodes.licenseRefresh.onclick()");
  assert.equal(run("licenseState.key_state"), "valid");
  await run("nodes.licenseRemove.onclick()");
  run("saved.trial_seconds=1"); await run("licenseLoad()");
  run("clock+=2000; tick()");
  assert.match(run("nodes.licenseTrial.textContent"), /all features remain available/);
  assert.equal(run("nodes.licenseActivation.hidden"), false);
  assert.equal(run("userAccess.role"), "administrator");
  assert.equal(run("userAccess.authenticated"), true);
  assert.equal(run("requests.some(r=>r.url!=='/api/license')"), false);
});

test("file and paste inputs are exclusive; bounded file import accepts CRLF", async () => {
  const run = setup(); await settle(run);
  run(`nodes.licenseKey.value='stale'; nodes.licenseFile.files=[{size:202,text:async()=>${JSON.stringify(key + '\r\n')}}]; nodes.licenseFile.onchange();`);
  assert.equal(run("nodes.licenseKey.value"), "");
  await run("nodes.licenseImport.onclick()");
  assert.equal(run("saved.key_state"), "valid");
  assert.equal(run("nodes.licenseFile.files.length"), 0);
  run("nodes.licenseFile.files=[{}]; nodes.licenseKey.oninput()");
  assert.equal(run("nodes.licenseFile.files.length"), 0);
});

test("oversized, truncated, malformed and wrong-device keys preserve saved state", async () => {
  const run = setup(); await settle(run);
  for (const size of [0, 199, 203, 1024*1024]) {
    run(`nodes.licenseFile.files=[{size:${size},text:async()=>{throw new Error('must not read');}}];`);
    await run("licenseChange()");
  }
  for (const text of ["x".repeat(200), key.toUpperCase(), key + "x", " " + key]) {
    run(`nodes.licenseKey.value=${JSON.stringify(text)}`); await run("licenseChange()");
  }
  assert.equal(run("requests.filter(r=>r.method==='POST').length"), 0);
  run(`nodes.licenseKey.value=${JSON.stringify(key)}; status=400`); await run("licenseChange()");
  assert.equal(run("licenseState.key_state"), "missing");
  assert.equal(run("licenseBusy"), false);
  assert.match(run("nodes.licenseMessage.textContent"), /invalid/);
});

test("all roles retain read access; only administrators import and remove", async () => {
  for (const role of ["viewer", "operator", "installer", "administrator"]) {
    const run = setup(role); await settle(run);
    if (["viewer", "operator"].includes(role)) {
      assert.equal(run("nodes.userLicenseMount.child.id"), "settingsLicenseSection");
      assert.equal(run("requests.length"), 0);
      run("nodes.userSettingsOpen.onclick()"); await settle(run);
      assert.equal(run("nodes.userViewerScreen.hidden"), true);
      assert.equal(run("userSettingsSections().map(i=>i.sectionId).join(',')"), "settingsLicenseSection");
    }
    assert.equal(run("licenseState.key_state"), "missing");
    assert.equal(run("nodes.licenseActions.hidden"), role !== "administrator");
    if (role !== "administrator") {
      run(`nodes.licenseKey.value=${JSON.stringify(key)}`); await run("licenseChange()"); await run("licenseChange(true)");
      assert.equal(run("requests.filter(r=>r.method!=='GET').length"), 0);
    }
  }
});

test("local preview and unconfigured issuer disable activation; cancel and concurrency do not write", async () => {
  const local = setup("administrator", true); await settle(local);
  await local("licenseChange()");
  assert.equal(local("requests.length"), 0);
  assert.equal(local("nodes.licenseImport.disabled"), true);
  assert.equal(local("nodes.licenseAgreementRead.disabled"), true);
  const run = setup(); await settle(run);
  run("saved.key_state='unconfigured'"); await run("licenseLoad()");
  assert.equal(run("nodes.licenseImport.disabled"), true);
  await run("licenseChange()");
  run("window.confirm=()=>false"); await run("licenseChange(true)");
  run("window.confirm=()=>true; licenseBusy=true"); await run("licenseChange(true)");
  assert.equal(run("requests.filter(r=>r.method!=='GET').length"), 0);
});

test("license tabs support click and keyboard without changing draft or device", async () => {
  const run = setup(); await settle(run);
  run("nodes.licenseKey.value='draft'; nodes.agreementTab.onclick()");
  assert.equal(run("nodes.licensePanel.hidden"), true);
  assert.equal(run("nodes.agreementPanel.hidden"), false);
  run("nodes.agreementTab.onkeydown({key:'Home',preventDefault(){}})");
  assert.equal(run("nodes.licenseTab.attrs['aria-selected']"), "true");
  assert.equal(run("nodes.licenseTab.focused"), true);
  assert.equal(run("nodes.licenseKey.value"), "draft");
  assert.equal(run("requests.length"), 1);
});

test("agreement acknowledgment survives reload, is reversible and scoped to account/device/revision", async () => {
  const run = setup(); await settle(run);
  run("nodes.licenseAgreementRead.checked=true; nodes.licenseAgreementRead.onchange()");
  assert.equal(run("storage.size"), 1);
  run("nodes.licenseAgreementRead.checked=false; licenseAgreementRender()");
  assert.equal(run("nodes.licenseAgreementRead.checked"), true);
  assert.match(run("nodes.licenseAgreementMessage.textContent"), /acknowledged/);
  run("userAccess.username='other'; licenseAgreementRender()");
  assert.equal(run("nodes.licenseAgreementRead.checked"), false);
  run("userAccess.username='owner'; licenseState.device_id='c'.repeat(64); licenseAgreementRender()");
  assert.equal(run("nodes.licenseAgreementRead.checked"), false);
  run("licenseState.device_id='a'.repeat(64); LICENSE_AGREEMENT.revision+='new'; licenseAgreementRender()");
  assert.equal(run("nodes.licenseAgreementRead.checked"), false);
  run("LICENSE_AGREEMENT.revision=LICENSE_AGREEMENT.revision.slice(0,-3); licenseAgreementRender(); nodes.licenseAgreementRead.checked=false; nodes.licenseAgreementRead.onchange()");
  assert.equal(run("storage.size"), 0);
  assert.equal(run("requests.length"), 1);
  assert.equal(run("saved.key_state"), "missing");
});

test("blocked browser storage reports failure without claiming an acknowledgment or gating", async () => {
  const run = setup(); await settle(run);
  run("window.localStorage.setItem=()=>{throw new Error('blocked')}; nodes.licenseAgreementRead.checked=true; nodes.licenseAgreementRead.onchange()");
  assert.equal(run("nodes.licenseAgreementRead.checked"), false);
  assert.match(run("nodes.licenseAgreementMessage.textContent"), /could not/);
  assert.equal(run("nodes.licenseImport.disabled"), false);
});

test("leaving cancels pending requests, clears key and ignores late results; logout removes identity", async () => {
  const run = setup(); await settle(run);
  run("globalThis.resolveFetch=null; fetch=(url,opts)=>{globalThis.signal=opts.signal;return new Promise(resolve=>resolveFetch=resolve)}; globalThis.pending=licenseLoad(); nodes.licenseKey.value='transient'; licenseSetVisible(false)");
  assert.equal(run("signal.aborted"), true);
  assert.equal(run("nodes.licenseKey.value"), "");
  run("resolveFetch({ok:true,json:async()=>({...saved,key_state:'valid'})})"); await run("pending");
  assert.equal(run("licenseState.key_state"), "missing");
  run("userAccess.authenticated=false; licenseReset()");
  assert.equal(run("nodes.licenseDevice.value"), "");
  assert.equal(run("nodes.licenseAgreementRead.checked"), false);
});

test("navigation or logout during file reading cannot import the old file", async () => {
  const run = setup(); await settle(run);
  run("nodes.licenseFile.files=[{size:200,text:()=>new Promise(resolve=>globalThis.resolveFile=resolve)}]; globalThis.pending=licenseChange(); licenseSetVisible(false)");
  run(`resolveFile(${JSON.stringify(key)})`); await run("pending");
  assert.equal(run("requests.filter(r=>r.method==='POST').length"), 0);
});

test("polling is visible-only, bounded after failures and HTTP timeout releases busy state", async () => {
  const run = setup(); await settle(run);
  run("document.hidden=true; clock+=60000; tick()");
  assert.equal(run("requests.length"), 1);
  run("document.hidden=false; status=503; tick()"); await settle(run);
  assert.equal(run("requests.length"), 2);
  run("clock+=1000; tick()"); await settle(run);
  assert.equal(run("requests.length"), 2);
  run("fetch=(url,opts)=>new Promise((resolve,reject)=>opts.signal.addEventListener('abort',()=>reject(new Error('aborted')))); globalThis.pending=licenseLoad(); deadline()");
  await run("pending");
  assert.equal(run("licenseBusy"), false);
  assert.match(run("nodes.licenseMessage.textContent"), /Could not/);
});

test("agreement bundle exactly matches LICENSE including third-party notices and content hash", () => {
  const {run} = loadEditor();
  const text = fs.readFileSync(new URL("../../LICENSE", import.meta.url), "utf8").replace(/\r\n/g, "\n");
  assert.equal(run("LICENSE_AGREEMENT.text"), text);
  assert.equal(run("LICENSE_AGREEMENT.revision"), createHash("sha256").update(text).digest("hex"));
  assert.match(text, /12\. Third-Party Components/);
});
