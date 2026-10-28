/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
// Saved camera configuration is separate from Display automation and layout.
const cameraEditor = (() => {
  const node = id => document.getElementById(id);
  const tr = key => t(`cam.${key}`);
  const clone = value => JSON.parse(JSON.stringify(value));
  let draft, saved, defaults, supported = false, motionSupported = false;
  let busy = false, loaded = false, dirty = false, polling = false;
  let previewUntil = 0, previewId = "local", previewController, previewUrl, previewTimer, previewDeadline;
  let previewSession = 0;
  let outputRotation = 0, previewRotation = 0;
  const angle = value => [0,90,180,270].includes(Number(value)) ? Number(value) : 0;
  function rotateZone(z, rotation) {
    if(rotation===90)return {x:100-z.y-z.h,y:z.x,w:z.h,h:z.w};
    if(rotation===180)return {x:100-z.x-z.w,y:100-z.y-z.h,w:z.w,h:z.h};
    if(rotation===270)return {x:z.y,y:100-z.x-z.w,w:z.h,h:z.w};
    return {...z};
  }
  function resolutionLabels() {
    const choices=node("cam_resolution");if(!choices)return;
    const sizes=outputRotation%180?[[640,800],[320,400]]:[[800,640],[400,320]];
    [...choices.children].forEach((option,i)=>{option.textContent=sizes[i].join(" × ");});
  }
  const numbers = {
    image: [["brightness",-128,127],["contrast",0,255],["saturation",0,255],["hue",0,360],
      ["wb_red",25,400],["wb_blue",25,400],["sharpen",25,300],["denoise",25,200],
      ["tone_shadows",-100,100],["tone_highlights",-100,100]],
    motion: [["threshold",1,64],["min_area",0,100],["min_duration_ms",0,1000],
      ["cooldown_ms",0,30000],["start_delay_ms",0,10000]],
  };
  const message = text => { node("camMessage").textContent = text; };
  function changed() { dirty = true; message(tr("unsaved")); }
  function element(tag, text, parent, className) {
    const e = document.createElement(tag); if (text !== undefined) e.textContent = text;
    if (className) e.className = className; if (parent) parent.appendChild(e); return e;
  }
  function button(parent, text, fn) {
    const b=element("button",text,parent,"btn small");b.type="button";b.addEventListener("click",fn);return b;
  }
  function field(parent, key, value, change, options={}) {
    const label=element("label",undefined,parent,options.type==="checkbox"?"row":"cam-field");
    const title=element("span",tr(key),label);
    const input=element(options.choices?"select":"input",undefined,label);
    input.id=options.id || "cam_"+key;
    if(options.choices) for(const [v,name] of options.choices){const o=element("option",name,input);o.value=String(v);}
    else input.type=options.type || "number";
    if(options.min!==undefined)input.min=String(options.min);
    if(options.max!==undefined)input.max=String(options.max);
    if(options.maxLength)input.maxLength=options.maxLength;
    if(input.type==="checkbox")input.checked=!!value;
    else input.value=value===undefined?"":String(value);
    label.htmlFor=input.id;title.id=input.id+"Label";
    input.addEventListener(input.type==="checkbox"||options.choices?"change":"input",()=>{
      const v=input.type==="checkbox"?input.checked:input.type==="number"?Number(input.value):input.value;
      change(v);changed();visibility();
    });return input;
  }
  async function request(url, method="GET", body) {
    const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),12000);
    try {
      const r=await fetch(url,{method,cache:"no-store",signal:controller.signal,
        ...(method==="POST"||method==="PUT"?{headers:{"Content-Type":"application/json"}}:{}),
        ...(body===undefined?{}:{body:JSON.stringify(body)})});
      if(!r.ok)throw new Error((await r.text()).slice(0,200));return await r.json();
    } finally {clearTimeout(timeout);}
  }
  function visibility() {
    if(!draft)return;
    for(const input of node("settingsCameraSection").querySelectorAll("input, select, button"))input.disabled=busy;
    node("camOutputFields").hidden=!supported||!draft.local.enabled;
    node("camImageFields").hidden=!supported||!draft.local.enabled;
    node("camMotionFields").hidden=!motionSupported||!draft.local.motion.enabled;
    node("camOutputUnavailable").hidden=supported;
    node("cam_enabled").disabled=!supported||busy;
    node("cam_motion_enabled").disabled=!motionSupported||busy;
    node("camStreamAddress").hidden=!draft.local.enabled||!draft.local.stream_enabled;
    node("camPreview").disabled=busy||dirty||!saved?.local.enabled||!supported;
    node("camPreviewStop").disabled=!previewUntil;
    node("camMotionTest").disabled=busy||!motionSupported;
    node("camSave").disabled=busy||!loaded;
    node("camReload").disabled=busy;
    node("camReset").disabled=busy||!loaded;
    if(!draft.local.enabled&&previewId==="local")stopPreview();
  }
  function renderZones() {
    const root=node("camZones");root.replaceChildren();
    draft.local.motion.zones.forEach((z,i)=>{
      const row=element("div",undefined,root,"cam-zone-row");element("strong",String(i+1),row);
      for(const key of ["x","y","w","h"])field(row,key,z[key],v=>{z[key]=v;drawZones();},
        {id:`cam_zone_${i}_${key}`,min:key==="w"||key==="h"?1:0,max:100});
      button(row,tr("remove"),()=>{draft.local.motion.zones.splice(i,1);changed();renderZones();});
    });
    if(draft.local.motion.zones.length<4)button(root,tr("add_zone"),()=>{
      draft.local.motion.zones.push({x:0,y:0,w:100,h:100});changed();renderZones();
    });drawZones();
  }
  function drawZones() {
    const layer=node("camZoneOverlay");layer.replaceChildren();
    if(!draft?.local.motion.enabled||previewId!=="local")return;
    draft.local.motion.zones.forEach((zone,i)=>{
      const z=rotateZone(zone,previewRotation);
      const box=element("div",String(i+1),layer,"cam-zone-box");
      Object.assign(box.style,{left:z.x+"%",top:z.y+"%",width:z.w+"%",height:z.h+"%"});
    });
  }
  function addTile(id,name) {
    if(dirty){message(tr("save_first"));return;}
    const widget=addWidget("camera_tile",{entityId:"",title:name,cameraSource:id});
    if(widget){setActivePane("layout");message(tr("layout_save"));}
  }
  function renderSources() {
    const root=node("camSources");root.replaceChildren();
    draft.sources.forEach((s,i)=>{
      const row=element("div",undefined,root,"cam-source");
      element("h4",s.name||tr("source")+" "+(i+1),row);
      const form=element("div",undefined,row,"cam-grid");
      const base={id:"cam_source_"+i};
      for(const [key,max] of [["id",23],["name",63]])field(form,key,s[key],v=>{s[key]=v;},{type:"text",maxLength:max,id:base.id+"_"+key});
      field(form,"source",s.source,v=>{s.source=v;renderSources();},{id:base.id+"_source",choices:[["ha",tr("ha")],["http",tr("http")]]});
      field(form,"source_enabled",s.enabled,v=>{s.enabled=v;},{type:"checkbox",id:base.id+"_enabled"});
      field(form,"refresh_ms",s.refresh_ms,v=>{s.refresh_ms=v;},{min:1000,max:3600000,id:base.id+"_refresh"});
      const details=element("div",undefined,row,"cam-grid");
      const fields=s.source==="ha"?[["entity_id",127]]:[["snapshot_url",383],["username",63],["password",95]];
      for(const [key,max] of fields){
        const input=field(details,key,s[key],v=>{s[key]=v;},{type:key==="password"?"password":"text",maxLength:max,id:base.id+"_"+key});
        if(key==="password")input.autocomplete="new-password";
      }
      if(s.source==="http"&&s.password_set)element("p",tr("password_saved"),row,"meta");
      const actions=element("div",undefined,row,"row");
      button(actions,tr("preview"),()=>{if(dirty)message(tr("save_first"));else startPreview(s.id);});
      button(actions,tr("add_tile"),()=>addTile(s.id,s.name));
      button(actions,tr("remove"),()=>{draft.sources.splice(i,1);changed();renderSources();});
    });
    if(draft.sources.length<4)button(root,tr("add_source"),()=>{
      let n=1;while(draft.sources.some(s=>s.id==="camera"+n))n++;
      draft.sources.push({id:"camera"+n,name:"",source:"http",snapshot_url:"",entity_id:"",username:"",enabled:false,refresh_ms:10000});
      changed();renderSources();
    });
  }
  function render() {
    const root=node("camForm");root.replaceChildren();
    field(root,"enabled",draft.local.enabled,v=>{draft.local.enabled=v;},{type:"checkbox",id:"cam_enabled"});
    const out=element("div",undefined,root,"cam-grid");out.id="camOutputFields";
    field(out,"stream_enabled",draft.local.stream_enabled,v=>{draft.local.stream_enabled=v;},{type:"checkbox"});
    field(out,"resolution",draft.local.resolution,v=>{draft.local.resolution=Number(v);},{choices:[[0,"800 × 640"],[1,"400 × 320"]]});
    resolutionLabels();element("p",tr("rotation_hint"),out,"hint");
    field(out,"jpeg_quality",draft.local.jpeg_quality,v=>{draft.local.jpeg_quality=v;},{min:10,max:95});
    for(const key of ["hflip","vflip"])field(out,key,draft.local[key],v=>{draft.local[key]=v;},{type:"checkbox"});
    const image=element("details",undefined,root);image.id="camImageFields";element("summary",tr("image"),image);
    const imageGrid=element("div",undefined,image,"cam-grid");
    for(const [key,min,max] of numbers.image)field(imageGrid,key,draft.local.image[key],v=>{draft.local.image[key]=v;},{min,max});
    element("h3",tr("motion"),root);
    field(root,"motion_enabled",draft.local.motion.enabled,v=>{draft.local.motion.enabled=v;drawZones();},{type:"checkbox",id:"cam_motion_enabled"});
    const motion=element("div",undefined,root);motion.id="camMotionFields";
    element("p",tr("motion_hint"),motion,"meta");
    const motionGrid=element("div",undefined,motion,"cam-grid");
    for(const [key,min,max] of numbers.motion)field(motionGrid,key,draft.local.motion[key],v=>{draft.local.motion[key]=v;},{min,max});
    field(motion,"ignore_lighting",draft.local.motion.ignore_lighting,v=>{draft.local.motion.ignore_lighting=v;},{type:"checkbox"});
    element("p",tr("zones_hint"),motion,"meta");
    element("div",undefined,motion).id="camZones";
    renderZones();renderSources();visibility();
  }
  function accept(data) {
    supported=!!data.supported;motionSupported=!!data.motion_supported;defaults=data.defaults;
    outputRotation=angle(data.rotation_degrees);
    saved={local:clone(data.local),sources:clone(data.sources)};draft=clone(saved);
    loaded=true;dirty=false;render();
  }
  function validate() {
    const bad=[...node("settingsCameraSection").querySelectorAll("input")].find(input=>!input.checkValidity());
    if(bad){bad.reportValidity();throw new Error(tr("invalid"));}
    if(draft.local.motion.zones.some(z=>z.x+z.w>100||z.y+z.h>100))throw new Error(tr("invalid_zone"));
  }
  async function action(fn) {
    if(busy)return;busy=true;visibility();
    try{await fn();}catch(e){message(tr("failed")+": "+e.message);}finally{busy=false;visibility();}
  }
  function stopPreview() {
    previewSession++;previewUntil=0;clearTimeout(previewTimer);clearTimeout(previewDeadline);previewController?.abort();previewController=null;
    const img=node("camPreviewImage");img.removeAttribute("src");img.hidden=true;
    if(previewUrl)URL.revokeObjectURL(previewUrl);previewUrl=null;
    node("camPreviewStop").disabled=true;
  }
  async function previewTick() {
    if(!previewUntil||Date.now()>=previewUntil||!visible()){stopPreview();return;}
    const session=previewSession;
    const controller=new AbortController();previewController=controller;
    const timeout=setTimeout(()=>controller.abort(),7000);
    try{
      const r=await fetch("/api/camera/snapshot?id="+encodeURIComponent(previewId),{cache:"no-store",signal:controller.signal});
      if(!r.ok)throw new Error((await r.text()).slice(0,160));
      const blob=await r.blob();
      if(controller.signal.aborted||session!==previewSession||!previewUntil||Date.now()>=previewUntil)return;
      if(previewUrl)URL.revokeObjectURL(previewUrl);previewUrl=URL.createObjectURL(blob);
      previewRotation=previewId==="local"?angle(r.headers?.get?.("X-Camera-Rotation")):0;
      if(previewId==="local")node("camPreviewFrame").style.aspectRatio=previewRotation%180?"4 / 5":"5 / 4";
      node("camPreviewImage").src=previewUrl;node("camPreviewImage").hidden=false;drawZones();
    }catch(e){if(!controller.signal.aborted)message(tr("failed")+": "+e.message);}
    finally{clearTimeout(timeout);if(previewController===controller)previewController=null;}
    if(previewUntil&&session===previewSession)previewTimer=setTimeout(previewTick,500);
  }
  function startPreview(id="local") {
    if(busy||dirty){message(tr("save_first"));return;}
    stopPreview();previewId=id;previewUntil=Date.now()+30000;node("camPreviewStop").disabled=false;
    previewDeadline=setTimeout(stopPreview,30000);previewTick();
  }
  function visible(){return !document.hidden&&node("settingsCameraSection")&&!node("settingsCameraSection").classList.contains("hidden");}
  async function poll() {
    if(!visible()){if(previewUntil)stopPreview();return;}
    if(busy||polling)return;polling=true;
    try{
      if(!loaded){const data=await request("/api/camera/config");if(!busy)accept(data);}
      const s=await request("/api/camera/status");
      outputRotation=angle(s.rotation_degrees);resolutionLabels();
      node("camStatus").textContent=(s.local_only?tr("local_simulation")+" · ":"")+tr(s.failed?"failed":s.ready?"ready":"waiting")+
        " · "+tr("level")+": "+(s.valid?s.level:"—")+" · "+tr("changed")+": "+(s.valid?s.changed_pct+"%":"—")+
        " · "+tr("triggers")+": "+s.trigger_count+(s.ignored_lighting?" · "+tr("ignored_lighting"):"")+
        (s.test_active?" · "+tr("test_remaining")+": "+s.test_remaining_s+"s":"");
      node("camZoneStatus").textContent=(s.zones||[]).map((z,i)=>`${i+1}: ${z.level} / ${z.changed_pct}%`).join(" · ");
    }catch(e){message(tr("failed")+": "+e.message);}finally{polling=false;visibility();}
  }
  function bind() {
    if(!node("camSave"))return;
    node("camSave").addEventListener("click",()=>action(async()=>{validate();stopPreview();accept(await request("/api/camera/config","PUT",draft));message(tr("saved"));}));
    node("camReload").addEventListener("click",()=>action(async()=>{
      if(dirty&&!window.confirm(tr("discard")))return;stopPreview();accept(await request("/api/camera/config"));message("");
    }));
    node("camReset").addEventListener("click",()=>{if(!defaults||busy)return;stopPreview();draft=clone(defaults);changed();render();});
    node("camPreview").addEventListener("click",()=>startPreview());
    node("camPreviewStop").addEventListener("click",stopPreview);
    node("camPreviewImage").addEventListener("load",()=>{
      const img=node("camPreviewImage");
      if(img.naturalWidth&&img.naturalHeight)node("camPreviewFrame").style.aspectRatio=img.naturalWidth+" / "+img.naturalHeight;
    });
    node("camLocalTile").addEventListener("click",()=>addTile("local",tr("local")));
    node("camMotionTest").addEventListener("click",()=>action(()=>request("/api/camera/test","POST")));
    node("camStreamAddress").textContent=location.origin+"/api/camera/stream";
    document.addEventListener("visibilitychange",()=>{if(document.hidden)stopPreview();});
    window.addEventListener("pagehide",stopPreview);
    const overlay=node("camZoneOverlay");let start;
    overlay.addEventListener("pointerdown",e=>{
      if(busy||!previewUntil||!draft?.local.motion.enabled||previewId!=="local"||draft.local.motion.zones.length>=4)return;
      const r=overlay.getBoundingClientRect();start={rotation:previewRotation,x:Math.max(0,Math.min(99,Math.round((e.clientX-r.left)*100/r.width))),y:Math.max(0,Math.min(99,Math.round((e.clientY-r.top)*100/r.height)))};
      overlay.setPointerCapture(e.pointerId);
    });
    overlay.addEventListener("pointerup",e=>{
      if(!start)return;if(start.rotation!==previewRotation){start=null;return;}const r=overlay.getBoundingClientRect();
      const x=Math.max(0,Math.min(100,Math.round((e.clientX-r.left)*100/r.width))),y=Math.max(0,Math.min(100,Math.round((e.clientY-r.top)*100/r.height)));
      const z={x:Math.min(start.x,x),y:Math.min(start.y,y),w:Math.abs(x-start.x),h:Math.abs(y-start.y)};start=null;
      if(z.w&&z.h){draft.local.motion.zones.push(rotateZone(z,(360-previewRotation)%360));changed();renderZones();}
    });
    overlay.addEventListener("pointercancel",()=>{start=null;});
    setInterval(poll,1500);
  }
  bind();
  return {poll,stopPreview,request};
})();
