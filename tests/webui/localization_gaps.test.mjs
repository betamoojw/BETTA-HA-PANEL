// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import test from 'node:test';
import assert from 'node:assert/strict';
import {loadEditor} from './load_editor.mjs';

function setup(lang) {
  const editor = loadEditor();
  editor.run(`
    editor.i18nMap = WEB_I18N_BUILTIN[${JSON.stringify(lang)}];
    editor.i18nLanguage = ${JSON.stringify(lang)};
    function node() {
      return {value: '', textContent: '', style: {}, dataset: {}, children: [],
        addEventListener(name, fn) { this[name] = fn; },
        appendChild(child) { this.children.push(child); },
        set innerHTML(value) { this.children = []; }, get innerHTML() { return ''; }};
    }
    const nodes = {};
    for (const id of ['settingsThemeSelect','settingsThemeNewId','settingsThemeNewName',
      'settingsThemeBase','settingsThemeSaveCustomBtn','settingsThemeInfo','settingsThemeColors']) nodes[id] = node();
    document.getElementById = id => nodes[id] || null;
    document.createElement = node;
    themeState.list = [{id: 'builtin', name: 'BETTA', builtin: true}, {id: 'mine', name: '我的主題', builtin: false}];
    themeState.activeId = 'builtin';
    themeState.editing = {id: 'builtin', name: 'BETTA', colors: {card_bg_off: '#112233', text_primary: '#ffffff'}};
  `);
  return editor;
}

for (const lang of ['zh-cn', 'zh-tw']) {
  test(`${lang}: theme language refresh preserves pending name, ID, selection and colors`, () => {
    const {run} = setup(lang);
    run(`themePopulateSelect(); themeRenderColorGrid(themeState.editing); themeSyncEditFields();
      nodes.settingsThemeNewId.value = 'pending'; nodes.settingsThemeNewName.value = '未保存名稱';
      nodes.settingsThemeSelect.value = 'mine';
      const input = nodes.settingsThemeColors.children[0].children[1].children[0].children[1];
      input.value = '#abcdef'; input.input();
      const pending = JSON.stringify(themeState.editing.colors);
      themeRefreshTranslations();`);
    assert.equal(run('nodes.settingsThemeNewId.value'), 'pending');
    assert.equal(run('nodes.settingsThemeNewName.value'), '未保存名稱');
    assert.equal(run('nodes.settingsThemeSelect.value'), 'mine');
    assert.equal(run('JSON.stringify(themeState.editing.colors) === pending'), true);
    assert.match(run('nodes.settingsThemeBase.textContent'), /[基礎础]/);
    assert.match(run('nodes.settingsThemeColors.children[0].children[0].textContent'), /文字/);
    assert.match(run('nodes.settingsThemeSelect.children[0].textContent'), /內建|内置/);
  });
  test(`${lang}: save sends pending values, delete confirmation is localized, errors hide raw English`, async () => {
    const {run} = setup(lang);
    run(`const requests = []; fetch = async (url, options) => {
      requests.push({url, options}); return {ok: false, text: async () => 'server exploded'};
    };
    nodes.settingsThemeNewId.value = 'mine'; nodes.settingsThemeNewName.value = '我的配色';`);
    await run('themeSaveCustom()');
    assert.deepEqual(JSON.parse(run('requests[0].options.body')).name, '我的配色');
    assert.match(run('nodes.settingsThemeInfo.textContent'), /无法保存|無法儲存/);
    assert.doesNotMatch(run('nodes.settingsThemeInfo.textContent'), /server exploded/);
    run(`nodes.settingsThemeSelect.value = 'mine'; let confirmation = ''; window.confirm = text => {confirmation = text; return true;};`);
    await run('themeDeleteCustom()');
    assert.match(run('confirmation'), /删除|刪除/);
    assert.equal(run('requests[1].options.method'), 'DELETE');
    assert.match(run('nodes.settingsThemeInfo.textContent'), /无法删除|無法刪除/);
  });
  test(`${lang}: OTA errors retain actionable localized reasons`, () => {
    const {fn} = setup(lang);
    const translate = fn('otaLocalizedError');
    const errors = ['SHA256 mismatch', 'Failed to allocate OTA task', 'OTA image is larger than the target partition',
      'Selected file is not an ESP app image', 'Failed to write OTA data', 'OTA already running', 'Failed to open OTA URL'];
    const results = errors.map(translate);
    assert.equal(new Set(results).size, errors.length);
    for (const text of results) assert.match(text, /\p{Script=Han}/u);
  });
  test(`${lang}: adding widgets localizes defaults but preserves explicit titles`, () => {
    const {run} = setup(lang);
    run(`editor.layout = {pages: [{id:'p', title:'Page', widgets:[]}]}; editor.selectedPageId='p';
      renderAll = () => {}; markDirty = () => {};`);
    for (const type of ['car_widget','solar_forecast','sonos_favorites','bambu_printer']) {
      run(`addWidget(${JSON.stringify(type)}, {entityId:''});`);
      assert.match(run('editor.layout.pages[0].widgets.at(-1).title'), /\p{Script=Han}/u);
      run(`addWidget(${JSON.stringify(type)}, {entityId:'', title:'My custom name'});`);
      assert.equal(run('editor.layout.pages[0].widgets.at(-1).title'), 'My custom name');
    }
  });
}

test('dynamic theme group and color keys exist in every bundled language', () => {
  const {run} = setup('zh-cn');
  assert.equal(run(`Object.values(WEB_I18N_BUILTIN).every(catalog => THEME_COLOR_GROUPS.every(group =>
    catalog['settings.theme.group.' + group.id] && group.keys.every(key =>
      catalog['settings.theme.color.' + key])))`), true);
});

test('theme feedback changes language without a new operation', () => {
  const {run} = setup('zh-cn');
  run(`themeSetInfo('settings.theme.invalid_id', true); editor.i18nMap = WEB_I18N_BUILTIN['zh-tw']; themeRefreshTranslations();`);
  assert.equal(run('nodes.settingsThemeInfo.textContent'), run('WEB_I18N_BUILTIN["zh-tw"]["settings.theme.invalid_id"]'));
});

for (const lang of ['zh-cn', 'zh-tw']) {
  test(`${lang}: request errors preserve localized validation and replace raw server diagnostics`, () => {
    const {run, fn} = setup(lang);
    const translate = fn('localizedRequestError');
    const validation = run('t("settings.translation.invalid_json")');
    assert.equal(translate(new Error(validation)), validation);
    assert.equal(translate(new Error('Failed to fetch')), run('t("common.request_failed")'));
    assert.equal(translate(new Error('invalid admin password')), run('t("common.request_failed")'));
  });
}
