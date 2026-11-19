// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
// Refresh the offline date profiles from an unpacked IANA tzdata release and Node ICU.
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const dir = process.argv[2];
if (!dir) throw new Error("Usage: node tools/update_timezone_dates.mjs <unpacked-tzdata>");
const database = new URL("../main/net/tzdb/", import.meta.url);
const zones = Object.keys(JSON.parse(readFileSync(new URL("zones.json", database))));
const territories = new Map();
const links = new Map();
for (const line of readFileSync(join(dir, "zone.tab"), "utf8").split(/\r?\n/)) {
  if (!line || line.startsWith("#")) continue;
  const [territory, , zone] = line.split("\t");
  territories.set(zone, territory);
}
for (const line of readFileSync(join(dir, "backward"), "utf8").split(/\r?\n/)) {
  const match = /^Link\s+(\S+)\s+(\S+)/.exec(line);
  if (match) links.set(match[2], match[1]);
}

function datePattern(territory) {
  if (territory === "001" || territory === "AQ") return "Y-M-D";
  const { language } = new Intl.Locale(`und-${territory}`).maximize();
  // Full ICU still lacks a few of the likely languages. Use a supported
  // regional locale explicitly so that the build host's locale cannot leak in.
  const secondary = { BQ: "nl", CW: "nl", HT: "fr", PY: "es" }[territory] || "en";
  const [locale] = Intl.DateTimeFormat.supportedLocalesOf([`${language}-${territory}`, `${secondary}-${territory}`]);
  if (!locale) throw new Error(`No supported locale for ${territory}`);
  const formatter = new Intl.DateTimeFormat(locale, {
    calendar: "gregory", numberingSystem: "latn", timeZone: "UTC",
    year: "numeric", month: "2-digit", day: "2-digit",
  });
  const parts = formatter.formatToParts(Date.UTC(2006, 10, 23));
  const fields = parts.filter(p => ["day", "month", "year"].includes(p.type));
  if (fields.length !== 3) throw new Error(`Missing Gregorian date parts for ${territory}`);
  const separator = parts.find(p => p.type === "literal" && /[./-]/.test(p.value));
  return fields.map(p => p.type[0].toUpperCase()).join(separator?.value.match(/[./-]/)[0] || "-");
}

const groups = new Map();
for (const zone of zones.sort()) {
  let name = zone;
  while (!territories.has(name) && links.has(name)) name = links.get(name);
  const territory = zone.startsWith("Etc/") ? "001" : territories.get(name);
  if (!territory) throw new Error(`Unassigned timezone: ${zone}`);
  if (!groups.has(territory)) groups.set(territory, []);
  groups.get(territory).push(zone);
}
const version = readFileSync(join(dir, "version"), "utf8").trim();
const rows = [`# Numeric Gregorian date profiles | IANA ${version}, CLDR ${process.versions.cldr}`,
  "# Territory  pattern  catalog locations", ...[...groups.keys()].sort().map(territory =>
    [territory, datePattern(territory), ...groups.get(territory)].join(" "))];
writeFileSync(new URL("date_patterns.txt", database), rows.join("\n") + "\n");
