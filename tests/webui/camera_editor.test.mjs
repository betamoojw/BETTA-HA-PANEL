// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source=fs.readFileSync(new URL('../../components/webui/www/js/camera-editor.js',import.meta.url),'utf8');
const defaults={local:{enabled:false,stream_enabled:false,hflip:false,vflip:false,resolution:0,jpeg_quality:55,
  image:{brightness:0,contrast:128,saturation:128,hue:0,wb_red:100,wb_blue:100,sharpen:100,denoise:100,tone_shadows:0,tone_highlights:0},
  motion:{enabled:false,ignore_lighting:true,threshold:8,min_area:0,min_duration_ms:0,cooldown_ms:1000,start_delay_ms:2000,zones:[]}},sources:[]};
function setup(initial=defaults,overrides={}) {
  const nodes=new Map(), requests=[], timers=new Map(), tiles=[];
  let timerId=0,clock=0,poll,rotation=0;
  const model=structuredClone(initial);
  function element(tag='div',id='') {
    let identity=id;
    const e={tag,type:'',value:'',checked:false,disabled:false,hidden:false,children:[],style:{},listeners:{},
      classList:{contains:()=>false},addEventListener(k,fn){this.listeners[k]=fn;},
      appendChild(child){this.children.push(child);},replaceChildren(){this.children=[];},
      removeAttribute(k){delete this[k];},checkValidity(){return this.type!=='number'||(Number.isInteger(+this.value)&&(!this.min||+this.value>=+this.min)&&(!this.max||+this.value<=+this.max));},
      getBoundingClientRect(){return {left:0,top:0,width:100,height:100};},setPointerCapture(){},
      reportValidity(){},querySelectorAll(selector){return this.children.flatMap(c=>[...(selector.split(',').map(s=>s.trim()).includes(c.tag)?[c]:[]),...c.querySelectorAll(selector)]);},
      get id(){return identity;},set id(v){identity=v;nodes.set(v,this);}};
    if(id)nodes.set(id,e);return e;
  }
  const node=id=>nodes.get(id)||element('div',id);
  node('settingsCameraSection').children.push(...['camForm','camSources'].map(node));
  const reply=()=>({supported:true,motion_supported:true,...structuredClone(model),defaults:structuredClone(defaults)});
  const fetch=async(url,options={})=>{
    requests.push({url,...options});
    if(url==='/api/camera/config'&&options.method==='PUT')Object.assign(model,JSON.parse(options.body));
    return {ok:true,headers:{get:()=>String(rotation)},json:async()=>url==='/api/camera/config'?reply():{zones:[],trigger_count:0,rotation_degrees:rotation},blob:async()=>new Blob(['synthetic JPEG'])};
  };
  vm.runInNewContext(source,{document:{getElementById:node,createElement:element,hidden:false,addEventListener(){}},
    window:{confirm:()=>true,addEventListener(){}},location:{origin:'http://local.test'},t:k=>k,
    setInterval:fn=>poll=fn,setTimeout:(fn,ms)=>{timers.set(++timerId,{fn,at:clock+ms});return timerId;},clearTimeout:id=>timers.delete(id),
    Date:{now:()=>clock},AbortController,Blob,URL:{createObjectURL:()=>'/synthetic-blob',revokeObjectURL(){}},fetch,
    addWidget:(type,options)=>{tiles.push({type,options});return {};},setActivePane(){},...overrides});
  return {node,model,requests,tiles,poll:()=>poll(),rotate:value=>rotation=value,
    fire:async(id,event,args={})=>node(id).listeners[event]({target:node(id),...args}),
    async set(id,value){const e=node(id);if(e.type==='checkbox')e.checked=value;else e.value=String(value);await this.fire(id,e.type==='checkbox'||e.tag==='select'?'change':'input');},
    async advance(ms){clock+=ms;for(const [id,t] of [...timers])if(t.at<=clock&&timers.delete(id))await t.fn();await new Promise(r=>setImmediate(r));},
    async button(root,key){const e=node(root).querySelectorAll('button').find(b=>b.textContent===`cam.${key}`);assert.ok(e);return e.listeners.click();}};
}
test('camera outputs default off; enable/save/disable/re-enable/reload retains image and motion values',async()=>{
  const s=setup();await s.poll();assert.equal(s.node('cam_enabled').checked,false);assert.equal(s.node('camOutputFields').hidden,true);
  await s.set('cam_enabled',true);await s.set('cam_jpeg_quality',72);await s.set('cam_motion_enabled',true);
  await s.set('cam_threshold',5);await s.set('cam_stream_enabled',true);await s.fire('camSave','click');
  assert.equal(s.model.local.jpeg_quality,72);assert.equal(s.model.local.motion.threshold,5);
  for(const value of [false,true]){await s.set('cam_enabled',value);await s.fire('camSave','click');assert.equal(s.node('camOutputFields').hidden,!value);}
  await s.fire('camReload','click');assert.equal(s.node('cam_jpeg_quality').value,'72');assert.equal(s.node('cam_stream_enabled').checked,true);
  assert.equal(s.requests.some(r=>r.url.includes('snapshot')),false);
});
test('motion zones use editing events and saved JSON; invalid rectangles cannot be saved',async()=>{
  const s=setup();await s.poll();await s.set('cam_motion_enabled',true);await s.button('camZones','add_zone');
  await s.set('cam_zone_0_w',30);await s.set('cam_zone_0_x',20);await s.fire('camSave','click');
  assert.deepEqual(s.model.local.motion.zones,[{x:20,y:0,w:30,h:100}]);
  await s.set('cam_zone_0_x',90);await s.fire('camSave','click');assert.equal(s.model.local.motion.zones[0].x,20);
  await s.fire('camReload','click');assert.equal(s.node('cam_zone_0_x').value,'20');
});
test('independent sources, omitted password, and source tile survive events and save',async()=>{
  const config=structuredClone(defaults);config.sources=[{id:'door',name:'Door',source:'http',enabled:true,refresh_ms:2000,snapshot_url:'http://camera.test/snapshot',username:'viewer',password_set:true}];
  const s=setup(config);await s.poll();await s.set('cam_source_0_name','Front door');await s.fire('camSave','click');
  assert.equal(s.model.sources[0].name,'Front door');assert.equal('password' in s.model.sources[0],false);
  await s.button('camSources','add_tile');assert.equal(s.tiles[0].options.cameraSource,'door');
  await s.set('cam_source_0_enabled',false);await s.fire('camSave','click');await s.fire('camReload','click');
  assert.equal(s.node('cam_source_0_enabled').checked,false);assert.equal(s.node('cam_source_0_snapshot_url').value,config.sources[0].snapshot_url);
});
test('preview is explicit, stops at 30 seconds despite edits, and stops on disable',async()=>{
  const config=structuredClone(defaults);config.local.enabled=true;
  const s=setup(config);await s.poll();await s.fire('camPreview','click');await s.advance(100);
  assert.equal(s.node('camPreviewImage').hidden,false);
  await s.set('cam_jpeg_quality',70);await s.advance(29900);
  assert.equal(s.node('camPreviewImage').hidden,true);assert.equal(s.node('camPreviewStop').disabled,true);
  await s.fire('camSave','click');await s.fire('camPreview','click');await s.advance(100);
  await s.set('cam_enabled',false);assert.equal(s.node('camPreviewImage').hidden,true);
});
test('page reset is a draft, covers every camera control and never changes Display settings',async()=>{
  const config=structuredClone(defaults);config.local.enabled=true;config.local.image.brightness=25;
  const s=setup(config);await s.poll();await s.fire('camReset','click');assert.equal(s.model.local.enabled,true);
  await s.fire('camSave','click');assert.deepEqual(s.model,defaults);
  assert.ok(s.requests.every(r=>r.url.startsWith('/api/camera/')));
});
test('failed saves leave manual opt-in unsaved and preview unavailable',async()=>{
  const s=setup(defaults,{fetch:async(_u,o)=>o.method==='PUT'?{ok:false,text:async()=>'disk error'}:{ok:true,json:async()=>({supported:true,motion_supported:true,...structuredClone(defaults),defaults,zones:[]})}});
  await s.poll();await s.set('cam_enabled',true);await s.fire('camSave','click');
  assert.equal(s.node('cam_enabled').checked,true);assert.equal(s.node('camPreview').disabled,true);assert.match(s.node('camMessage').textContent,/disk error/);
});
test('motion test does not save the draft or turn image output on',async()=>{
  const s=setup();await s.poll();await s.fire('camMotionTest','click');
  assert.equal(s.requests.at(-1).method,'POST');assert.equal(s.requests.at(-1).body,undefined);assert.equal(s.model.local.enabled,false);
  // The device guard requires a non-simple media type even on an empty POST.
  assert.equal(s.requests.at(-1).headers['Content-Type'],'application/json');
});

