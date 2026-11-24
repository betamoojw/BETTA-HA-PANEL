// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
import { test } from "node:test";
import assert from "node:assert/strict";
import { boundEditor } from "./bound_editor.mjs";

function section(id) {
  const classes = new Set();
  return { id, hidden: false, classList: {
    add: name => classes.add(name),
    remove: name => classes.delete(name),
    contains: name => classes.has(name),
    toggle(name, hidden) {
      if (hidden) classes.add(name); else classes.delete(name);
    },
  } };
}

function workspace(savedPane = "settings") {
  const page = section("pageInspectorSection");
  const widget = section("inspectorSection");
  const actions = section("settingsSidebarActions");
  const run = boundEditor({ bind: false, elements: {
    pageInspectorSection: page, inspectorSection: widget, settingsSidebarActions: actions,
    remoteTabBtn: { textContent: "Live Display" },
  } });
  run("window").sessionStorage = {
    getItem: () => JSON.stringify({ pane: savedPane, section: "settingsHaSection" }),
    setItem() {},
  };
  run(`
    el.actionsPanel.children = [el.pageInspectorSection, el.inspectorSection, el.settingsSidebarActions];
    el.settingsNavButtons = []; el.settingsContentSections = [];
    // Keep the real pane, page, widget Inspector and renderAll paths.
    for (const name of ["renderPages", "renderWidgets", "renderCanvas", "renderLayoutCapacity",
      "updateCanvasScale", "refreshInspectorLayout", "renderHaDiagnosticsBanner", "syncPanelPage",
      "renderEntityOptions"])
      globalThis[name] = () => {};
    editor.layout = { pages: [{ id: "living", title: "Living", visible: false,
      widgets: [{ id: "value", type: "sensor", title: "Temperature", entity_id: "sensor.temperature",
        rect: { x: 0, y: 0, w: 120, h: 120 } }] }] };
    editor.selectedPageId = "living"; editor.selectedWidgetId = null;
    setActivePane();
  `);
  return { run, page, widget, actions };
}

test("restoring Settings keeps layout inspectors hidden after a late full render", () => {
  const { run, page, widget, actions } = workspace();
  assert.equal(run("editor.activePane"), "settings");
  assert.equal(run("editor.activeSettingsSection"), "settingsHaSection");
  run("renderAll()");
  assert.equal(page.classList.contains("hidden"), true);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(actions.classList.contains("hidden") || actions.hidden, false);
});

test("switching to Settings while the layout request is pending keeps both inspectors hidden", async () => {
  const { run, page, widget, actions } = workspace("layout");
  run(`
    refreshLayoutLimits = async () => {};
    applyCanvasGeometry = () => {};
    apiGet = () => new Promise(resolve => { globalThis.finishLayout = resolve; });
    globalThis.pendingLayout = loadLayout();
    setActivePane("settings");
    finishLayout(JSON.parse(JSON.stringify(editor.layout)));
  `);
  assert.equal(await run("pendingLayout"), true);
  assert.equal(run("editor.activePane"), "settings");
  assert.equal(page.classList.contains("hidden"), true);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(actions.classList.contains("hidden") || actions.hidden, false);
});

test("Settings retains the selected widget while Inspector refreshes stay hidden", () => {
  const { run, page, widget, actions } = workspace("layout");
  run("editor.selectedWidgetId = 'value'; setActivePane('settings'); renderAll()");
  assert.equal(page.classList.contains("hidden"), true);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(actions.classList.contains("hidden") || actions.hidden, false);
  run("setActivePane('layout')");
  assert.equal(page.classList.contains("hidden"), true);
  assert.equal(widget.classList.contains("hidden"), false);
  assert.equal(actions.classList.contains("hidden"), true);
  assert.equal(run("editor.selectedWidgetId"), "value");
  assert.equal(run("selectedWidget().entity_id"), "sensor.temperature");
  run("editor.selectedWidgetId = null; renderAll()");
  assert.equal(page.classList.contains("hidden"), false);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(run("selectedPage().visible"), false);
});

test("a late layout render keeps both inspectors hidden throughout Live Display", () => {
  const { run, page, widget, actions } = workspace("layout");
  run("setActivePane('remote'); renderAll()");
  assert.equal(page.classList.contains("hidden"), true);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(run("editor.activePane"), "remote");
  run("setActivePane('layout')");
  assert.equal(page.classList.contains("hidden"), false);
  assert.equal(widget.classList.contains("hidden"), true);
  assert.equal(actions.classList.contains("hidden"), true);
});
