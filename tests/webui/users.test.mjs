// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import test from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

function setup() {
  const { run } = loadEditor();
  run(`
    globalThis.TextEncoder = class { encode(s) { return {length: unescape(encodeURIComponent(s)).length}; } };
    globalThis.nodes = {};
    document.getElementById = id => nodes[id] ||= {
      value: '', checked: false, hidden: false, disabled: false, textContent: '', children: [],
      focus() {}, close() {}, showModal() {}, reset() {},
      appendChild(child) { this.children.push(child); }, replaceChildren() { this.children = []; },
      setAttribute() {}, querySelectorAll() { return []; }, classList: { add() {}, remove() {} }
    };
    document.createElement = () => ({children: [], setAttribute() {}, appendChild(child) { this.children.push(child); }});
    globalThis.requests = [];
    globalThis.savedRows = [];
    userShowLogin = message => { globalThis.loginMessage = message; };
    userRequest = async (url, payload) => {
      requests.push({url,payload});
      if (payload) {
        const index = savedRows.findIndex(row => row.username === payload.username);
        if (payload.action === 'create') savedRows.push({...payload});
        if (payload.action === 'update') savedRows[index] = {...savedRows[index], ...payload,
          password: payload.password || savedRows[index].password};
        if (payload.action === 'delete') savedRows.splice(index,1);
        return {ok:true};
      }
      return {users:savedRows.map(({username,role,enabled}) => ({username,role,enabled}))};
    };
    userAccess = {enabled:true,authenticated:true,username:'owner',role:'administrator'};
    usersBind();
  `);
  return run;
}

test("user form events persist creation, role, disable/re-enable, blank-password retention and reload", async () => {
  const run = setup();
  run(`nodes.usersAdd.onclick();
    nodes.usersName.value='reader'; nodes.usersPassword.value=nodes.usersRepeat.value='password1';
    nodes.usersRole.value='viewer'; nodes.usersEnabled.checked=true;`);
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  assert.equal(run("requests[0].url"), "/api/users");
  assert.equal(run("savedRows[0].role"), "viewer");
  assert.equal(run("nodes.usersPassword.value"), "");
  await run("usersLoad()");
  assert.equal(run("usersRows[0].username"), "reader");
  assert.equal(run("usersRows[0].password"), undefined);
  run("usersEdit(usersRows[0]); nodes.usersRole.value='operator'; nodes.usersEnabled.checked=false");
  assert.equal(run("nodes.usersName.disabled"), true);
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  await run("usersLoad()");
  assert.equal(run("usersRows[0].enabled"), false);
  assert.equal(run("usersRows[0].role"), "operator");
  run("usersEdit(usersRows[0]); nodes.usersEnabled.checked=true");
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  await run("usersLoad()");
  assert.equal(run("usersRows[0].enabled"), true);
  assert.equal(run("savedRows[0].password"), "password1");
  assert.equal(run("nodes.usersRepeat.value"), "");
});

test("invalid account input never reaches the save API; UTF-8 byte length is enforced", async () => {
  const run = setup();
  run(`nodes.usersAdd.onclick(); nodes.usersName.value='bad name'; nodes.usersPassword.value=nodes.usersRepeat.value='password1'`);
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  assert.equal(run("requests.length"), 0);
  run(`nodes.usersName.value='reader'; nodes.usersPassword.value=nodes.usersRepeat.value='密'.repeat(22)`);
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  assert.equal(run("requests.length"), 0);
  run(`nodes.usersPassword.value='password1'; nodes.usersRepeat.value='password2'`);
  await run("nodes.usersForm.onsubmit({preventDefault(){}})");
  assert.equal(run("requests.length"), 0);
  assert.equal(run("usersBuildChange('create',{username:'reader',password:'密码安全',repeat:'密码安全',role:'viewer',enabled:true}).password"), "密码安全");
});

test("roles expose only the intended editor and administration modes", async () => {
  const run = setup();
  for (const [role, edit, admin] of [["viewer",false,false],["operator",false,false],["installer",true,false],["administrator",true,true]]) {
    run(`userAccess.role=${JSON.stringify(role)}`);
    assert.equal(run("userCanEdit()"), edit);
    assert.equal(run("userIsAdmin()"), admin);
  }
  run("userAccess.role='viewer'"); await run("usersLoad()");
  assert.equal(run("requests.length"), 0);
});

