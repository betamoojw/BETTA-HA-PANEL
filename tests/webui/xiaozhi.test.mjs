// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const source = fs.readFileSync(new URL('../../components/webui/www/js/xiaozhi.js', import.meta.url), 'utf8');
function setup(saved = {}, overrides = {}) {
  const model = {supported:true, enabled:false, paired:false, volume:50, duplex_enabled:true, state:'disabled', code:'', text:'', wake_config_revision:0,
    dialog_epoch:197,dialog_seq:1,dialog_end_seq:0,dialog_active:!!(saved.conversation||saved.listening||saved.speaking),
    dialog_voice_opened:!!saved.wake_enabled,silence_remaining_ms:30000,dialog_end_reason:'',...saved};
  const nodes = new Map(), events = {}, requests = [], intervals = new Map();
  function node(id) {
    if (!nodes.has(id)) nodes.set(id, {listeners:{}, attrs:{}, style:{}, children:[], appendChild(child){this.children.push(child);}, getAttribute(k){return this.attrs[k];}, setAttribute(k,v){this.attrs[k]=v;}, removeAttribute(k){delete this.attrs[k];}, classList:{contains:()=>false}, addEventListener(type, cb){this.listeners[type]=cb;}});
    return nodes.get(id);
  }
  const document = {getElementById:node, createElement:()=>({style:{}}), hidden:false, addEventListener:(type, cb)=>events[type]=cb};
  const context = {document, window:{addEventListener:(type, cb)=>events[type]=cb},
    AbortController, setTimeout, clearTimeout,
    setInterval:(cb,ms)=>intervals.set(ms,cb), t:key=>key, apiGet:async()=>({...model}),
    fetch:async(url, options)=>{
      assert.equal(url, '/api/xiaozhi');
      const body = JSON.parse(options.body); requests.push(body);
      if (body.action==='enable') model.enabled=true;
      if (body.action==='disable') model.enabled=false;
      if (['disable','reset','disconnect'].includes(body.action) || (body.action==='dismiss'&&body.value===model.dialog_seq&&body.epoch===model.dialog_epoch)) {
        model.dialog_active=false;model.dialog_end_seq=model.dialog_seq;model.dialog_end_reason='manual';
        model.conversation=model.listening=model.speaking=false;
        if (body.action==='dismiss'&&['connection_error','audio_or_connection_error'].includes(model.state)) model.state='ready';
      }
      if (body.action==='start'&&!model.dialog_active) {model.dialog_active=true;model.dialog_seq++;model.dialog_end_reason='';}
      if (body.action==='dialog_turn'&&!model.dialog_active&&body.value===model.dialog_seq&&body.epoch===model.dialog_epoch) {
        model.dialog_active=true;model.dialog_seq=(model.dialog_seq+1)>>>0;model.dialog_voice_opened=false;model.dialog_end_reason='';
      }
      if (body.action==='volume') model.volume=body.value;
      if (body.action==='wake_config') {model.wake_word=body.value.word;model.wake_phrase=body.value.phrase;model.wake_config_revision++;model.wake_config_failed=false;}
      if (body.action==='wake') model.wake_enabled=!!body.value;
      if (body.action==='duplex') model.duplex_enabled=!!body.value;
      if (body.action==='timeout') model.dialog_timeout=body.value;
      if (body.action==='emoji') model.emoji_style=body.value;
      if (body.action==='reset') {model.enabled=false;model.volume=50;model.wake_enabled=false;model.duplex_enabled=true;model.dialog_timeout=30;model.emoji_style=0;model.wake_word=0;model.wake_phrase='ni hao xiao zhi';}
      return {ok:true};
    }};
  Object.assign(context, overrides);
  vm.runInNewContext(source, context);
  async function fire(id, type, extra={}) {
    const target=node(id); await target.listeners[type]({target,type,preventDefault(){},...extra});
    await Promise.resolve();
  }
  return {model,node,events,requests,poll:async()=>{await intervals.get(1000)();await intervals.get(200)();},tick:ms=>intervals.get(ms)(),fire,document};
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
test('manual press opens a guarded dialogue and release targets the same generation',async()=>{
  const s=setup({enabled:true,paired:true});await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  await s.events.pointerup();
  assert.deepEqual(s.requests,[{action:'dialog_turn',value:1,epoch:197},{action:'start_turn',value:2,epoch:197},{action:'stop_turn',value:2,epoch:197}]);
});
test('leaving the settings page stops recording',async()=>{
  const s=setup({enabled:true,paired:true});await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  s.node('settingsXiaozhiSection').classList.contains=()=>true;await s.poll();
  assert.equal(s.requests.at(-1).action,'stop_turn');
});
test('unsupported board disables enable and hides dependent controls',async()=>{
  const s=setup({supported:false});await s.poll();
  assert.equal(s.node('xiaozhiEnabled').disabled,true);assert.equal(s.node('xiaozhiFields').hidden,true);
});
test('reset is scoped to AI Assistant and preserves cloud pairing',async()=>{
  const s=setup({enabled:true,paired:true,volume:70});await s.fire('xiaozhiReset','click');await s.poll();
  assert.equal(s.model.enabled,false);assert.equal(s.model.volume,50);assert.equal(s.model.paired,true);
  assert.equal(s.requests.length,1);assert.equal(s.requests[0].action,'reset');
});
test('server text is rendered as text, never HTML',async()=>{
  const s=setup({enabled:true,dialog_active:true,text:'<img src=x onerror=alert(1)>',code:'123456'});await s.poll();
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

for (const leave of ['tab', 'section', 'pagehide']) test(`${leave}: return before a delayed response cannot revive the previous view`, async()=>{
  let resolve, signal;
  const s=setup({enabled:true,dialog_active:true,state:'listening',reply:'Old reply',
    spectrum:{microphone:Array(24).fill(255),mic_active:true}},
    {apiGet:(_path,requestSignal)=>{signal=requestSignal;return new Promise(r=>resolve=r);}});
  const first=s.tick(1000);
  if (leave==='tab') { s.document.hidden=true;s.events.visibilitychange();s.document.hidden=false; }
  else if (leave==='pagehide') s.events.pagehide?.();
  else {
    s.node('settingsXiaozhiSection').classList.contains=()=>true;s.tick(100);
    s.node('settingsXiaozhiSection').classList.contains=()=>false;
  }
  assert.equal(signal.aborted,true);
  resolve({...s.model});await first; // A body/parser can finish even after cancellation.
  assert.notEqual(s.node('xiaozhiText').textContent,'Old reply');
  assert.ok(s.node('xiaozhiSpectrum').children.every(b=>!b.style.height||b.style.height==='4px'));
  const next=s.tick(1000);resolve({...s.model,reply:'Current reply'});await next;
  assert.equal(s.node('xiaozhiText').textContent,'Current reply');
});

test('ten virtual minutes keep polling bounded and unchanged bars avoid style writes',async()=>{
  let calls=0, writes=0, now=1000;
  const s=setup({enabled:true,dialog_active:true,state:'speaking_listening',reply:'Long response. '.repeat(200),
    spectrum:{speaker:Array(24).fill(180),speaker_active:true}},
    {performance:{now:()=>now},apiGet:async()=>{calls++;return {...s.model};}});
  await s.tick(1000);
  for (const bar of s.node('xiaozhiSpectrum').children) for (const key of ['height','opacity']) {
    let value=bar.style[key];
    Object.defineProperty(bar.style,key,{get:()=>value,set:v=>{writes++;value=v;}});
  }
  for (let i=0;i<3000;i++) { now+=200;await s.tick(200);s.tick(100); }
  assert.equal(calls,3001);assert.equal(writes,0);
  for (let i=0;i<24;i++) {
    s.model.spectrum={microphone:Array.from({length:24},(_,n)=>n===i?255:0),mic_active:true};
    now+=200;await s.tick(200);
    assert.equal(s.node('xiaozhiSpectrum').children[i].style.height,'64px');
  }
  s.model.spectrum.microphone.fill(0);now+=200;await s.tick(200);
  assert.ok(s.node('xiaozhiSpectrum').children.every(b=>b.style.height==='4px'));
  s.document.hidden=true;s.events.visibilitychange();
  for (let i=0;i<5;i++) {now+=1000;await s.tick(1000);await s.tick(200);}
  assert.equal(calls,3026);
});

test('all AI Assistant HTML labels are wired into the existing translation refresh',()=>{
  const html=fs.readFileSync(new URL('../../components/webui/www/index.html',import.meta.url),'utf8');
  const translations=fs.readFileSync(new URL('../../components/webui/www/js/translations.js',import.meta.url),'utf8');
  const labels=[...html.matchAll(/<[^>]+id="([^"]+)"[^>]+data-i18n-key="(xiaozhi\.[^"]+)"/g)];
  assert.equal(labels.length,25);
  for (const [,id,key] of labels) assert.ok(translations.includes(`setTextById("${id}", "${key}")`),`${id} must refresh when language changes`);
});

test('Otto is default, classic selection persists and reset restores Otto without changing pairing',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,emotion:'happy'});await s.poll();
  assert.equal(s.node('xiaozhiOtto').hidden,false);
  assert.equal(s.node('xiaozhiOtto').getAttribute('src'),'/api/xiaozhi/emoji?emotion=happy');
  s.node('xiaozhiEmoji').value='1';await s.fire('xiaozhiEmoji','change');await s.poll();
  assert.equal(s.node('xiaozhiEmotion').hidden,false);assert.equal(s.node('xiaozhiOtto').getAttribute('src'),undefined);
  const reload=setup(s.model);await reload.poll();assert.equal(reload.node('xiaozhiEmoji').value,1);
  reload.node('xiaozhiEnabled').checked=false;await reload.fire('xiaozhiEnabled','change');await reload.poll();
  assert.equal(reload.model.emoji_style,1);
  await reload.fire('xiaozhiReset','click');assert.equal(reload.model.emoji_style,0);assert.equal(reload.model.paired,true);
  const invalid=setup({enabled:true,dialog_active:true,emotion:'../credentials'});await invalid.poll();
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

test('fixed silence policy has no editable timeout and preserves wake settings and pairing',async()=>{
  const s=setup({enabled:true,paired:true,dialog_timeout:45});await s.poll();
  s.node('xiaozhiWake').checked=true;await s.fire('xiaozhiWake','change');
  assert.equal(s.node('xiaozhiTimeout').listeners.change,undefined);
  const html=fs.readFileSync(new URL('../../components/webui/www/index.html',import.meta.url),'utf8');
  assert.doesNotMatch(html,/<input[^>]*id="xiaozhiTimeout"/);
  assert.match(html,/id="xiaozhiTimeoutLabel"[^>]*data-i18n-key="xiaozhi.timeout"/);
  s.node('xiaozhiEnabled').checked=false;await s.fire('xiaozhiEnabled','change');
  const r=setup(s.model);await r.poll();assert.equal(r.node('xiaozhiWake').checked,true);
  assert.equal(r.model.dialog_timeout,45);assert.equal(s.requests.some(r=>r.action==='timeout'),false);
  await r.fire('xiaozhiReset','click');assert.equal(r.model.wake_enabled,false);assert.equal(r.model.dialog_timeout,30);
  assert.equal(r.model.paired,true);
});
test('full-duplex preference saves JSON, survives disable and reload, and reset restores it',async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:false});await s.poll();
  assert.equal(s.node('xiaozhiDuplex').checked,true);
  s.node('xiaozhiDuplex').checked=false;await s.fire('xiaozhiDuplex','change');
  assert.deepEqual(s.requests.at(-1),{action:'duplex',value:0});
  s.node('xiaozhiEnabled').checked=false;await s.fire('xiaozhiEnabled','change');
  const r=setup(JSON.parse(JSON.stringify(s.model)));await r.poll();
  assert.equal(r.node('xiaozhiFields').hidden,true);assert.equal(r.node('xiaozhiDuplex').checked,false);
  r.node('xiaozhiEnabled').checked=true;await r.fire('xiaozhiEnabled','change');await r.poll();
  assert.equal(r.node('xiaozhiFields').hidden,false);assert.equal(r.node('xiaozhiDuplex').checked,false);
  assert.equal(r.model.wake_enabled,false);assert.equal(r.model.paired,true);
  r.node('xiaozhiDuplex').checked=true;await r.fire('xiaozhiDuplex','change');
  assert.deepEqual(r.requests.at(-1),{action:'duplex',value:1});
  const reloaded=setup(JSON.parse(JSON.stringify(r.model)));await reloaded.poll();
  assert.equal(reloaded.node('xiaozhiDuplex').checked,true);
  reloaded.node('xiaozhiDuplex').checked=false;await reloaded.fire('xiaozhiDuplex','change');
  await reloaded.fire('xiaozhiReset','click');await reloaded.poll();
  assert.equal(reloaded.node('xiaozhiDuplex').checked,true);assert.equal(reloaded.model.enabled,false);
  assert.equal(reloaded.model.paired,true);assert.equal(reloaded.model.wake_enabled,false);
});
test('duplex preference does not start capture or change manual press and release',async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:false});await s.poll();
  s.node('xiaozhiDuplex').checked=true;await s.fire('xiaozhiDuplex','change');
  assert.deepEqual(s.requests.map(r=>r.action),['duplex']);
  await s.fire('xiaozhiTalk','pointerdown');await s.events.pointerup();
  assert.deepEqual(s.requests.map(r=>r.action),['duplex','dialog_turn','start_turn','stop_turn']);
});
test('duplex edits reject delayed status and show the saved value after a failed command',async()=>{
  let resolve;
  const s=setup({enabled:true}, {apiGet:()=>new Promise(r=>resolve=r)});
  const poll=s.poll();s.node('xiaozhiDuplex').checked=false;await s.fire('xiaozhiDuplex','change');
  resolve({...s.model,duplex_enabled:true});await poll;
  assert.equal(s.node('xiaozhiDuplex').checked,false);assert.equal(s.model.duplex_enabled,false);
  const fail=setup({enabled:true}, {fetch:async()=>({ok:false})});await fail.poll();
  fail.node('xiaozhiDuplex').checked=false;await fail.fire('xiaozhiDuplex','change');
  assert.equal(fail.node('xiaozhiStatus').textContent,'xiaozhi.failed');
  await fail.poll();assert.equal(fail.node('xiaozhiDuplex').checked,true);
});
test('simultaneous speaking and listening has an offline translation in every catalog',async()=>{
  for(const lang of ['es','fr','pl','zh-cn','zh-tw']) {
    const catalog=JSON.parse(fs.readFileSync(new URL(`../../components/webui/i18n/${lang}.json`,import.meta.url),'utf8'));
    const native=JSON.parse(fs.readFileSync(new URL(`../../main/i18n/${lang}.json`,import.meta.url),'utf8'));
    const key='xiaozhi.state.speaking_listening';
    assert.ok(catalog['xiaozhi.duplex']);assert.ok(catalog['xiaozhi.duplex_hint']);assert.ok(catalog[key]);
    assert.equal(native.lvgl.xiaozhi.state.speaking_listening,catalog[key]);
    const s=setup({enabled:true,duplex_active:true,listening:true,speaking:true,state:'speaking_listening'}, {t:k=>catalog[k]||k});
    await s.poll();assert.equal(s.node('xiaozhiStatus').textContent,catalog[key]);
  }
});
test('talk hides only when local wake engine is ready, with manual fallback on failure',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,wake_enabled:true,wake_ready:true,recognized:'hello',reply:'hi',emotion:'happy'});await s.poll();
  assert.equal(s.node('xiaozhiTalk').hidden,true);
  await s.fire('xiaozhiTalk','pointerdown');assert.equal(s.requests.length,0);
  assert.equal(s.node('xiaozhiRecognized').textContent,'hello');assert.equal(s.node('xiaozhiText').textContent,'hi');
  assert.equal(s.node('xiaozhiEmotion').textContent,'🙂');
  s.model.wake_ready=false;await s.poll();assert.equal(s.node('xiaozhiTalk').hidden,false);
  assert.equal(s.requests.length,0);
});

