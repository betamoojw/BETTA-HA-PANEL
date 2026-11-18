// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import { loadEditor } from "./load_editor.mjs";

// Supply just the DOM used while binding editor controls. Tests can replace a
// control with the real Live Display harness and run the normal bootstrap.
export function boundEditor({ elements = {}, bind = true } = {}) {
  const { run } = loadEditor();
  run(`
    globalThis.realHardwareBinder = bindHardwareLive;
    for (const name of ["setupSettingsWorkspace", "applySectionCollapseState", "initSimpleUiMenus",
      "bindAddTileDialog", "bindUpdateCheck", "bindCrashBanner", "bindInspectorHints",
      "refreshBrightnessNow", "refreshCameraBrightnessStatus", "bindHardwareLive", "bindBrightnessTests",
      "initEditorTheme", "bindSecurityControls", "bindStatusControls", "loadHaDiagnostics"])
      globalThis[name] = () => {};
    globalThis.setInterval = () => 0;
    window.setTimeout = () => 0;
    globalThis.Event = class { constructor(type) { this.type = type; } };
    for (const key of Object.keys(el)) {
      if (Array.isArray(el[key])) continue;
      const listeners = new Map();
      el[key] = { value: "", checked: false, style: {}, dataset: {},
        classList: { toggle() {}, add() {}, remove() {} },
        addEventListener(name, callback) { listeners.set(name, [...(listeners.get(name) || []), callback]); },
        dispatchEvent(event) { for (const fn of listeners.get(event.type) || []) fn({ target: this }); },
        focus() { this.focused = true; } };
    }
    document.getElementById = id => el[id] || null;
  `);
  Object.assign(run("el"), elements);
  if (bind) run("bindUi()");
  return run;
}
