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
    if (!nodes.has(id)) nodes.set(id, {listeners:{}, attrs:{}, getAttribute(k){return this.attrs[k];}, setAttribute(k,v){this.attrs[k]=v;}, removeAttribute(k){delete this.attrs[k];}, classList:{contains:()=>false}, addEventListener(type, cb){this.listeners[type]=cb;}});
    return nodes.get(id);
  }
  let poll;
  const document = {getElementById:node, hidden:false, addEventListener:(type, cb)=>events[type]=cb};
  const context = {document, window:{addEventListener:(type, cb)=>events[type]=cb},
    AbortController, setTimeout, clearTimeout,
    setInterval:cb=>poll=cb, t:key=>key, apiGet:async()=>({...model}),
    fetch:async(url, options)=>{
      assert.equal(url, '/api/xiaozhi');
      const body = JSON.parse(options.body); requests.push(body);
      if (body.action==='enable') model.enabled=true;
      if (body.action==='disable') model.enabled=false;
      if (body.action==='volume') model.volume=body.value;
      if (body.action==='wake') model.wake_enabled=!!body.value;
      if (body.action==='timeout') model.dialog_timeout=body.value;
      if (body.action==='emoji') model.emoji_style=body.value;
      if (body.action==='reset') {model.enabled=false;model.volume=50;model.wake_enabled=false;model.dialog_timeout=30;model.emoji_style=0;}
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

test('slow status polling never accumulates overlapping requests', async()=>{
  let resolve, calls=0;
  const s=setup({}, {apiGet:()=>{calls++;return new Promise(r=>resolve=r);}});
  const first=s.poll();
  await Promise.all(Array.from({length:20},()=>s.poll()));
  assert.equal(calls,1);
  resolve({...s.model});await first;
  const second=s.poll();assert.equal(calls,2);resolve({...s.model});await second;
});
test('a response started before an edit cannot overwrite the control', async()=>{
  let resolve;
  const s=setup({enabled:true,emoji_style:0}, {apiGet:()=>new Promise(r=>resolve=r)});
  const poll=s.poll();s.node('xiaozhiEmoji').value='1';await s.fire('xiaozhiEmoji','change');
  resolve({...s.model,emoji_style:0});await poll;
  assert.equal(s.node('xiaozhiEmoji').value,'1');assert.equal(s.model.emoji_style,1);
});
test('status deadline releases the single-flight guard after a lost connection', async()=>{
  let abort,calls=0,cleared=0;
  const s=setup({}, {setTimeout:cb=>{abort=cb;return 1;},clearTimeout:()=>cleared++,
    apiGet:(_path,signal)=>{calls++;return new Promise((_r,reject)=>signal.addEventListener('abort',()=>reject(new Error('timeout'))));}});
  let poll=s.poll();abort();await poll;
  assert.equal(s.node('xiaozhiStatus').textContent,'xiaozhi.state.connection_error');
  poll=s.poll();assert.equal(calls,2);abort();await poll;assert.equal(cleared,2);
});
test('leaving the page while polling does not apply its delayed response', async()=>{
  let resolve;const s=setup({}, {apiGet:()=>new Promise(r=>resolve=r)});
  const poll=s.poll();s.document.hidden=true;resolve({...s.model,enabled:true});await poll;
  assert.equal(s.node('xiaozhiEnabled').checked,undefined);
});

test('all Xiaozhi HTML labels are wired into the existing translation refresh',()=>{
  const html=fs.readFileSync(new URL('../../components/webui/www/index.html',import.meta.url),'utf8');
  const translations=fs.readFileSync(new URL('../../components/webui/www/js/translations.js',import.meta.url),'utf8');
  const labels=[...html.matchAll(/<[^>]+id="([^"]+)"[^>]+data-i18n-key="(xiaozhi\.[^"]+)"/g)];
  assert.equal(labels.length,16);
  for (const [,id,key] of labels) assert.ok(translations.includes(`setTextById("${id}", "${key}")`),`${id} must refresh when language changes`);
});

test('Otto is default, classic selection persists and reset restores Otto without changing pairing',async()=>{
  const s=setup({enabled:true,paired:true,emotion:'happy'});await s.poll();
  assert.equal(s.node('xiaozhiOtto').hidden,false);
  assert.equal(s.node('xiaozhiOtto').getAttribute('src'),'/api/xiaozhi/emoji?emotion=happy');
  s.node('xiaozhiEmoji').value='1';await s.fire('xiaozhiEmoji','change');await s.poll();
  assert.equal(s.node('xiaozhiEmotion').hidden,false);assert.equal(s.node('xiaozhiOtto').getAttribute('src'),undefined);
  const reload=setup(s.model);await reload.poll();assert.equal(reload.node('xiaozhiEmoji').value,1);
  reload.node('xiaozhiEnabled').checked=false;await reload.fire('xiaozhiEnabled','change');await reload.poll();
  assert.equal(reload.model.emoji_style,1);
  await reload.fire('xiaozhiReset','click');assert.equal(reload.model.emoji_style,0);assert.equal(reload.model.paired,true);
  const invalid=setup({enabled:true,emotion:'../credentials'});await invalid.poll();
  assert.equal(invalid.node('xiaozhiOtto').getAttribute('src'),'/api/xiaozhi/emoji?emotion=neutral');
  invalid.node('xiaozhiEmoji').value='2';await invalid.fire('xiaozhiEmoji','change');assert.equal(invalid.requests.length,0);
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

test('wake and timeout edits survive disable and reload, reset remains scoped',async()=>{
  const s=setup({enabled:true,paired:true});await s.poll();
  s.node('xiaozhiWake').checked=true;await s.fire('xiaozhiWake','change');
  s.node('xiaozhiTimeout').value='45';await s.fire('xiaozhiTimeout','change');
  s.node('xiaozhiEnabled').checked=false;await s.fire('xiaozhiEnabled','change');
  const r=setup(s.model);await r.poll();assert.equal(r.node('xiaozhiWake').checked,true);
  assert.equal(r.node('xiaozhiTimeout').value,45);
  await r.fire('xiaozhiReset','click');assert.equal(r.model.wake_enabled,false);assert.equal(r.model.dialog_timeout,30);
  assert.equal(r.model.paired,true);
});
test('talk hides only when local wake engine is ready, with manual fallback on failure',async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:true,wake_ready:true,recognized:'hello',reply:'hi',emotion:'happy'});await s.poll();
  assert.equal(s.node('xiaozhiTalk').hidden,true);
  await s.fire('xiaozhiTalk','pointerdown');assert.equal(s.requests.length,0);
  assert.equal(s.node('xiaozhiRecognized').textContent,'hello');assert.equal(s.node('xiaozhiText').textContent,'hi');
  assert.equal(s.node('xiaozhiEmotion').textContent,'🙂');
  s.model.wake_ready=false;await s.poll();assert.equal(s.node('xiaozhiTalk').hidden,false);
  for(const value of ['0','301','1.2','bad']) {s.node('xiaozhiTimeout').value=value;await s.fire('xiaozhiTimeout','change');}
  assert.equal(s.requests.length,0);
});