test('only backend silence time drives the last-five-seconds hint and new audio removes it',async()=>{
  let now=1000;
  const s=setup({enabled:true,wake_enabled:true,wake_ready:true,dialog_active:true,listening:true,state:'listening',silence_remaining_ms:5001},
    {Date:{now:()=>now},t:key=>key==='xiaozhi.hint_timeout'?'Returning in %u s':key});
  await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_listening');
  s.model.silence_remaining_ms=5000;await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'Returning in 5 s');
  now+=20000;s.tick(100);assert.equal(s.node('xiaozhiConversationHint').textContent,'Returning in 5 s');
  s.model.silence_remaining_ms=1001;await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'Returning in 2 s');
  s.model.silence_remaining_ms=1;await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'Returning in 1 s');
  s.model.silence_remaining_ms=30000;await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_listening');
  s.model.speaking=true;s.model.duplex_active=true;s.model.state='speaking_listening';
  await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_duplex');
  assert.equal(s.requests.length,0);
});

test('manual countdown names the press-to-talk action even with wake detection available',async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:true,wake_ready:true,dialog_active:true,dialog_voice_opened:false,
    silence_remaining_ms:5000,state:'ready'}, {t:key=>key==='xiaozhi.hint_timeout_manual'?'Returning in %u s - Hold to talk':key});
  await s.poll();assert.equal(s.node('xiaozhiTalk').hidden,false);
  assert.equal(s.node('xiaozhiConversationHint').textContent,'Returning in 5 s - Hold to talk');
  s.model.silence_remaining_ms=30000;await s.poll();
  assert.doesNotMatch(s.node('xiaozhiConversationHint').textContent,/Returning/);
  s.model.dialog_voice_opened=true;s.model.silence_remaining_ms=5000;await s.poll();
  assert.equal(s.node('xiaozhiTalk').hidden,true);
  assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_timeout');
});

