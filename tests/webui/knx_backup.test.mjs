// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from "node:test";
import assert from "node:assert/strict";
import {loadEditor} from "./load_editor.mjs";

const registry = {media_type:"knx_ip", physical_address:"15.15.199",
  media_parameters:{multicast_address:"224.0.23.12",udp_port:3671,transport_mode:"routing",interface_identifier:"KNX-IP-Interface",nat:false},
  comObjects:[{id:"lamp",name:"客厅灯",group_address:"1/0/1",datapoint_type:"DPT-1.001",readable:true,writable:true}]};
const stored = {
  "/api/layout":{pages:[{id:"room",widgets:[
    {type:"slider",slider_direction:"auto",slider_entity_domain:"auto",slider_accent_color:"#6fe8ff",
      binding:{source:"knx",roles:{brightness_command:"A",brightness_feedback:"B"}}},
    {type:"scene_tile",binding:{source:"knx",object_id:"pulse",value:"on"}},
    {type:"sensor",entity_id:"sensor.room"}
  ]}]}, "/api/settings":{ui:{language:"en"},hardware:{rotation:180}},
  "/api/themes":{themes:[]}, "/api/i18n/languages":{languages:[]},
  "/api/knx":{enabled:true}, "/api/knx/config":registry,
  "/api/camera/config":{local:{enabled:false,stream_enabled:false},sources:[{id:"door",source:"http",snapshot_url:"http://camera.test/jpeg",password_set:true}]}
};
function registryWithCount(count) {
  return {...registry, comObjects:Array.from({length:count}, (_,i)=>({
    ...registry.comObjects[0], id:"point-"+i, group_address:`1/${i>>8}/${i%256}`, description:"点".repeat(40)
  }))};
}
const oversizedBackup = {format:"betta-panel-backup",layout:{pages:[]},settings:stored["/api/settings"],
  languages:{custom:{en:{label:"new"}}},themes:{custom:[{id:"room"}],active_id:"room"},cameras:stored["/api/camera/config"],
  knx:{configuration:registryWithCount(513),enabled:false}};

function setup(backup, {failure, storedRegistry=registry, language="en"}={}) {
  const writes=[], message={};
  const restoreButton={addEventListener(type,handler){if(type==="click") this.click=handler;}};
  const nodes={settingsBackupMsg:message,settingsBackupRestore:restoreButton,settingsBackupFile:{files:[{text:async()=>JSON.stringify(backup)}]}};
  let captured, confirmations=0;
  const {run,fn}=loadEditor();
  run(`editor.i18nMap=WEB_I18N_BUILTIN[${JSON.stringify(language)}]`);
  Object.assign(run("globalThis"), {
    document:{getElementById:id=>nodes[id],createElement:()=>({click(){}})},window:{confirm:()=>{confirmations++;return true;}},
    fetch:async(path,options)=>{
      if(options.method) {
        writes.push({path,body:JSON.parse(options.body)});
        if (failure && path===failure.path) {
          if (failure.network) throw new TypeError("Failed to fetch");
          return {ok:false,status:failure.status,text:async()=>failure.body};
        }
        return {ok:true};
      }
      return {ok:true,json:async()=>structuredClone(path==="/api/knx/config"?storedRegistry:stored[path])};
    },
    Blob,URL:{createObjectURL:b=>{captured=b;return "blob:test";},revokeObjectURL(){}},setTimeout(){}
  });
  run("bindBackup()");
  return {writes,message,run,t:fn("t"),clickRestore:()=>restoreButton.click(),confirmations:()=>confirmations,exported:async()=>JSON.parse(await captured.text())};
}
test("full backup includes KNX configuration and enable preference without changing existing settings",async()=>{
  const s=setup();await s.run("downloadBackup()");const backup=await s.exported();
  assert.deepEqual(backup.knx,{configuration:registry,enabled:true});
  assert.equal(backup.settings.ui.language,"en");assert.equal(backup.settings.hardware.rotation,180);
  const restore=setup(backup);await restore.clickRestore();
  assert.deepEqual(backup.layout,stored["/api/layout"]);
  assert.deepEqual(restore.writes.find(w=>w.path==="/api/layout").body,backup.layout);
  assert.deepEqual(restore.writes.slice(0,2),[
    {path:"/api/knx/config",body:registry},{path:"/api/knx",body:{enabled:true}}
  ]);
});

test("full backup includes camera preferences and redacted sources; restore keeps IDs and opt-in",async()=>{
  const s=setup();await s.run("downloadBackup()");const backup=await s.exported();
  assert.deepEqual(backup.cameras,stored["/api/camera/config"]);
  assert.equal('password' in backup.cameras.sources[0],false);
  const restore=setup(backup);await restore.clickRestore();
  assert.deepEqual(restore.writes.find(w=>w.path==='/api/camera/config').body,backup.cameras);
  assert.equal(backup.settings.ui.language,'en');assert.equal(backup.settings.hardware.rotation,180);
});

