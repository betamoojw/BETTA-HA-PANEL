// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const read = path => fs.readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const context = vm.createContext({});
for (const name of ['i18n', 'i18n-knx', 'i18n-es', 'i18n-fr', 'i18n-pl', 'i18n-zh-cn', 'i18n-zh-tw']) {
  vm.runInContext(read(`components/webui/www/js/${name}.js`), context);
}
const catalogs = JSON.parse(vm.runInContext('JSON.stringify(WEB_I18N_BUILTIN)', context));
const fields = s => [...s.matchAll(/\{[a-zA-Z0-9_]+\}/g)].map(m => m[0]).sort();
const formats = s => [...s.matchAll(/%(?:[-+#0 ]*\d*(?:\.\d+)?[dfsux]|%)/g)].map(m => m[0]);
const flatten = (obj, prefix = '') => Object.fromEntries(Object.entries(obj).flatMap(([key, value]) =>
  typeof value === 'object' ? Object.entries(flatten(value, prefix + key + '.')) : [[prefix + key, value]]));
const store = read('main/settings/i18n_store.c');
const englishSource = store.match(/static const char \*I18N_BUILTIN_EN\s*=\s*((?:"(?:\\.|[^"\\])*"\s*)+);/)[1];
const english = flatten(JSON.parse([...englishSource.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m => JSON.parse(m[0])).join('')).lvgl);

for (const code of ['es', 'fr', 'pl', 'zh-cn', 'zh-tw']) {
  test(`${code}: every current browser key and interpolation is translated`, () => {
    const source = JSON.parse(read(`components/webui/i18n/${code}.json`));
    assert.deepEqual(catalogs[code], source);
    assert.deepEqual(Object.keys(source).sort(), Object.keys(catalogs.en).sort());
    for (const [key, value] of Object.entries(source)) {
      assert.equal(typeof value, 'string', key);
      assert.deepEqual(fields(value), fields(catalogs.en[key]), key);
      assert.ok(value || !catalogs.en[key], key);
    }
    assert.equal(source['remote.title'], {es: 'Pantalla en directo', fr: 'Écran en direct', pl: 'Podgląd na żywo', 'zh-cn': '实时屏幕', 'zh-tw': '實時螢幕'}[code]);
  });
  test(`${code}: complete panel catalog and printf argument order`, () => {
    const root = JSON.parse(read(`main/i18n/${code}.json`));
    const generated = read(`main/i18n/${code}.inc`).split(' =\n')[1];
    const embedded = JSON.parse([...generated.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m => JSON.parse(m[0])).join(''));
    assert.deepEqual(embedded, root, 'generated firmware catalog matches its JSON source');
    assert.deepEqual(Object.keys(root), ['lvgl']);
    const translated = flatten(root.lvgl);
    for (const [key, value] of Object.entries(english)) {
      assert.equal(typeof translated[key], 'string', key);
      assert.ok(translated[key].length, key);
      assert.deepEqual(formats(translated[key]), formats(value), key);
    }
    if (code.startsWith('zh-')) assert.match(translated['energy.title'], /能源/);
    const supported = new Set([...read('main/ui/fonts/cjk_codepoints.txt').matchAll(/^U\+([0-9A-F]+)$/gm)].map(m => parseInt(m[1], 16)));
    for (const [key, value] of Object.entries(translated)) {
      for (const char of value) {
        if (char.codePointAt(0) >= 0x2e80) assert.ok(supported.has(char.codePointAt(0)), `${key}: missing glyph ${char}`);
      }
    }
  });
}


for (const lang of ['zh-cn', 'zh-tw']) {
  test(`${lang}: local Xiaozhi translations survive unavailable translation APIs`, async () => {
    const requests = [];
    const sandbox = vm.createContext({
      WEB_I18N_BUILTIN: catalogs, DEFAULT_UI_LANGUAGE: 'en',
      editor: {}, el: {}, document: {documentElement: {}},
      normalizeUiLanguage: value => value.toLowerCase(),
      flattenTranslationObject: value => value,
      apiGet: async path => {requests.push(path); throw new Error('offline');},
      fetch: async path => {requests.push(path); throw new Error('offline');},
    });
    vm.runInContext(read('components/webui/www/js/translations.js'), sandbox);
    // Layout rendering is covered separately; exercise the real local catalog loader.
    vm.runInContext('applyWebTranslations = () => {};', sandbox);
    await vm.runInContext(`loadI18nLanguage("${lang}", true)`, sandbox);
    assert.deepEqual(requests, ['/api/i18n/languages', `/api/i18n/effective?lang=${lang}`]);
    assert.equal(sandbox.document.documentElement.lang, lang);
    for (const key of Object.keys(catalogs.en).filter(key => key.startsWith('xiaozhi.'))) {
      assert.equal(sandbox.editor.i18nMap[key], catalogs[lang][key], key);
      assert.match(sandbox.editor.i18nMap[key], /\p{Script=Han}/u, key);
    }
    const panel = JSON.parse(read(`main/i18n/${lang}.json`)).lvgl.xiaozhi;
    assert.equal(panel.title, catalogs[lang]['xiaozhi.title']);
    assert.equal(panel.talk, catalogs[lang]['xiaozhi.talk']);
    for (const [state, value] of Object.entries(panel.state)) {
      assert.equal(value, catalogs[lang][`xiaozhi.state.${state}`], state);
    }
  });
}