test('farewell, silence and peer close hide only the conversation and clear stale content',async()=>{
  for(const reason of ['farewell','silence','peer_close','manual']) {
    const s=setup({enabled:true,paired:true,dialog_active:true,conversation:true,listening:true,state:'listening',recognized:'bye bye',reply:'Old response',
      spectrum:{microphone:Array(24).fill(255),mic_active:true}});
    await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,false);
    assert.equal(s.node('xiaozhiSpectrum').children[0].style.height,'64px');
    Object.assign(s.model,{dialog_active:false,dialog_end_seq:1,dialog_end_reason:reason,conversation:false,listening:false,state:'ready'});
    await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,true);
    assert.equal(s.node('xiaozhiFields').hidden,false);assert.equal(s.node('xiaozhiTalk').disabled,false);
    assert.equal(s.node('xiaozhiText').textContent,'');assert.equal(s.node('xiaozhiRecognized').textContent,'');
    assert.equal(s.node('xiaozhiOtto').getAttribute('src'),undefined);
    assert.ok(s.node('xiaozhiSpectrum').children.every(b=>b.style.height==='4px'));
    assert.equal(s.node('xiaozhiStatus').hidden,false);assert.equal(s.node('xiaozhiStatus').textContent,'ready');
  }
});

test('real connection faults keep recovery guidance without spectrum or a stale timeout',async()=>{
  for(const state of ['connection_error','audio_or_connection_error']) {
    const s=setup({enabled:true,paired:true,wake_enabled:true,wake_ready:true,dialog_active:false,dialog_end_seq:1,dialog_end_reason:'error',
      state,silence_remaining_ms:1000,recognized:'bye bye',reply:'stale reply'});
    await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,false);
    assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_error');
    assert.equal(s.node('xiaozhiText').textContent,'');assert.equal(s.node('xiaozhiSpectrum').hidden,true);
    assert.equal(s.node('xiaozhiConversationState').textContent,state);
  }
});

