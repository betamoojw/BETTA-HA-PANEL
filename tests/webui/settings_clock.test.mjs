// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { loadEditor } from "./load_editor.mjs";

function setup() {
  const { run } = loadEditor();
  run(`
    for (const key of Object.keys(el)) {
      if (Array.isArray(el[key])) continue;
      const listeners = new Map();
      el[key] = { value: '', checked: false, style: {}, dataset: {}, options: [],
        classList: { toggle() {}, add() {}, remove() {} },
        addEventListener(type, fn) { listeners.set(type, [...(listeners.get(type) || []), fn]); },
        dispatch(type) { for (const fn of listeners.get(type) || []) fn({ target: this }); } };
    }
    globalThis.puts = [];
    globalThis.putSettings = async value => puts.push(JSON.parse(JSON.stringify(value)));
    globalThis.setTimeout = fn => { fn(); return 0; };
    globalThis.renderCanvas = () => {};
    bindHardwareLive();
    fillClockSettings();
  `);
  return run;
}

test("clock controls persist through their real event handlers without rebooting", () => {
  const run = setup();
  run(`el.settingsSaverMode.value = 'clock'; el.settingsSaverMode.dispatch('change');`);
  assert.equal(run("el.settingsClockFields.hidden"), false);
  assert.equal(run("el.settingsSaverTiming.hidden"), true);
  const cases = [["Start", "start_seconds", "120", 120], ["Brightness", "brightness", "17", 17],
    ["OffSeconds", "off_seconds", "600", 600]];
  for (const [id, key, value, expected] of cases) {
    run(`el.settingsClock${id}.value = '${value}'; el.settingsClock${id}.dispatch('change');`);
    assert.equal(run(`puts.at(-1).hardware.clock.${key}`), expected);
    assert.equal(run("puts.at(-1).reboot"), false);
  }
  for (const [id, key] of [["Off", "off_enabled"], ["24h", "format_24h"], ["Seconds", "show_seconds"], ["Date", "show_date"]]) {
    run(`el.settingsClock${id}.checked = false; el.settingsClock${id}.dispatch('change');`);
    assert.equal(run(`puts.at(-1).hardware.clock.${key}`), false);
  }
  assert.equal(run("el.settingsClockOffSecondsRow.hidden"), true);
  run("globalThis.saved = puts.at(-1).hardware.clock; fillClockSettings(saved)");
  assert.equal(run("el.settingsClockBrightness.value"), "17");
  assert.equal(run("el.settingsClockOff.checked"), false);
  run("el.settingsClockOff.checked = true; el.settingsClockOff.dispatch('change')");
  assert.equal(run("el.settingsClockOffSeconds.value"), "600");
  assert.equal(run("el.settingsClockOffSecondsRow.hidden"), false);
  run("el.settingsSaverMode.value = 'disabled'; el.settingsSaverMode.dispatch('change')");
  assert.equal(run("el.settingsClockFields.hidden"), true);
  assert.equal(run("puts.at(-1).hardware.clock.brightness"), 17);
  run("el.settingsSaverMode.value = 'clock'; el.settingsSaverMode.dispatch('change')");
  assert.equal(run("puts.at(-1).hardware.clock.brightness"), 17);
});

test("display reset includes hidden clock options and matches firmware defaults", () => {
  const run = setup();
  run("fillClockSettings({brightness: 19, show_date: false}); resetDisplayDefaults()");
  assert.deepEqual(JSON.parse(run("JSON.stringify(puts.at(-1).hardware.clock)")), {
    start_seconds: 60, brightness: 5, off_enabled: true, off_seconds: 300,
    format_24h: true, show_seconds: true, show_date: true,
  });
  assert.equal(run("puts.at(-1).hardware.saver_mode"), "disabled");
});
