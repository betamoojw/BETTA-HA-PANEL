// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import {loadEditor} from "./load_editor.mjs";

const settings = fs.readFileSync(new URL("../../components/webui/www/js/settings.js",import.meta.url),"utf8");
const backupSource = settings.slice(settings.indexOf("async function backupGet("), settings.indexOf("function bindBackup("));
const registry = {media_type:"knx_ip",comObjects:[{id:"lamp",name:"客厅灯",writable:true}]};
const stored = {
  "/api/layout":{pages:[]}, "/api/settings":{ui:{language:"en"},hardware:{rotation:180}},
  "/api/themes":{themes:[]}, "/api/i18n/languages":{languages:[]},
  "/api/knx":{enabled:true}, "/api/knx/config":registry,
  "/api/camera/config":{local:{enabled:false,stream_enabled:false},sources:[{id:"door",source:"http",snapshot_url:"http://camera.test/jpeg",password_set:true}]}
};
function setup(backup, failKnx=false) {
  const writes=[], message={}, nodes={settingsBackupMsg:message,settingsBackupFile:{files:[{text:async()=>JSON.stringify(backup)}]}};
  let captured;
  const ctx=vm.createContext({
    document:{getElementById:id=>nodes[id],createElement:()=>({click(){}})},window:{confirm:()=>true},
    fetch:async(path,options)=>{
      if(options.method) {
        writes.push({path,body:JSON.parse(options.body)});
        return {ok:!(failKnx && path==="/api/knx/config"),text:async()=>"invalid registry"};
      }
      return {ok:true,json:async()=>structuredClone(stored[path])};
    },
    Blob,URL:{createObjectURL:b=>{captured=b;return "blob:test";},revokeObjectURL(){}},setTimeout(){},
    normalizeTextStep:n=>n||0,UI_STYLE_DEFAULT:"flat",t:k=>k,localizedRequestError:e=>e.message,
    normalizeLayoutWidgets(){},storageSet(){},RESTORE_NOTICE_KEY:"test"
  });
  vm.runInContext(backupSource,ctx);
  return {writes,message,run:code=>vm.runInContext(code,ctx),exported:async()=>JSON.parse(await captured.text())};
}
test("full backup includes KNX configuration and enable preference without changing existing settings",async()=>{
  const s=setup();await s.run("downloadBackup()");const backup=await s.exported();
  assert.deepEqual(backup.knx,{configuration:registry,enabled:true});
  assert.equal(backup.settings.ui.language,"en");assert.equal(backup.settings.hardware.rotation,180);
  const restore=setup(backup);await restore.run("restoreBackup()");
  assert.deepEqual(restore.writes.slice(0,2),[
    {path:"/api/knx/config",body:registry},{path:"/api/knx",body:{enabled:true}}
  ]);
});

test("full backup includes camera preferences and redacted sources; restore keeps IDs and opt-in",async()=>{
  const s=setup();await s.run("downloadBackup()");const backup=await s.exported();
  assert.deepEqual(backup.cameras,stored["/api/camera/config"]);
  assert.equal('password' in backup.cameras.sources[0],false);
  const restore=setup(backup);await restore.run("restoreBackup()");
  assert.deepEqual(restore.writes.find(w=>w.path==='/api/camera/config').body,backup.cameras);
  assert.equal(backup.settings.ui.language,'en');assert.equal(backup.settings.hardware.rotation,180);
});
test("legacy backups leave KNX intact and rejected registries cannot enable it",async()=>{
  const backup={format:"betta-panel-backup",layout:{pages:[]},settings:{ui:{language:"en"}}};
  const legacy=setup(backup);await legacy.run("restoreBackup()");
  assert.deepEqual(legacy.writes.map(w=>w.path),["/api/layout","/api/settings"]);
  const rejected=setup({...backup,knx:{configuration:registry,enabled:true}},true);
  await rejected.run("restoreBackup()");
  assert.deepEqual(rejected.writes.map(w=>w.path),["/api/knx/config"]);
  assert.match(rejected.message.textContent,/invalid registry/);
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