test('ending a failed dialogue stays closed after acknowledgment and reload while diagnostics remain',async()=>{
  const s=setup({enabled:true,paired:true,wake_enabled:false,dialog_active:false,dialog_end_seq:1,
    dialog_end_reason:'error',state:'connection_error',connection_errors:2,connection_end:'transport_error'});
  await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,false);
  await s.fire('xiaozhiStop','click');await s.poll();await s.poll();
  assert.equal(s.node('xiaozhiConversation').hidden,true);
  assert.equal(s.node('xiaozhiStatus').textContent,'ready');
  const reloaded=setup(s.model);await reloaded.poll();
  assert.equal(reloaded.node('xiaozhiConversation').hidden,true);
  assert.equal(reloaded.model.connection_errors,2);assert.equal(reloaded.model.connection_end,'transport_error');
  await reloaded.fire('xiaozhiTalk','pointerdown');await reloaded.poll();
  assert.equal(reloaded.node('xiaozhiConversation').hidden,false);
  assert.equal(reloaded.model.dialog_seq,2);
  await reloaded.events.pointerup();
});

test('ended generations cannot reopen and a late old close cannot dismiss a new dialogue',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,dialog_seq:7,listening:true,state:'listening',reply:'First'});
  await s.poll();
  Object.assign(s.model,{dialog_active:false,dialog_end_seq:7,dialog_end_reason:'farewell',listening:false,state:'ready'});
  await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,true);
  Object.assign(s.model,{dialog_active:true,dialog_end_seq:0,listening:true,state:'listening'});
  await s.poll();assert.equal(s.node('xiaozhiConversation').hidden,true);
  Object.assign(s.model,{dialog_seq:8,reply:'New conversation'});await s.poll();
  assert.equal(s.node('xiaozhiConversation').hidden,false);assert.equal(s.node('xiaozhiText').textContent,'New conversation');
  Object.assign(s.model,{dialog_seq:7,dialog_active:false,dialog_end_seq:7,state:'ready'});await s.poll();
  assert.equal(s.node('xiaozhiConversation').hidden,false);assert.equal(s.node('xiaozhiText').textContent,'New conversation');
});

