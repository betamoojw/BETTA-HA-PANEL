// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import test from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

function setup() {
  const { run } = loadEditor();
  run(`
    globalThis.clock=0; globalThis.performance={now:()=>clock};
    globalThis.label={textContent:''}; globalThis.height=28;
    globalThis.banner={hidden:true,querySelector:()=>label,getBoundingClientRect:()=>({height})};
    document.getElementById=id=>id==='licenseWarning'?banner:null;
    document.body={dataset:{}}; document.hidden=false;
    globalThis.spacing={}; document.documentElement.style={setProperty:(k,v)=>spacing[k]=v,getPropertyValue:k=>spacing[k]||''};
    globalThis.events={}; document.addEventListener=(n,f)=>events[n]=f;
    document.removeEventListener=n=>delete events[n];
    window.addEventListener=(n,f)=>events[n]=f; window.removeEventListener=n=>delete events[n];
    globalThis.timeouts=new Map(); globalThis.intervalCount=0; globalThis.nextTimer=1;
    window.setTimeout=f=>{const id=nextTimer++;timeouts.set(id,f);return id;}; window.clearTimeout=id=>timeouts.delete(id);
    window.setInterval=f=>{globalThis.tick=f;intervalCount++;return 1;}; window.clearInterval=()=>intervalCount--;
    globalThis.observed=0; globalThis.ResizeObserver=class {constructor(fn){globalThis.resize=fn;} observe(){observed++;} disconnect(){observed--;}};
    globalThis.requests=[]; globalThis.responseState='missing';
    globalThis.fetch=async(url,options)=>{requests.push({url,options});return {ok:true,json:async()=>({key_state:responseState})};};
  `);
  return run;
}
const settle = run => run("Promise.resolve().then(()=>{}).then(()=>{}).then(()=>{})");

test("global warning covers unauthenticated/setup/editor states and releases all reserved space when valid", async () => {
  const run=setup();
  run("userAccess.authenticated=false; licenseWarningStart(); licenseWarningStart()"); await settle(run);
  assert.equal(run("requests.length"),1);
  assert.equal(run("requests[0].url"),"/api/license/status");
  assert.equal(run("requests[0].options.cache"),"no-store");
  assert.equal(run("intervalCount"),1);
  for (const state of ["missing","invalid","error","unconfigured","valid"]) {
    run(`licenseWarningUpdate(${JSON.stringify(state)})`);
    assert.equal(run("banner.hidden"),state==="valid");
    assert.equal(run("spacing['--license-warning-height']"),state==="valid"?"0px":"28px");
  }
  assert.equal(run("requests.length"),1);
  assert.equal(run("userAccess.authenticated"),false);
});

test("translations and wrapped height refresh without duplicate banners or API writes", () => {
  const run=setup();
  run("licenseWarningUpdate('invalid')");
  for (const language of ["en","de","es","fr","pl","zh-cn","zh-tw"]) {
    run(`editor.i18nMap=WEB_I18N_BUILTIN[${JSON.stringify(language)}]; userTranslate()`);
    assert.equal(run("label.textContent"),run("t('license.invalid')+' — '+t('license.warning_usage')"));
  }
  run("height=72; licenseWarningMeasure()");
  assert.equal(run("spacing['--license-warning-height']"),"72px");
  run("licenseWarningUpdate('valid')");
  assert.equal(run("spacing['--license-warning-height']"),"0px");
  assert.equal(run("label.textContent"),"");
  assert.equal(run("requests.length"),0);
});

test("polling skips hidden tabs, bounds hung requests and resumes once per minute", async () => {
  const run=setup();
  run("licenseWarningStart()"); await settle(run);
  run("document.hidden=true; clock=60000; tick()"); await settle(run);
  assert.equal(run("requests.length"),1);
  run("document.hidden=false; events.visibilitychange()"); await settle(run);
  assert.equal(run("requests.length"),2);
  run("tick()"); await settle(run); assert.equal(run("requests.length"),2);
  run("fetch=(url,options)=>new Promise((resolve,reject)=>{requests.push({url,options}); options.signal.addEventListener('abort',()=>reject(new Error('abort')));}); clock=120000; tick(); tick()");
  assert.equal(run("requests.length"),3);
  run("[...timeouts.values()][0]()"); await settle(run);
  assert.equal(run("licenseWarningAbort"),null);
  assert.equal(run("licenseWarningState"),"unknown");
  run("licenseWarningStop()");
  assert.equal(run("intervalCount"),0); assert.equal(run("observed"),0);
  assert.equal(run("Object.keys(events).length"),0);
});

test("an import response wins over stale public data, including rejected or late reads", async () => {
  const run=setup();
  run("fetch=()=>new Promise(resolve=>globalThis.finish=resolve); licenseWarningStart(); licenseWarningUpdate('valid')");
  run("finish({ok:true,json:async()=>({key_state:'missing'})})"); await settle(run);
  assert.equal(run("licenseWarningState"),"valid"); assert.equal(run("banner.hidden"),true);
  run("fetch=async()=>({ok:false});"); await run("licenseWarningRefresh()");
  assert.equal(run("licenseWarningState"),"unknown");
  assert.ok(run("label.textContent.startsWith(t('license.unknown'))"));
  run("fetch=async()=>({ok:true,json:async()=>({key_state:'<script>invalid</script>'})})"); await run("licenseWarningRefresh()");
  assert.equal(run("licenseWarningState"),"unknown");
  run("fetch=()=>new Promise(resolve=>globalThis.finish=resolve); licenseWarningRefresh(); licenseWarningStop(); finish({ok:true,json:async()=>({key_state:'valid'})})"); await settle(run);
  assert.equal(run("licenseWarningState"),"unknown");
});

test("local previews do not claim a device license or start status polling", async () => {
  const run=setup(); run("document.body.dataset.localUi='classic'; licenseWarningStart()");
  await run("licenseWarningRefresh()");
  assert.equal(run("requests.length"),0); assert.equal(run("intervalCount"),0);
  assert.equal(run("banner.hidden"),true);
});
