// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const source = fs.readFileSync(new URL('../../components/webui/www/js/xiaozhi.js', import.meta.url), 'utf8');
function setup(saved = {}, overrides = {}) {
  const model = {supported:true, enabled:false, paired:false, volume:50, state:'disabled', code:'', text:'', ...saved};
  const nodes = new Map(), events = {}, requests = [];
  function node(id) {
    if (!nodes.has(id)) nodes.set(id, {listeners:{}, classList:{contains:()=>false}, addEventListener(type, cb){this.listeners[type]=cb;}});
    return nodes.get(id);
  }
  let poll;
  const document = {getElementById:node, hidden:false, addEventListener:(type, cb)=>events[type]=cb};
  const context = {document, window:{addEventListener:(type, cb)=>events[type]=cb},
    setInterval:cb=>poll=cb, t:key=>key, apiGet:async()=>({...model}),
    fetch:async(url, options)=>{
      assert.equal(url, '/api/xiaozhi');
      const body = JSON.parse(options.body); requests.push(body);
      if (body.action==='enable') model.enabled=true;
      if (body.action==='disable') model.enabled=false;
      if (body.action==='volume') model.volume=body.value;
      if (body.action==='reset') {model.enabled=false;model.volume=50;}
      return {ok:true};
    }};
  Object.assign(context, overrides);
  vm.runInNewContext(source, context);
  async function fire(id, type, extra={}) {
    const target=node(id); await target.listeners[type]({target,type,preventDefault(){},...extra});
    await Promise.resolve();
  }
  return {model,node,events,requests,poll:()=>poll(),fire,document};
}
test('enable, volume, disable and reload preserve the saved volume and pairing',async()=>{
  const s=setup({paired:true}); await s.poll();
  s.node('xiaozhiEnabled').checked=true; await s.fire('xiaozhiEnabled','change'); await s.poll();
  assert.equal(s.node('xiaozhiFields').hidden,false);
  s.node('xiaozhiVolume').value='65'; await s.fire('xiaozhiVolume','change');
  s.node('xiaozhiEnabled').checked=false; await s.fire('xiaozhiEnabled','change');
  const reloaded=setup(s.model); await reloaded.poll();
  assert.equal(reloaded.node('xiaozhiFields').hidden,true);
  assert.equal(reloaded.node('xiaozhiVolume').value,65); assert.equal(reloaded.model.paired,true);
});
test('pointer release sends stop after start',async()=>{
  const s=setup({enabled:true,paired:true});await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  await s.events.pointerup();
  assert.deepEqual(s.requests.map(r=>r.action),['start','stop']);
});
test('leaving the settings page stops recording',async()=>{
  const s=setup({enabled:true,paired:true});await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  s.node('settingsXiaozhiSection').classList.contains=()=>true;await s.poll();
  assert.equal(s.requests.at(-1).action,'stop');
});
test('unsupported board disables enable and hides dependent controls',async()=>{
  const s=setup({supported:false});await s.poll();
  assert.equal(s.node('xiaozhiEnabled').disabled,true);assert.equal(s.node('xiaozhiFields').hidden,true);
});
test('reset is scoped to Xiaozhi and preserves cloud pairing',async()=>{
  const s=setup({enabled:true,paired:true,volume:70});await s.fire('xiaozhiReset','click');await s.poll();
  assert.equal(s.model.enabled,false);assert.equal(s.model.volume,50);assert.equal(s.model.paired,true);
  assert.equal(s.requests.length,1);assert.equal(s.requests[0].action,'reset');
});
test('server text is rendered as text, never HTML',async()=>{
  const s=setup({text:'<img src=x onerror=alert(1)>',code:'123456'});await s.poll();
  assert.equal(s.node('xiaozhiText').textContent,s.model.text);assert.equal(s.node('xiaozhiText').innerHTML,undefined);
});

test('all Xiaozhi HTML labels are wired into the existing translation refresh',()=>{
  const html=fs.readFileSync(new URL('../../components/webui/www/index.html',import.meta.url),'utf8');
  const translations=fs.readFileSync(new URL('../../components/webui/www/js/translations.js',import.meta.url),'utf8');
  const labels=[...html.matchAll(/<[^>]+id="([^"]+)"[^>]+data-i18n-key="(xiaozhi\.[^"]+)"/g)];
  assert.equal(labels.length,8);
  for (const [,id,key] of labels) assert.ok(translations.includes(`setTextById("${id}", "${key}")`),`${id} must refresh when language changes`);
});


test('rapid pairing and volume edits are queued rather than dropped',async()=>{
  const s=setup({enabled:true});await s.poll();
  s.node('xiaozhiVolume').value='35';
  await Promise.all([s.fire('xiaozhiPair','click'),s.fire('xiaozhiVolume','change')]);
  assert.deepEqual(s.requests.map(r=>r.action),['pair','volume']);
  assert.equal(s.model.volume,35);
});


for (const lang of ['zh-cn','zh-tw']) {
  test(`${lang}: network failures use local Chinese messages`,async()=>{
    const catalog=JSON.parse(fs.readFileSync(new URL(`../../components/webui/i18n/${lang}.json`,import.meta.url),'utf8'));
    const fail=async()=>{throw new Error('Failed to fetch');};
    const s=setup({}, {t:key=>catalog[key]||key, apiGet:fail, fetch:fail});
    await s.poll();
    assert.equal(s.node('xiaozhiStatus').textContent,catalog['xiaozhi.state.connection_error']);
    await s.fire('xiaozhiPair','click');
    assert.equal(s.node('xiaozhiStatus').textContent,catalog['xiaozhi.failed']);
  });
}