test('manual end carries the displayed generation and rollback retains disconnect compatibility',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,dialog_seq:0xffffffff,state:'listening',listening:true});
  await s.poll();await s.fire('xiaozhiStop','click');
  assert.deepEqual(s.requests.at(-1),{action:'dismiss',value:0xffffffff,epoch:197});
  Object.assign(s.model,{dialog_active:true,dialog_seq:0,dialog_end_seq:0xffffffff});await s.poll();
  assert.equal(s.node('xiaozhiConversation').hidden,false);
  await s.fire('xiaozhiStop','click');assert.deepEqual(s.requests.at(-1),{action:'dismiss',value:0,epoch:197});
  const old=setup({enabled:true,paired:true,listening:true,state:'listening',dialog_seq:undefined,dialog_active:undefined});
  await old.poll();await old.fire('xiaozhiStop','click');assert.deepEqual(old.requests.at(-1),{action:'disconnect',value:0});
});

test('release while a manual dialogue is opening never starts capture afterward',async()=>{
  let resolve;const requests=[];
  const s=setup({enabled:true,paired:true},{fetch:async(_url,options)=>{
    requests.push(JSON.parse(options.body));return new Promise(r=>resolve=r);
  }});
  await s.poll();const press=s.fire('xiaozhiTalk','pointerdown');await Promise.resolve();
  await s.events.pointerup();
  Object.assign(s.model,{dialog_active:true,dialog_seq:2});resolve({ok:true});await press;await s.poll();
  assert.deepEqual(requests,[{action:'dialog_turn',value:1,epoch:197}]);
});

