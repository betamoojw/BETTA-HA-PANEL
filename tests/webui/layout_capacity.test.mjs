// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { loadEditor } from './load_editor.mjs';

function setup(width = 1280) {
  const api = loadEditor({ width });
  api.run(`
    const capacityNodes = Object.fromEntries(['layoutCapacity','layoutCapacityPages','layoutCapacityPage',
      'layoutCapacityTotal','layoutCapacityHint'].map(id => [id, {textContent:'', classList:{toggle() {}}}]));
    document.getElementById = id => capacityNodes[id] || null;
    for (const name of ['renderPages','renderPageEditor','renderWidgets','renderInspector','renderCanvas',
      'updateCanvasScale','refreshInspectorLayout','loadEnergyPreview','importHouseRooms','loadHaDiagnostics']) globalThis[name] = () => {};
    el.status = {textContent:'', style:{}, classList:{toggle() {}}};
    window.confirm = () => true; window.setTimeout = () => 0;
    globalThis.Blob = class { constructor(parts) { this.size = layoutJsonBytes(parts.join('')); } };
    globalThis.makePages = (count, widgets = 0) => Array.from({length:count}, (_,p) => ({id:'p'+p, widgets:
      Array.from({length:widgets}, (_,w) => ({id:'w'+p+'_'+w, type:'empty_tile', rect:{x:0,y:0,w:120,h:80}}))}));
    editor.layout = {version:1, pages:makePages(1)};
    editor.selectedPageId = 'p0'; editor.activePane = 'layout';
    renderAll(); clearDirty(); layoutHistoryReset();
  `);
  return api;
}

test('all display widths and rotations use the runtime budget, even with old firmware metadata', () => {
  const {run} = setup();
  for (const [width, height, pages, widgets] of [[480,480,5,160],[720,720,8,224],
    [1280,800,12,224],[800,1280,8,224],[1280,720,12,224],[720,1280,8,224]]) {
    run(`applyLayoutLimits({screen_w:${width},screen_h:${height},max_pages:12,max_widgets_total:224})`);
    assert.equal(run('maxPages()'), pages);
    assert.equal(run('maxWidgetsTotal()'), widgets);
  }
});

test('hidden pages count; page allowance respects total and fixed pages show no editable widgets', () => {
  const {run} = setup();
  run(`editor.layout.pages = makePages(7,32); editor.layout.pages[0].visible = false;
    editor.layout.pages[0].widgets.pop(); editor.layout.pages.push({id:'p7',widgets:[]}); editor.selectedPageId='p7'; renderAll()`);
  assert.equal(run('layoutCapacity().remainingPages'), 4);
  assert.equal(run('layoutCapacity().remainingPage'), 1);
  assert.equal(run('layoutCapacity().remainingTotal'), 1);
  assert.match(run(`document.getElementById('layoutCapacityPage').textContent`), /1 widgets left/);
  run(`selectedPage().type=ENERGY_PAGE_TYPE; renderAll()`);
  assert.equal(run('layoutCapacity().remainingPage'), null);
  assert.match(run(`document.getElementById('layoutCapacityPage').textContent`), /no editable widgets/);
});

test('all page creation paths and page duplication stop at the current width limit', () => {
  const {run} = setup(480);
  run(`editor.layout.pages=makePages(5); renderAll(); globalThis.before=layoutSnapshot();
    addPage(); addEnergyPage(); addHousePage(); duplicatePage(selectedPage());`);
  assert.equal(run('layoutSnapshot()'), run('before'));
  assert.match(run('el.status.textContent'), /5 pages/);
});

test('add and duplicate widgets stop at per-page or total caps without partial mutation', () => {
  const {run} = setup();
  run(`editor.layout.pages=makePages(1,32); renderAll(); globalThis.before=layoutSnapshot();
    addWidget('empty_tile'); duplicateWidget('w0_0');`);
  assert.equal(run('layoutSnapshot()'), run('before'));
  run(`editor.layout.pages=makePages(7,32); editor.layout.pages.push({id:'p7',widgets:[]});
    editor.selectedPageId='p7'; renderAll(); before=layoutSnapshot(); addWidget('empty_tile');
    duplicatePage(editor.layout.pages[0]);`);
  assert.equal(run('layoutSnapshot()'), run('before'));
  assert.equal(run('layoutCapacity().remainingPage'), 0);
});

