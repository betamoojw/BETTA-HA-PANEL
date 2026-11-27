// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import {test} from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import {loadEditor} from "./load_editor.mjs";
const source = fs.readFileSync(new URL("../../components/webui/www/js/knx.js",import.meta.url),"utf8");
const config = {media_type:"knx_ip",media_parameters:{multicast_address:"224.0.23.12",udp_port:3671,transport_mode:"routing",interface_identifier:"KNX-IP-Interface",nat:false},
  physical_address:"15.15.199",comObjects:[{id:"sensor",name:"温度",description:"Living room",group_address:"1/0/1",datapoint_type:"DPT-9.001",readable:true,writable:false},
  {id:"lamp",name:"Lamp",group_address:"1/0/2",datapoint_type:"DPT-1.001",readable:true,writable:true}]};
function setup(initial={}, override={}) {
  const model={configuration:structuredClone(config),enabled:false,...initial}, nodes=new Map(), requests=[], downloads=[];
  let poll;
  const node=id=>{
    if (!nodes.has(id)) nodes.set(id,{id,value:"",checked:false,listeners:{},children:[],classList:{contains:()=>false},
      addEventListener(k,cb){this.listeners[k]=cb;},replaceChildren(){this.children=[];},appendChild(o){this.children.push(o);},
      click(){if(this.download) downloads.push(this.download);}});
    return nodes.get(id);
  };
  const fetch=async(url,options)=>{
    requests.push({url,...options});
    const body=options.body?JSON.parse(options.body):null;
    if(url==="/api/knx/config" && options.method==="PUT") {
      if (body.physical_address === "bad") return {ok:false,text:async()=>"Invalid physical address"};
      model.configuration=body;
    }
    if(url==="/api/knx" && options.method==="PUT") model.enabled=body.enabled;
    if(url==="/api/layout" && options.method==="PUT") model.layout=body;
    if(url.startsWith("/api/knx/objects?")) {
      const offset=Number(new URLSearchParams(url.split("?")[1]).get("offset"));
      const objects=model.configuration.comObjects.slice(offset,offset+20).map(o=>({...o,valid:false,...model.feedback?.[o.id]}));
      return {ok:true,json:async()=>structuredClone({objects})};
    }
    const data=url==="/api/knx/config"?model.configuration:url==="/api/knx"?{
      enabled:model.enabled,state:model.enabled?"running":"disabled",object_count:model.configuration.comObjects?.length || 0
    }:{queued:true};
    return {ok:true,json:async()=>structuredClone(data),text:async()=>typeof data==="string"?data:JSON.stringify(data)};
  };
  const ctx=vm.createContext({document:{getElementById:node,createElement:tag=>({tag,click(){downloads.push(this.download);}}),hidden:false},
    window:{confirm:()=>true},TextEncoder,AbortController,Blob,URL:{createObjectURL:()=>"/download",revokeObjectURL(){}},
    setTimeout,clearTimeout,setInterval:cb=>poll=cb,t:k=>k,fetch,...override});
  vm.runInContext(source,ctx);
  return {node,model,requests,downloads,run:code=>vm.runInContext(code,ctx),poll:()=>poll(),fire:async(id,type)=>node(id).listeners[type]({target:node(id)})};
}
test("load, edit, save, disable/re-enable and reload preserve the whole configuration",async()=>{
  const s=setup(); await s.poll();
  assert.equal(s.node("knxEnabled").checked,false);
  s.node("knxConfig").value=JSON.stringify({...config,physical_address:"1.1.9"});
  await s.fire("knxConfig","input");
  await s.poll(); assert.equal(JSON.parse(s.node("knxConfig").value).physical_address,"1.1.9");
  await s.fire("knxSave","click"); assert.equal(s.model.configuration.physical_address,"1.1.9");
  for(const enabled of [true,false,true]) {
    s.node("knxEnabled").checked=enabled;await s.fire("knxEnabled","change");
    assert.equal(s.model.enabled,enabled);assert.equal(s.node("knxOperations").hidden,!enabled);
  }
  const reload=setup(s.model);await reload.poll();
  assert.equal(reload.node("knxEnabled").checked,true);
  assert.deepEqual(JSON.parse(reload.node("knxConfig").value),s.model.configuration);
});
test("import is a draft until save; export uses knxConfig.json",async()=>{
  const s=setup();await s.poll();
  s.node("knxFile").files=[{size:200,text:async()=>JSON.stringify({...config,physical_address:"2.2.2"})}];
  await s.fire("knxFile","change");
  assert.equal(s.model.configuration.physical_address,"15.15.199");
  await s.fire("knxExport","click");assert.deepEqual(s.downloads,["knxConfig.json"]);
  await s.fire("knxSave","click");assert.equal(s.model.configuration.physical_address,"2.2.2");
});
test("oversized/invalid imports and invalid edits never send a configuration write",async()=>{
  const s=setup();await s.poll();
  s.node("knxFile").files=[{size:262145,text:async()=>JSON.stringify(config)}];await s.fire("knxFile","change");
  s.node("knxConfig").value="[]";await s.fire("knxSave","click");
  assert.equal(s.requests.filter(r=>r.method==="PUT").length,0);
});
test("512 distinct objects survive import, format, export, save, reload and last-object selection",async()=>{
  const s=setup({enabled:true});await s.poll();
  const comObjects=Array.from({length:512},(_,i)=>({...config.comObjects[0],id:"point-"+i,
    group_address:`1/${Math.floor(i/256)}/${i%256}`,description:"窗".repeat(60)}));
  const registry={...config,comObjects},text=JSON.stringify(registry);
  assert.ok(Buffer.byteLength(text)>65535);
  s.node("knxFile").files=[{size:Buffer.byteLength(text),text:async()=>text}];
  await s.fire("knxFile","change");await s.fire("knxFormat","click");await s.fire("knxExport","click");
  assert.deepEqual(s.downloads,["knxConfig.json"]);await s.fire("knxSave","click");
  assert.deepEqual(s.model.configuration,registry);
  const reopened=setup(s.model);await reopened.poll();
  assert.equal(reopened.node("knxObject").children.length,512);
  reopened.node("knxObject").value="511";await reopened.fire("knxObject","change");
  await reopened.fire("knxRefresh","click");assert.equal(reopened.requests.at(-1).url,"/api/knx/objects?offset=500");
  assert.match(reopened.node("knxCache").textContent,/point-511/);
  for(const invalid of [JSON.stringify({...registry,comObjects:[...comObjects,comObjects[0]]}),
                        text+" ".repeat(262145-Buffer.byteLength(text))]){
    reopened.node("knxConfig").value=invalid;await reopened.fire("knxSave","click");
    assert.equal(reopened.requests.filter(r=>r.method==="PUT").length,0);
  }
  // An accepted near-limit compact registry stays saveable after reload/Format.
  const near={...config,comObjects:comObjects.map(o=>({...o,name:"N".repeat(80),id:o.id.padEnd(48,"i"),unit:"u".repeat(32)}))};
  const compact=JSON.stringify(near);
  assert.ok(Buffer.byteLength(compact)<=262144 && Buffer.byteLength(JSON.stringify(near,null,2))>262144);
  reopened.node("knxConfig").value=compact;await reopened.fire("knxSave","click");await reopened.fire("knxFormat","click");
  assert.equal(reopened.node("knxConfig").value,compact);
});
test("read-only objects cannot be written; exact saved ID reaches command API",async()=>{
  const s=setup({enabled:true});await s.poll();
  assert.equal(s.node("knxWrite").disabled,true);
  await s.fire("knxWrite","click");assert.equal(s.requests.filter(r=>r.method==="POST").length,0);
  await s.fire("knxRead","click");
  assert.deepEqual(JSON.parse(s.requests.at(-1).body),{object_id:"sensor",action:"read"});
  s.node("knxObject").value="1";await s.fire("knxObject","change");
  s.node("knxValue").value="on";await s.fire("knxWrite","click");
  assert.deepEqual(JSON.parse(s.requests.at(-1).body),{object_id:"lamp",action:"write",value:"on"});
  await s.fire("knxRefresh","click");assert.match(s.node("knxCache").textContent,/"valid": false/);
});
test("after refusing a 513-object backup the bound 512th object still sends commands and receives independent feedback",async()=>{
  const comObjects=Array.from({length:512},(_,i)=>({...config.comObjects[1],id:"point-"+i,
    group_address:`1/${i>>8}/${i%256}`,datapoint_type:"DPT-5.001"}));
  const full={...config,comObjects},last=comObjects[511].id;
  const layout={pages:[{id:"room",widgets:[{type:"slider",binding:{source:"knx",roles:{brightness_command:last,brightness_feedback:last}}}]}]};
  const backup={format:"betta-panel-backup",layout:{pages:[]},settings:{},knx:{enabled:false,
    configuration:{...full,comObjects:[...comObjects,{...comObjects[0],id:"overflow",group_address:"1/2/0"}]}}};
  const translations=loadEditor();
  const s=setup({enabled:true,configuration:structuredClone(full),layout:structuredClone(layout)},
    {t:translations.fn("t"),localizedRequestError:translations.fn("localizedRequestError")});
  s.run(fs.readFileSync(new URL("../../components/webui/www/js/settings.js",import.meta.url),"utf8"));
  await s.poll();
  s.node("knxObject").value="511";await s.fire("knxObject","change");
  s.node("settingsBackupFile").files=[{text:async()=>JSON.stringify(backup)}];
  await s.run("restoreBackup()");
  assert.match(s.node("settingsBackupMsg").textContent,/at most 512 communication objects/);
  assert.deepEqual(s.requests.filter(r=>r.method!=="GET"),[]);
  assert.deepEqual(s.model.configuration,full);assert.deepEqual(s.model.layout,layout);assert.equal(s.model.enabled,true);
  assert.equal(s.node("knxObject").value,"511");
  s.node("knxValue").value="80";await s.fire("knxWrite","click");await s.fire("knxRead","click");
  assert.deepEqual(s.requests.filter(r=>r.method==="POST").map(r=>JSON.parse(r.body)),[
    {object_id:last,action:"write",value:"80"},{object_id:last,action:"read"}]);
  s.model.feedback={[last]:{valid:true,live_feedback:true,value:62}};
  await s.fire("knxRefresh","click");
  assert.equal(s.requests.at(-1).url,"/api/knx/objects?offset=500");
  assert.deepEqual(JSON.parse(s.node("knxCache").textContent),{...comObjects[511],...s.model.feedback[last]});
});
test("polling is single-flight and leaves the draft untouched",async()=>{
  let release,calls=0;
  const s=setup({}, {fetch:()=>{calls++;return new Promise(r=>release=r);}});
  const first=s.poll();await s.poll();await s.poll();assert.equal(calls,1);
  release({ok:true,text:async()=>JSON.stringify(config)});
  await new Promise(r=>setImmediate(r));
  release({ok:true,json:async()=>({enabled:false,state:"disabled",object_count:2})});await first;
  s.node("settingsKnxSection").classList.contains=()=>true;await s.poll();assert.equal(calls,2);
});
test("server errors leave the draft and saved configuration intact",async()=>{
  const s=setup();await s.poll();
  const invalid={...config,physical_address:"bad"};
  s.node("knxConfig").value=JSON.stringify(invalid);await s.fire("knxConfig","input");
  // Client only checks JSON shape; address/DPT validation is authoritative on device.
  assert.equal(s.node("knxMessage").textContent,"knx.unsaved");
  await s.fire("knxSave","click");
  assert.match(s.node("knxMessage").textContent,/Invalid physical address/);
  assert.deepEqual(JSON.parse(s.node("knxConfig").value),invalid);
  assert.deepEqual(s.model.configuration,config);
  await s.fire("knxReload","click");assert.deepEqual(JSON.parse(s.node("knxConfig").value),config);
});

test("a damaged saved file remains exportable and repairable without overwriting it on load",async()=>{
  const s=setup({configuration:'{"broken":'}); await s.poll();
  assert.equal(s.node("knxConfig").disabled,false);
  assert.equal(s.node("knxConfig").value,'{"broken":');
  await s.fire("knxExport","click"); assert.deepEqual(s.downloads,["knxConfig.json"]);
  assert.equal(s.model.configuration,'{"broken":');
  s.node("knxConfig").value=JSON.stringify(config);await s.fire("knxSave","click");
  assert.deepEqual(s.model.configuration,config);
});