test('an intervening wake cannot inherit a pending manual press or its release',async()=>{
  const requests=[];
  const s=setup({enabled:true,paired:true},{fetch:async(_url,options)=>{
    requests.push(JSON.parse(options.body));return {ok:true};
  }});
  await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  Object.assign(s.model,{dialog_active:true,dialog_seq:2,dialog_voice_opened:true,listening:true,state:'listening'});
  await s.poll();await s.events.pointerup();
  assert.deepEqual(requests,[{action:'dialog_turn',value:1,epoch:197}]);
});

test('a completed manual generation cannot stop a subsequent wake on late release',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,dialog_seq:8});
  await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  Object.assign(s.model,{dialog_active:true,dialog_seq:9,dialog_end_seq:8,dialog_voice_opened:true});
  await s.poll();await s.events.pointerup();
  assert.deepEqual(s.requests,[{action:'start_turn',value:8,epoch:197}]);
});

test('a restart accepts the new boot generation and cancels the old manual release',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,dialog_seq:37});
  await s.poll();await s.fire('xiaozhiTalk','pointerdown');
  Object.assign(s.model,{dialog_epoch:248,dialog_seq:0,dialog_active:false,dialog_end_seq:0,state:'ready'});
  await s.poll();await s.events.pointerup();
  assert.equal(s.node('xiaozhiConversation').hidden,true);
  Object.assign(s.model,{dialog_seq:1,dialog_active:true,dialog_voice_opened:true,reply:'After reboot'});
  await s.poll();assert.equal(s.node('xiaozhiText').textContent,'After reboot');
  await s.fire('xiaozhiStop','click');
  assert.deepEqual(s.requests,[{action:'start_turn',value:37,epoch:197},{action:'dismiss',value:1,epoch:248}]);
});