test('preview follows frame rotation while stored motion zones stay anchored',async()=>{
  const config=structuredClone(defaults);config.local.enabled=true;config.local.motion.enabled=true;
  config.local.motion.zones=[{x:10,y:20,w:30,h:40}];
  const s=setup(config);await s.poll();
  const boxes=[[10,20,30,40],[40,10,40,30],[60,40,30,40],[20,60,40,30]];
  for(let turn=0;turn<4;turn++){
    s.rotate(turn*90);await s.poll();await s.fire('camPreview','click');await s.advance(100);
    const box=s.node('camZoneOverlay').children[0];
    assert.deepEqual(['left','top','width','height'].map(k=>box.style[k]),boxes[turn].map(v=>v+'%'));
    assert.equal(s.node('camPreviewFrame').style.aspectRatio,turn%2?'4 / 5':'5 / 4');
    assert.equal(s.node('cam_resolution').children[0].textContent,turn%2?'640 × 800':'800 × 640');
    await s.fire('camPreviewStop','click');
  }
  assert.deepEqual(s.model.local.motion.zones,config.local.motion.zones);
  s.rotate(90);await s.fire('camPreview','click');await s.advance(100);
  await s.fire('camZoneOverlay','pointerdown',{clientX:20,clientY:10,pointerId:1});
  await s.fire('camZoneOverlay','pointerup',{clientX:60,clientY:30,pointerId:1});
  await s.fire('camSave','click');
  assert.deepEqual(s.model.local.motion.zones[1],{x:10,y:40,w:20,h:40});
  await s.fire('camReload','click');
  assert.equal(s.node('cam_zone_1_x').value,'10');
});