test("cancel clears transient passwords and opening another account retains no password", () => {
  const run = setup();
  run("usersEdit({username:'reader',role:'viewer',enabled:true}); nodes.usersPassword.value='temporary'; nodes.usersRepeat.value='temporary'; nodes.usersCancel.onclick()");
  assert.equal(run("nodes.usersPassword.value"), "");
  assert.equal(run("nodes.usersRepeat.value"), "");
  assert.equal(run("nodes.usersForm.hidden"), true);
});

test("all seven account catalogs contain identical keys and interpolation fields", () => {
  const { run } = loadEditor();
  const catalogs = JSON.parse(run("JSON.stringify(WEB_I18N_BUILTIN)"));
  const keys = Object.keys(catalogs.en).filter(key => key.startsWith("users.")).sort();
  assert.ok(keys.length > 30);
  for (const language of ["de","es","fr","pl","zh-cn","zh-tw"]) {
    assert.deepEqual(Object.keys(catalogs[language]).filter(key => key.startsWith("users.")).sort(), keys);
    for (const key of keys) {
      assert.ok(catalogs[language][key]);
      assert.deepEqual(catalogs[language][key].match(/\{\w+\}/g), catalogs.en[key].match(/\{\w+\}/g));
    }
  }
});


test("fetch transport adds CSRF marker only to same-origin API mutations and expires once", async () => {
  const { run, fn } = loadEditor();
  fn("values => Object.assign(globalThis, values)")({URL, Request, Headers});
  run(`window.location = {href:'http://panel.local/',origin:'http://panel.local'};
    globalThis.calls = []; globalThis.expired = 0; globalThis.status = 200;
    userShowLogin = () => { expired++; };
    window.fetch = async (input, options) => { calls.push({input,options}); return {status}; };
    userInstallFetch();`);
  await run(`window.fetch(new URL('http://panel.local/api/users'), {method:'POST',headers:{'Content-Type':'application/json'},body:'{}'})`);
  assert.equal(run("calls[0].options.headers.get('X-Betta-Request')"), "1");
  assert.equal(run("calls[0].options.credentials"), "same-origin");
  await run(`window.fetch(new Request('http://panel.local/api/themes/custom', {method:'DELETE'}))`);
  assert.equal(run("calls[1].options.headers.get('X-Betta-Request')"), "1");
  await run(`window.fetch('https://api.github.com/repos/example', {method:'GET'})`);
  assert.equal(run("calls[2].options.headers"), undefined);
  run("status=401");
  await run(`window.fetch('/api/auth/password',{method:'POST'})`);
  assert.equal(run("expired"), 0);
  await run(`window.fetch('/api/layout')`);
  await run(`window.fetch('/api/state')`);
  assert.equal(run("expired"), 1);
  assert.equal(run("userAccess.authenticated"), false);
});

test("installer settings navigation includes only themes and licensing; simulator cannot create accounts", async () => {
  const run = setup();
  run("userAccess.role='installer'");
  assert.equal(run("userSettingsSections().map(item => item.sectionId).join(',')"), "settingsThemeSection,settingsLicenseSection");
  assert.equal(run("activeSettingsNavItem('settingsUsersSection').sectionId"), "settingsThemeSection");
  run("userAccess.role='administrator'; userAccess.local=true");
  await run("usersLoad()");
  assert.equal(run("nodes.usersAdd.disabled"), true);
  assert.equal(run("requests.length"), 0);
});

test("OTA upload keeps the cookie mutation marker and handles an expired session", async () => {
  const { run } = loadEditor();
  run(`globalThis.uploadHeaders={}; globalThis.expired=0;
    userShowLogin = () => { expired++; };
    setOtaInfo = setOtaProgress = setOtaControlsDisabled = () => {};
    el.settingsOtaFile = {files:[{size:32}]};
    globalThis.XMLHttpRequest = class {
      constructor(){ this.upload={}; this.status=401; this.responseText='{}'; }
      open(){} setRequestHeader(k,v){ uploadHeaders[k]=v; } send(){ this.onload(); }
    };`);
  await run("uploadOtaFile()");
  assert.equal(run("uploadHeaders['X-Betta-Request']"), "1");
  assert.equal(run("uploadHeaders['Content-Type']"), "application/octet-stream");
  assert.equal(run("expired"), 1);
});
