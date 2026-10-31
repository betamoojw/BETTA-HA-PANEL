// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const source = fs.readFileSync(new URL('../../components/webui/www/js/xiaozhi.js', import.meta.url), 'utf8');
function setup(saved = {}, overrides = {}) {
  const model = {supported:true, enabled:false, paired:false, volume:50, state:'disabled', code:'', text:'', wake_config_revision:0, ...saved};
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
      if (body.action==='wake_config') {model.wake_word=body.value.word;model.wake_phrase=body.value.phrase;model.wake_config_revision++;model.wake_config_failed=false;}
      if (body.action==='wake') model.wake_enabled=!!body.value;
      if (body.action==='timeout') model.dialog_timeout=body.value;
      if (body.action==='emoji') model.emoji_style=body.value;
      if (body.action==='reset') {model.enabled=false;model.volume=50;model.wake_enabled=false;model.dialog_timeout=30;model.emoji_style=0;model.wake_word=0;model.wake_phrase='ni hao xiao zhi';}
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
  assert.equal(labels.length,22);
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

 test('custom phrase saves JSON, survives reload/disable and reset restores defaults', async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:true,wake_model_ready:true});await s.poll();
  s.node('xiaozhiWakeWord').value='3';await s.fire('xiaozhiWakeWord','change');
  s.node('xiaozhiWakePhrase').value='NI HAO GUAN JIA';await s.fire('xiaozhiWakePhrase','input');
  await s.poll();assert.equal(s.node('xiaozhiWakePhrase').value,'NI HAO GUAN JIA');
  await s.fire('xiaozhiWakeSave','click');await s.poll();
  assert.deepEqual(s.requests.at(-1),{action:'wake_config',value:{word:3,phrase:'ni hao guan jia'}});
  assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.wake_saved');
  s.node('xiaozhiEnabled').checked=false;await s.fire('xiaozhiEnabled','change');
  const r=setup(JSON.parse(JSON.stringify(s.model)));await r.poll();
  assert.equal(r.node('xiaozhiWakeWord').value,3);assert.equal(r.node('xiaozhiWakePhrase').value,'ni hao guan jia');
  r.node('xiaozhiEnabled').checked=true;await r.fire('xiaozhiEnabled','change');await r.poll();
  assert.equal(r.node('xiaozhiCustomFields').hidden,false);
  await r.fire('xiaozhiReset','click');await r.poll();
  assert.equal(r.node('xiaozhiWakeWord').value,0);assert.equal(r.node('xiaozhiWakePhrase').value,'ni hao xiao zhi');
  assert.equal(r.model.paired,true);
 });
 test('invalid custom input sends nothing and failed saves retain the editable draft', async()=>{
  const s=setup({enabled:true,wake_word:0});await s.poll();s.node('xiaozhiWakeWord').value='3';
  for(const phrase of ['', 'ni hao', 'ni hao 123', '你好管家', 'ni '.repeat(13), '<script>']) {
    s.node('xiaozhiWakePhrase').value=phrase;await s.fire('xiaozhiWakeSave','click');
  }
  assert.equal(s.requests.length,0);
  const f=setup({enabled:true}, {fetch:async()=>({ok:false})});await f.poll();
  f.node('xiaozhiWakeWord').value='3';f.node('xiaozhiWakePhrase').value='ni hao guan jia';
  await f.fire('xiaozhiWakeSave','click');await f.poll();
  assert.equal(f.node('xiaozhiWakePhrase').value,'ni hao guan jia');
  assert.equal(f.node('xiaozhiWakeSave').disabled,false);
  assert.equal(f.node('xiaozhiWakeResult').textContent,'xiaozhi.state.wake_config_error');
 });

test('worker rejection remains visible after detection returns to ready', async()=>{
 const s=setup({enabled:true,wake_word:0,wake_phrase:'ni hao xiao zhi',wake_config_revision:4}, {fetch:async()=>({ok:true})});
 await s.poll();s.node('xiaozhiWakeWord').value='3';s.node('xiaozhiWakePhrase').value='zz zz zz';
 await s.fire('xiaozhiWakeSave','click');
 assert.equal(s.node('xiaozhiWakePhrase').disabled,true);
 assert.equal(s.node('xiaozhiWakeWord').disabled,true);
 await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,true);
 s.model.wake_config_revision=5;s.model.wake_config_failed=true;s.model.state='ready';await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,false);
 assert.equal(s.node('xiaozhiWakePhrase').value,'zz zz zz');
 assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.state.wake_config_error');
});

test('lost save across a device restart unlocks the retained draft after the deadline', async()=>{
 let now=1000;
 const s=setup({enabled:true,wake_word:0,wake_phrase:'ni hao xiao zhi',wake_config_revision:0},
   {fetch:async()=>({ok:true}),Date:{now:()=>now}});
 await s.poll();s.node('xiaozhiWakeWord').value='3';s.node('xiaozhiWakePhrase').value='ni hao xiao bei';
 await s.fire('xiaozhiWakeSave','click');now=30000;await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,true);
 now=32000;await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,false);
 assert.equal(s.node('xiaozhiWakePhrase').disabled,false);
 assert.equal(s.node('xiaozhiWakePhrase').value,'ni hao xiao bei');
 assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.state.wake_config_error');
});

