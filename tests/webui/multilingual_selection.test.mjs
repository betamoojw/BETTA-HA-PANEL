// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import test from 'node:test';
import assert from 'node:assert/strict';
import { loadEditor } from './load_editor.mjs';

for (const language of ['de', 'en', 'es', 'fr', 'pl', 'zh-cn', 'zh-tw']) {
  test(`${language}: selection event, saved settings, reload and custom override`, async () => {
    const { run } = loadEditor();
    run(`
      for (const name of ['setupSettingsWorkspace', 'applySectionCollapseState', 'initSimpleUiMenus',
        'bindAddTileDialog', 'bindUpdateCheck', 'bindCrashBanner', 'bindInspectorHints',
        'refreshBrightnessNow', 'refreshCameraBrightnessStatus', 'bindBrightnessTests',
        'initEditorTheme', 'bindSecurityControls', 'bindStatusControls', 'loadHaDiagnostics'])
        globalThis[name] = () => {};
      globalThis.setInterval = () => 0;
      window.setTimeout = () => 0;
      globalThis.Event = class { constructor(type) { this.type = type; } };
      for (const key of Object.keys(el)) {
        if (Array.isArray(el[key])) continue;
        const listeners = new Map();
        el[key] = { value: '', checked: false, style: {}, dataset: {}, options: [],
          classList: { toggle() {}, add() {}, remove() {} },
          addEventListener(name, fn) { listeners.set(name, [...(listeners.get(name) || []), fn]); },
          dispatch(name) { for (const fn of listeners.get(name) || []) fn({ target: this }); },
          appendChild(option) { this.options.push(option); } };
      }
      document.getElementById = id => el[id] || null;
      document.createElement = () => ({});
      bindUi();
      globalThis.saved = null;
      putSettings = async payload => { saved = JSON.parse(JSON.stringify(payload)); };
      globalThis.setTimeout = fn => { fn(); return 0; };
      renderSettings = () => {};
      applyWebTranslations = () => {};
      renderCanvas = () => {};
      editor.languageCatalog = [{code: 'en'}];
      loadEffectiveTranslation = async () => ({});
      el.settingsUiStyle.value = 'flat';
    `);
    await run(`el.settingsLanguage.value = '${language}'; el.settingsLanguage.onchange()`);
    run("el.settingsLanguage.dispatch('change')");
    assert.equal(run('editor.settings.ui.language'), language);
    assert.equal(run('saved.ui.language'), language);
    assert.equal(run('saved.reboot'), false);
    await run("editor.i18nLanguage = 'en'; loadI18nLanguage(saved.ui.language)");
    assert.equal(run('document.documentElement.lang'), language);
    assert.equal(run('el.settingsLanguage.value'), language);
    assert.equal(run("t('remote.title')"), run(`WEB_I18N_BUILTIN['${language}']['remote.title']`));
    await run(`loadEffectiveTranslation = async () => ({web: {remote: {title: 'Custom title'}}}); loadI18nLanguage('${language}')`);
    assert.equal(run("t('remote.title')"), 'Custom title');
    assert.equal(run("t('settings.language')"), run(`WEB_I18N_BUILTIN['${language}']['settings.language'] || 'settings.language'`));
    assert.equal(run("el.settingsLanguage.options.some(o => o.value === 'pl' && o.textContent === 'Polski')"), true);
  });
}