test("a full registry survives panel backup and restore in the same order",async()=>{
  const full=registryWithCount(512);
  const source=setup(undefined,{storedRegistry:full});await source.run("downloadBackup()");
  const backup=await source.exported();
  assert.deepEqual(backup.knx.configuration,full);
  const restored=setup(backup);await restored.clickRestore();
  assert.deepEqual(restored.writes[0],{path:"/api/knx/config",body:full});
  assert.deepEqual(restored.writes.map(w=>w.path),["/api/knx/config","/api/knx","/api/camera/config","/api/layout","/api/settings"]);
  assert.deepEqual(restored.writes.at(-1).body,{...backup.settings,reboot:true});
  assert.equal(restored.message.textContent,restored.t("settings.backup.restored"));
});
test("legacy backups leave KNX intact",async()=>{
  const backup={format:"betta-panel-backup",layout:{pages:[]},settings:{ui:{language:"en"}}};
  const legacy=setup(backup);await legacy.clickRestore();
  assert.deepEqual(legacy.writes.map(w=>w.path),["/api/layout","/api/settings"]);
});
test("513 objects show the limit in the current editor language before confirmation or any write", async () => {
  const expected = {en:"at most 512 communication objects",de:"maximal 512 Kommunikationsobjekte",
    es:"como máximo 512 objetos de comunicación",fr:"au maximum 512 objets de communication",
    pl:"maksymalnie 512 obiektów komunikacyjnych","zh-cn":"最多支持 512 个通信对象","zh-tw":"最多支援 512 個通訊物件"};
  for (const [language, phrase] of Object.entries(expected)) {
    const s=setup(oversizedBackup,{language});
    await s.clickRestore();
    assert.ok(s.message.textContent.includes(phrase),language);
    assert.equal(s.message.textContent,s.t("knx.object_limit",{max:512}));
    assert.equal(s.confirmations(),0);
    assert.deepEqual(s.writes,[]);
  }
});
test("refusing a backup neither writes settings nor cancels a pending language autosave", async () => {
  const s=setup(oversizedBackup,{language:"zh-cn"});
  const language=new EventTarget();
  language.value="zh-cn";
  s.run("el").settingsLanguage=language;
  let saveLanguage;
  const browser=s.run("globalThis");
  browser.setTimeout=callback=>{saveLanguage=callback;return 1;};
  browser.clearTimeout=id=>{if(id===1) saveLanguage=null;};
  s.run("bindHardwareLive()");
  language.dispatchEvent(new Event("change"));
  assert.equal(typeof saveLanguage,"function");

  await s.clickRestore();
  assert.equal(s.message.textContent,"KNX 注册表最多支持 512 个通信对象。");
  assert.equal(s.confirmations(),0);
  assert.deepEqual(s.writes,[]);
  assert.equal(typeof saveLanguage,"function");
  await saveLanguage();
  assert.deepEqual(s.writes.map(w=>w.path),["/api/settings"]);
  assert.equal(s.writes[0].body.ui.language,"zh-cn");
  assert.equal(s.writes[0].body.reboot,false);
});
test("restore localizes the panel's precise KNX capacity rejection and keeps unrelated errors generic", async () => {
  const backup={format:"betta-panel-backup",layout:{pages:[]},settings:{},knx:{configuration:registry,enabled:true}};
  const path="/api/knx/config", detail="Too many KNX communication objects";
  for (const [failure,expected] of [
    [{path,status:400,body:detail},"knx.object_limit"],
    [{path,status:400,body:JSON.stringify({error:detail})},"knx.object_limit"],
    [{path,status:400,body:"Invalid group address"},"common.request_failed"],
    [{path,status:413,body:"Request too large"},"common.request_failed"],
    [{path,status:500,body:detail},"common.request_failed"],
    [{path,network:true},"common.request_failed"],
    [{path:"/api/layout",status:400,body:detail},"common.request_failed"],
  ]) {
    const s=setup(backup,{failure,language:"zh-cn"});
    await s.clickRestore();
    assert.equal(s.confirmations(),1);
    const message = expected==="knx.object_limit" ? s.t(expected,{max:512}) : s.t("settings.backup.failed")+s.t(expected);
    assert.equal(s.message.textContent,message);
    assert.deepEqual(s.writes.map(w=>w.path),failure.path===path?[path]:[path,"/api/knx","/api/layout"]);
  }
});
test("all seven native editor catalogs include the KNX controls and status text",()=>{
  const {run}=loadEditor();
  const catalogs=JSON.parse(run("JSON.stringify(WEB_I18N_BUILTIN)"));
  const keys=Object.keys(catalogs.en).filter(k=>k.startsWith("knx.") || k==="settings.nav.knx");
  assert.ok(keys.length>=29);
  for(const lang of ["en","de","es","fr","pl","zh-CN","zh-TW"]) {
    const c=catalogs[lang] || catalogs[lang.toLowerCase()];assert.ok(c,lang);
    for(const key of keys) assert.ok(c[key],`${lang}: ${key}`);
  }
});
test("KNX settings hide the unrelated global mobile save action",()=>{
  const {run}=loadEditor();
  run('setupSettingsWorkspace=()=>{}; editor.activePane="settings"; let saveHidden; el.stackedSaveBtn={classList:{toggle:(c,v)=>saveHidden=v}};');
  run('setActiveSettingsSection("settingsKnxSection")');assert.equal(run('saveHidden'),true);
  run('setActiveSettingsSection("settingsDeviceSection")');assert.equal(run('saveHidden'),false);
});
