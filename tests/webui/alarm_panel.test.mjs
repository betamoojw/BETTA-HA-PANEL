// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import { loadEditor } from './load_editor.mjs';

function setup() {
  function node(tag = 'div') {
    return { tag, children: [], dataset: {}, value: '', hidden: false,
      append(...children) { for (const child of children) { child.parent = this; this.children.push(child); } },
      replaceChildren() { this.children = []; },
      remove() { this.parent.children = this.parent.children.filter(c => c !== this); },
      setAttribute(key, value) { this[key] = value; },
      querySelectorAll(selector) {
        const match = n => selector === '.alarm-binding-row' ? n.className === 'alarm-binding-row' :
          selector === '[data-alarm-hours]' ? 'alarmHours' in n.dataset :
          selector === '[data-alarm-entity]' ? 'alarmEntity' in n.dataset :
          selector === '[data-alarm-name]' ? 'alarmName' in n.dataset : false;
        const list = selector.match(/^\[data-alarm-list="(.+)"\] \.alarm-binding-row$/);
        if (list) return this.children.filter(c => c.dataset.alarmList === list[1]).flatMap(c => c.children);
        return this.children.flatMap(c => [...(match(c) ? [c] : []), ...c.querySelectorAll(selector)]);
      },
      querySelector(selector) { return this.querySelectorAll(selector)[0] || null; },
    };
  }
  const section = node();
  let widget = { type:'alarm_panel', entity_id:'alarm_control_panel.home', alarm_log_hours:24,
    alarm_zones:[], alarm_actions:[] };
  const context = vm.createContext({ TextEncoder, document:{getElementById:()=>section,createElement:node},
    t:k=>k, editor:{entities:[]}, autoApplyInspector() {
      context.alarmPanelSaveInspector(widget,widget.type,context.alarmPanelReadInspector());
    } });
  vm.runInContext(fs.readFileSync(new URL('../../components/webui/www/js/alarm-panel.js',import.meta.url),'utf8'),context);
  const load = () => context.alarmPanelLoadInspector(widget);
  load();
  return {context,section,load,get widget(){return widget;},
    reload() { widget = JSON.parse(JSON.stringify(widget)); load(); },
    add(key,id,name) {
      const list = section.children.find(c => c.dataset.alarmList === key);
      const nextAdd = section.children.slice(section.children.indexOf(list)).find(c => c.tag === 'button');
      nextAdd.onclick();
      const row = list.children.at(-1);
      row.children[0].value=id; row.children[1].value=name; row.children[0].onchange();
      return row;
    } };
}

test('alarm binding events persist ordered names, controls and hours through reload and hide/show',()=>{
  const s=setup(); s.add('alarm_zones','binary_sensor.front','Front door');
  s.add('alarm_actions','input_boolean.chime','Chime'); s.add('alarm_actions','button.panic','Panic');
  const hours=s.section.querySelector('[data-alarm-hours]'); hours.value='48'; hours.onchange();
  const saved=JSON.stringify(s.widget);
  s.context.alarmPanelLoadInspector(null); assert.equal(s.section.hidden,true);
  s.reload(); assert.equal(s.section.hidden,false); assert.equal(JSON.stringify(s.widget),saved);
  assert.equal(s.section.querySelector('[data-alarm-hours]').value,'48');
  assert.equal(s.section.querySelectorAll('[data-alarm-list="alarm_actions"] .alarm-binding-row').length,2);
  s.section.querySelectorAll('[data-alarm-list="alarm_actions"] .alarm-binding-row')[0].children[2].onclick();
  s.reload(); assert.deepEqual(s.widget.alarm_actions,[{entity_id:'button.panic',name:'Panic'}]);
});

test('alarm invalid domains, duplicate IDs, byte lengths and hours are rejected before saving',()=>{
  const s=setup(), validate=s.context.alarmPanelValidateSlots;
  assert.equal(validate('alarm_zones',[{entity_id:'switch.door'}]),false);
  assert.equal(validate('alarm_zones',[{entity_id:'binary_sensor.a.b'}]),false);
  assert.equal(validate('alarm_zones',[{entity_id:'binary_sensor.door'},{entity_id:'binary_sensor.door'}]),false);
  assert.equal(validate('alarm_zones',[{entity_id:'binary_sensor.door',name:'门'.repeat(16)}]),false);
  assert.equal(validate('alarm_zones',[{entity_id:'binary_sensor.door',name:'门'.repeat(15)}]),true);
  const hours=s.section.querySelector('[data-alarm-hours]');
  for(const invalid of ['0','1.5','169']) {hours.value=invalid;assert.throws(()=>hours.onchange());}
  assert.equal(s.widget.alarm_log_hours,24);
});

test('alarm layout normalization and backups retain bindings and never persist entered codes',()=>{
  const s=setup(); Object.assign(s.widget,{code:'1234',pin:'1234',alarm_code:'1234'});
  s.add('alarm_zones','binary_sensor.door','Door');
  assert.equal('code' in s.widget,false); assert.equal('pin' in s.widget,false);
  assert.equal('alarm_code' in s.widget,false);
  const {run}=loadEditor();
  const layout={pages:[{id:'security',title:'Security',widgets:[{...s.widget,id:'security_panel',rect:{x:20,y:20,w:1240,h:640}}]}]};
  const restored=JSON.parse(run(`(() => {const layout=${JSON.stringify(layout)}; normalizeLayoutWidgets(layout); return JSON.stringify(layout);})()`));
  assert.deepEqual(restored.pages[0].widgets[0].alarm_zones,JSON.parse(JSON.stringify(s.widget.alarm_zones)));
  assert.deepEqual(JSON.parse(run(`JSON.stringify(allowedEntityDomainsForWidgetType('alarm_panel'))`)),['alarm_control_panel']);
});