test('failed dismissal permits the authoritative live dialogue to reappear',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true,reply:'Still connected'}, {fetch:async()=>({ok:false})});
  await s.poll();await s.fire('xiaozhiStop','click');await s.poll();
  assert.equal(s.node('xiaozhiConversation').hidden,false);assert.equal(s.node('xiaozhiText').textContent,'Still connected');
});

test('rollback retains manual start and release for images without the generation contract',async()=>{
  const s=setup({enabled:true,paired:true,dialog_seq:undefined,dialog_epoch:undefined,dialog_active:undefined});
  await s.poll();await s.fire('xiaozhiTalk','pointerdown');await s.events.pointerup();
  assert.deepEqual(s.requests,[{action:'start',value:0},{action:'stop',value:0}]);
});

test('no unguarded start or dismissal is sent before the first status identifies the API',async()=>{
  const s=setup({enabled:true,paired:true});
  await s.fire('xiaozhiTalk','pointerdown');await s.events.pointerup();await s.fire('xiaozhiStop','click');
  assert.deepEqual(s.requests,[]);
});

test('manual fallback can open after an ended voice-opened dialogue',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:false,dialog_voice_opened:true,dialog_end_seq:1,
    dialog_end_reason:'error',wake_enabled:true,wake_ready:false,state:'connection_error'});
  await s.poll();await s.fire('xiaozhiTalk','pointerdown');await s.events.pointerup();
  assert.deepEqual(s.requests.map(r=>r.action),['dialog_turn','start_turn','stop_turn']);
});