test('an invalid hidden custom draft does not block a preset save or disappear', async()=>{
 const s=setup({enabled:true,wake_word:3,wake_phrase:'ni hao xiao bei'});await s.poll();
 s.node('xiaozhiWakePhrase').value='unfinished draft!';await s.fire('xiaozhiWakePhrase','input');
 s.node('xiaozhiWakeWord').value='1';await s.fire('xiaozhiWakeWord','change');
 await s.fire('xiaozhiWakeSave','click');await s.poll();
 assert.deepEqual(s.requests.at(-1),{action:'wake_config',value:{word:1,phrase:'ni hao xiao bei'}});
 assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.wake_saved');
 s.node('xiaozhiWakeWord').value='3';await s.fire('xiaozhiWakeWord','change');await s.poll();
 assert.equal(s.node('xiaozhiWakePhrase').value,'unfinished draft!');
 const reload=setup(s.model);await reload.poll();
 assert.equal(reload.node('xiaozhiWakeWord').value,1);
 assert.equal(reload.node('xiaozhiWakePhrase').value,'ni hao xiao bei');
});

test('saving the existing phrase waits for the worker and reports its failure', async()=>{
 const s=setup({enabled:true,wake_word:3,wake_phrase:'ni hao xiao bei'}, {fetch:async()=>({ok:true})});
 await s.poll();await s.fire('xiaozhiWakeSave','click');await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,true);
 s.model.wake_config_revision++;s.model.wake_config_failed=true;await s.poll();
 assert.equal(s.node('xiaozhiWakeSave').disabled,false);
 assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.state.wake_config_error');
});

test('a stalled save request expires, retains the draft and releases the command queue', async()=>{
 let abort, stalled=true, completed=0;
 const s=setup({enabled:true}, {
  setTimeout:(cb,ms)=>{if(ms===10000)abort=cb;return 1;},clearTimeout:()=>{},
  fetch:async(_url,{signal})=>{
   if(stalled)return new Promise((_,reject)=>signal.addEventListener('abort',()=>reject(new Error('deadline'))));
   completed++;return {ok:true};
  }
 });
 await s.poll();s.node('xiaozhiWakeWord').value='3';s.node('xiaozhiWakePhrase').value='ni hao xiao bei';
 const save=s.fire('xiaozhiWakeSave','click');await Promise.resolve();abort();await save;
 assert.equal(s.node('xiaozhiWakeSave').disabled,false);
 assert.equal(s.node('xiaozhiWakePhrase').value,'ni hao xiao bei');
 stalled=false;s.node('xiaozhiVolume').value='60';await s.fire('xiaozhiVolume','change');
 assert.equal(completed,1);
});

test('model upload sends ordered chunks and requires final device verification', async()=>{
 for(const verified of [true,false]) {
  const offsets=[],file=new Blob([new Uint8Array(65539)]);
  const s=setup({}, {
   setTimeout:(cb,ms)=>{if(ms===150)queueMicrotask(cb);return 1;},clearTimeout:()=>{},
   fetch:async(_url,options)=>{
    if(!options.method)return {ok:true,json:async()=>({bytes:file.size,ready:false})};
    const offset=Number(options.headers['X-Model-Offset']);offsets.push(offset);
    assert.equal(options.body.size,Math.min(32768,file.size-offset));
    return {ok:true,json:async()=>({ready:verified&&offset+32768>=file.size})};
   }
  });
  s.node('xiaozhiModelFile').files=[file];await s.fire('xiaozhiModelFile','change');
  assert.deepEqual(offsets,[0,32768,65536]);
  assert.equal(s.node('xiaozhiWakeResult').textContent,verified?'xiaozhi.model_ready':'xiaozhi.model_failed');
  assert.equal(s.node('xiaozhiModelFile').disabled,false);
 }
});

test('a stalled upload response body expires and restores settings polling', async()=>{
 let abort,reading=false;
 const s=setup({enabled:true,wake_phrase:'ni hao xiao bei'}, {
  setTimeout:(cb,ms)=>{if(ms===20000)abort=cb;return 1;},clearTimeout:()=>{},
  fetch:async(_url,{signal,method})=>({ok:true,json:async()=>{
   if(!method)return {bytes:3,ready:false};
   reading=true;return new Promise((_,reject)=>signal.addEventListener('abort',()=>reject(new Error('deadline'))));
  }})
 });
 s.node('xiaozhiModelFile').files=[new Blob(['abc'])];
 const upload=s.fire('xiaozhiModelFile','change');
 for(let i=0;i<12&&!reading;i++)await Promise.resolve();
 assert.equal(reading,true);abort();await upload;
 assert.equal(s.node('xiaozhiModelFile').disabled,false);
 assert.equal(s.node('xiaozhiWakeResult').textContent,'xiaozhi.model_failed');
 await s.poll();assert.equal(s.node('xiaozhiWakePhrase').value,'ni hao xiao bei');
});
