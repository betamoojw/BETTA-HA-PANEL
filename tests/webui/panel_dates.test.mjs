// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import { loadEditor } from "./load_editor.mjs";

const read = file => readFileSync(new URL("../../main/net/tzdb/" + file, import.meta.url), "utf8");
const rules = JSON.parse(read("zones.json"));
const patterns = new Map();
for (const line of read("date_patterns.txt").trim().split(/\r?\n/).filter(line => !line.startsWith("#"))) {
  const [, pattern, ...zones] = line.split(" ");
  for (const zone of zones) patterns.set(zone, pattern);
}
function activate(run, zone) {
  const time = { active_timezone: zone, date_pattern: patterns.get(zone) || "Y-M-D", timezone_rules: rules[zone] || zone };
  run("editor.settings = {time:" + JSON.stringify(time) + "}");
}
test("midnight, weekday, civil HA dates and build stamps follow the selected panel location", () => {
  const now = Date.parse("2026-01-01T00:30:05Z");
  const { run } = loadEditor({ now });
  for (const [zone, stamp, civil, weekday, build] of [
    ["America/New_York", "12/31/2025 19:30", "01/01/2026", "Wed", "11/06/2026"],
    ["America/Toronto", "2025-12-31 19:30", "2026-01-01", "Wed", "2026-11-06"],
    ["Europe/London", "01/01/2026 00:30", "01/01/2026", "Thu", "06/11/2026"],
    ["Europe/Berlin", "01.01.2026 01:30", "01.01.2026", "Thu", "06.11.2026"],
    ["Asia/Tokyo", "2026/01/01 09:30", "2026/01/01", "Thu", "2026/11/06"],
    ["Asia/Kathmandu", "2026-01-01 06:15", "2026-01-01", "Thu", "2026-11-06"],
    ["EST5EDT,M3.2.0,M11.1.0", "2025-12-31 19:30", "2026-01-01", "Wed", "2026-11-06"],
  ]) {
    activate(run, zone);
    run("editor.i18nLanguage = 'en'");
    assert.equal(run("previewWeekday(0)"), weekday, zone);
    assert.equal(run("panelBuildDate('Nov  6 2026')"), build);
    for (const language of ["en", "de", "zh-cn"]) {
      run("editor.i18nLanguage = " + JSON.stringify(language));
      assert.equal(run("previewSensorTimestamp('2026-01-01T00:30:05Z', 'datetime')"), stamp, zone);
      assert.equal(run("previewDateTimeValue('2026-01-01T01:30:05.123+01:00')"), stamp + ":05");
      assert.equal(run("previewDateTimeValue('2026-01-01 01:30:00')"), civil + " 01:30");
      assert.equal(run("previewSensorDate('2026-01-01', 'date')"), civil);
    }
  }
  activate(run, "America/New_York");
  assert.equal(run("previewSensorDate('2026-01-01', 'relative')"), run("t('preview.sensor.tomorrow')"));
  run("editor.settings.hardware = {clock:{format_24h:false}}");
  assert.equal(run("panelCivilText(panelCivilTime(Date.now()), {seconds:true})"), "12/31/2025 07:30:05 PM");
});

test("recurring panel offsets handle both DST boundaries, southern summer and fractional offsets", () => {
  const { run } = loadEditor();
  const cases = [
    ["America/New_York", "2026-03-08T06:59:59Z", "2026-03-08T01:59:59"],
    ["America/New_York", "2026-03-08T07:00:00Z", "2026-03-08T03:00:00"],
    ["America/New_York", "2026-11-01T06:00:00Z", "2026-11-01T01:00:00"],
    ["Europe/Berlin", "2026-03-29T01:00:00Z", "2026-03-29T03:00:00"],
    ["Australia/Lord_Howe", "2026-01-01T13:00:00Z", "2026-01-02T00:00:00"],
    ["Australia/Lord_Howe", "2026-06-01T13:00:00Z", "2026-06-01T23:30:00"],
    ["Etc/GMT+5", "2024-03-01T00:30:00Z", "2024-02-29T19:30:00"],
    ["XST0XDT,J60/0,J300/0", "2024-03-01T00:00:00Z", "2024-03-01T01:00:00"],
    ["XST0XDT,59/0,300/0", "2024-02-29T00:00:00Z", "2024-02-29T01:00:00"],
  ];
  for (const [zone, source, expected] of cases) {
    activate(run, zone);
    assert.equal(run("panelCivilTime(Date.parse(" + JSON.stringify(source) + ")).toISOString().slice(0,19)"), expected, zone);
  }
  assert.equal(patterns.size, Object.keys(rules).length);
  for (const rule of new Set(Object.values(rules))) assert.ok(run("panelZoneRules(" + JSON.stringify(rule) + ")"), rule);
});