test('move at the total cap succeeds; copying at the cap preserves the layout', () => {
  const {run} = setup();
  run(`editor.layout.pages=makePages(7,32); editor.layout.pages.push({id:'p7',widgets:[]});
    renderAll(); transferWidget('p0','w0_0','p7','move');`);
  assert.equal(run('layoutCapacity().widgets'), 224);
  assert.equal(run('selectedPage().widgets.length'), 1);
  const before = run('layoutSnapshot()');
  assert.throws(() => run(`transferWidget('p0','w0_1','p7','copy')`), /224/);
  assert.equal(run('layoutSnapshot()'), before);
});

test('add, undo, redo, delete, save and reload update the rendered counters', async () => {
  const {run} = setup();
  run(`addWidget('empty_tile');`);
  assert.equal(run('layoutCapacity().remainingPage'), 31);
  run('layoutHistoryStep(-1)');
  assert.equal(run('layoutCapacity().remainingPage'), 32);
  run('layoutHistoryStep(1)');
  assert.equal(run('layoutCapacity().remainingPage'), 31);
  run(`globalThis.saved=null; fetch=async (url,opts) => {
    if(url.includes('limits=1'))return {ok:true,json:async()=>({screen_w:1280,screen_h:800,max_pages:12,max_pages_capacity:12,max_widgets_total:224})};
    if(opts?.method==='PUT'){saved=JSON.parse(opts.body);return {ok:true}};
    return {ok:true,json:async()=>JSON.parse(JSON.stringify(saved))};};`);
  await run('saveLayout()');
  assert.equal(await run('loadLayout()'), true);
  assert.equal(run('layoutCapacity().widgets'), 1);
  assert.equal(run(`'layoutCapacity' in saved`), false);
  run('editor.selectedWidgetId=selectedPage().widgets[0].id; deleteWidget()');
  assert.equal(run('layoutCapacity().widgets'), 0);
});

test('rotation updates limits without rewriting an unsaved layout, and wider stored layouts remain loadable', async () => {
  const {run} = setup();
  run(`editor.layout.orientation='landscape';editor.layout.pages=makePages(12);renderAll();
    globalThis.before=layoutSnapshot();applyLayoutLimits({screen_w:800,screen_h:1280,max_pages:8,max_pages_capacity:12});`);
  assert.equal(run('layoutSnapshot()'), run('before'));
  assert.equal(run('CANVAS_WIDTH'), 1280);
  assert.equal(run('layoutCapacity().remainingPages'), 0);
  assert.match(run(`document.getElementById('layoutCapacityHint').textContent`), /rotated/);
  run(`validateLayoutStructure(editor.layout,{stored:true})`);
  assert.throws(() => run('validateLayoutStructure(editor.layout)'), /8 pages/);
  run(`fetch=async()=>({ok:true,json:async()=>({screen_w:800,screen_h:1280,max_pages:8,max_pages_capacity:12})})`);
  await assert.rejects(run('saveLayout()'), /rotated/);
  run(`CANVAS_WIDTH=800;editor.layout.orientation='portrait';renderAll()`);
  assert.match(run(`document.getElementById('layoutCapacityHint').textContent`), /4 pages over/);
  run(`editor.layout.pages=makePages(8);renderAll()`);
  assert.equal(run(`document.getElementById('layoutCapacityHint').hidden`), true);
});

test('over-cap imports are atomic and empty layouts show no current-page quota', () => {
  const {run} = setup(480);
  const before = run('layoutSnapshot()');
  assert.throws(() => run(`importLayoutFromText(JSON.stringify({version:1,pages:makePages(6)}))`), /5 pages/);
  assert.equal(run('layoutSnapshot()'), before);
  run(`importLayoutFromText('' + JSON.stringify({version:1,pages:[]}))`);
  assert.equal(run('layoutCapacity().remainingPages'), 5);
  assert.equal(run('layoutCapacity().remainingPage'), null);
});

test('capacity polling is coalesced, ignores invalid limits and retains values on failure', async () => {
  const {run} = setup();
  run(`globalThis.calls=0;globalThis.reply=null;apiGet=()=>{calls++;return new Promise(resolve=>reply=resolve)};
    globalThis.first=refreshLayoutLimits();globalThis.second=refreshLayoutLimits();`);
  assert.equal(run('calls'), 1);
  run(`reply({screen_w:720,max_pages:8,max_widgets_total:224})`);
  await run('Promise.all([first,second])');
  assert.equal(run('maxPages()'), 8);
  run(`applyLayoutLimits({screen_w:-1,max_pages:0,max_widgets_total:'bad'});apiGet=async()=>{throw new Error('offline')}`);
  await run('refreshLayoutLimits()');
  assert.equal(run('maxPages()'), 8);
});