test('manual reopening immediately after End uses a new generation',async()=>{
  const s=setup({enabled:true,paired:true,dialog_active:true});
  await s.poll();await s.fire('xiaozhiStop','click');
  await s.fire('xiaozhiTalk','pointerdown');await s.events.pointerup();
  assert.deepEqual(s.requests,[{action:'dismiss',value:1,epoch:197},{action:'dialog_turn',value:1,epoch:197},
    {action:'start_turn',value:2,epoch:197},{action:'stop_turn',value:2,epoch:197}]);
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

test('spectrum uses measured playback bands before microphone and expires old snapshots', async()=>{
  let now=1000;
  const mic=Array(24).fill(0), speaker=Array(24).fill(0); mic[2]=255;speaker[17]=255;
  const s=setup({enabled:true,conversation:true,speaking:true,listening:true,state:'speaking_listening',
    spectrum:{microphone:mic,speaker,mic_active:true,speaker_active:true}}, {Date:{now:()=>now}});
  await s.poll();
  const bars=s.node('xiaozhiSpectrum').children;
  assert.equal(bars.length,24);
  assert.equal(bars[17].style.height,'64px');assert.equal(bars[2].style.height,'4px');
  now=1349;s.tick(100);assert.equal(bars[17].style.height,'64px');
  now=1350;s.tick(100);assert.ok(bars.every(b=>b.style.height==='4px'));
  s.model.spectrum.speaker_active=false;await s.poll();
  assert.equal(bars[2].style.height,'64px');assert.equal(bars[17].style.height,'4px');
});

test('silence, invalid spectrum, errors, goodbye and hiding cannot leave moving bars', async()=>{
  const s=setup({enabled:true,conversation:true,listening:true,state:'listening',
    spectrum:{microphone:Array(24).fill(255),mic_active:true}});
  const bars=s.node('xiaozhiSpectrum').children;
  const quiet=()=>assert.ok(bars.every(b=>b.style.height==='4px'));
  await s.poll();assert.equal(bars[0].style.height,'64px');
  s.model.spectrum.microphone=Array(24).fill(0);await s.poll();quiet();
  for(const data of [Array(23).fill(255),Array(24).fill(NaN),Array(24).fill(-1),Array(24).fill(256)]) {
    s.model.spectrum.microphone=data;await s.poll();quiet();
  }
  s.model.spectrum.microphone=Array(24).fill(255);
  for(const state of ['connection_error','audio_or_connection_error','ready','disabled']) {
    s.model.state='listening';await s.poll();assert.equal(bars[0].style.height,'64px');
    s.model.state=state;await s.poll();quiet();
  }
  s.model.state='listening';await s.poll();s.document.hidden=true;s.events.visibilitychange();quiet();
  s.document.hidden=false;await s.poll();s.node('settingsXiaozhiSection').classList.contains=()=>true;s.tick(100);quiet();
});

test('active polls are bounded, leave settings idle polls slow, and command failures clear spectrum', async()=>{
  let calls=0, reject;
  const s=setup({enabled:true,conversation:true,listening:true,state:'listening',
    spectrum:{microphone:Array(24).fill(255),mic_active:true}},
    {apiGet:async()=>{calls++;return {...s.model};}});
  await s.tick(1000);assert.equal(calls,1);
  await s.tick(1000);assert.equal(calls,1);
  await s.tick(200);assert.equal(calls,2);
  s.model.conversation=false;s.model.listening=false;s.model.state='ready';await s.tick(200);
  const idleCalls=calls;await s.tick(200);assert.equal(calls,idleCalls);
  await s.tick(1000);assert.equal(calls,idleCalls+1);
  const f=setup({enabled:true,conversation:true,listening:true,state:'listening',
    spectrum:{microphone:Array(24).fill(255),mic_active:true}}, {fetch:async()=>({ok:false})});
  await f.poll();await f.fire('xiaozhiStop','click');
  assert.ok(f.node('xiaozhiSpectrum').children.every(b=>b.style.height==='4px'));
});

test('bubble labels, visibility and hints follow actual duplex and wake availability', async()=>{
  const s=setup({enabled:true,recognized:'<b>hello</b>',reply:'A long response',duplex_active:true,
    speaking:true,listening:true,state:'speaking_listening',wake_enabled:true,wake_ready:true});
  await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_duplex');
  assert.equal(s.node('xiaozhiUserBubble').hidden,false);assert.equal(s.node('xiaozhiReplyBubble').hidden,false);
  assert.equal(s.node('xiaozhiRecognized').innerHTML,undefined);
  s.model.state='ready';s.model.listening=false;s.model.speaking=false;await s.poll();
  assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_ready');
  s.model.state='connection_error';await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_error');
  s.model.wake_ready=false;await s.poll();assert.equal(s.node('xiaozhiConversationHint').textContent,'xiaozhi.hint_manual_error');
  s.model.recognized='';s.model.reply='';await s.poll();
  assert.equal(s.node('xiaozhiUserBubble').hidden,true);assert.equal(s.node('xiaozhiReplyBubble').hidden,true);
});

test('late HTTP spectrum is discarded and unchanged live text is not announced again', async()=>{
  let now=1000, resolve;
  const s=setup({enabled:true,conversation:true,listening:true,state:'listening',recognized:'Hello',reply:'Hi',
    spectrum:{microphone:Array(24).fill(255),mic_active:true}},
    {Date:{now:()=>now},apiGet:()=>new Promise(r=>resolve=r)});
  const pending=s.tick(1000);now=1400;resolve({...s.model});await pending;
  assert.ok(s.node('xiaozhiSpectrum').children.every(b=>!b.style.height || b.style.height==='4px'));
  const writes=[];
  for(const id of ['xiaozhiStatus','xiaozhiText','xiaozhiRecognized','xiaozhiConversationHint']) {
    let value=s.node(id).textContent;
    Object.defineProperty(s.node(id),'textContent',{get:()=>value,set:text=>{writes.push(id);value=text;}});
  }
  const fresh=s.tick(200);resolve({...s.model});await fresh;
  assert.equal(s.node('xiaozhiSpectrum').children[0].style.height,'64px');
  assert.deepEqual(writes,[]);
});
