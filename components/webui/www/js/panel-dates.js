// SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
// A civil date (e.g. an HA date entity) has no offset. Only convert actual instants.
function panelDateText(year, month, day, compact = false) {
  const pattern = editor.settings?.time?.date_pattern || "Y-M-D";
  const fields = { Y: String(year).padStart(4, "0"), M: String(month).padStart(2, "0"), D: String(day).padStart(2, "0") };
  return pattern.split(pattern[1]).filter(field => !compact || field !== "Y").map(field => fields[field]).join(pattern[1]);
}

// ESP-IDF build metadata is compiler wall time with no source offset.
function panelBuildDate(raw) {
  const match = /^([A-Z][a-z]{2}) +(\d{1,2}) +(\d{4})$/.exec(raw || "");
  if (!match) return raw;
  const month = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"].indexOf(match[1]) + 1;
  return month ? panelDateText(match[3], month, match[2]) : raw;
}

// The panel applies recurring POSIX rules, including to older timestamps. Using those
// same rules also covers legacy configurations that have no browser/IANA zone name.
function panelOffsetSeconds(text) {
  const [hours, minutes = 0, seconds = 0] = text.replace(/^[+-]/, "").split(":").map(Number);
  return (text.startsWith("-") ? -1 : 1) * (hours * 3600 + minutes * 60 + seconds);
}

function panelTransition(year, rule, offsetBefore) {
  const [date, clock = "2"] = rule.split("/");
  let day;
  if (date.startsWith("M")) {
    const [month, week, weekday] = date.slice(1).split(".").map(Number);
    const first = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
    let target = 1 + (weekday - first + 7) % 7 + (week - 1) * 7;
    if (target > new Date(Date.UTC(year, month, 0)).getUTCDate()) target -= 7;
    day = Date.UTC(year, month - 1, target);
  } else {
    // J1..365 skips leap day. The zero-based Julian form includes it.
    let index = Number(date.replace(/^J/, ""));
    if (date.startsWith("J")) {
      if (index >= 60 && new Date(Date.UTC(year, 1, 29)).getUTCMonth() === 1) index++;
      index--;
    }
    day = Date.UTC(year, 0, 1 + index);
  }
  return day + (panelOffsetSeconds(clock) + offsetBefore) * 1000;
}

let panelZoneCache;
function panelZoneRules(source) {
  if (panelZoneCache?.source === source) return panelZoneCache.rules;
  const [names, start = "M3.2.0", end = "M11.1.0"] = source.split(",");
  const match = /^(?:<[^>]+>|[a-zA-Z]{3,})([+-]?\d+(?::\d+(?::\d+)?)?)(?:(<[^>]+>|[a-zA-Z]{3,})([+-]?\d+(?::\d+(?::\d+)?)?)?)?$/.exec(names);
  const transition = /^(?:M\d{1,2}\.[1-5]\.[0-6]|J?\d{1,3})(?:\/[+-]?\d+(?::\d+(?::\d+)?)?)?$/;
  let rules = null;
  if (match && transition.test(start) && transition.test(end)) {
    const standard = panelOffsetSeconds(match[1]);
    const daylight = match[3] ? panelOffsetSeconds(match[3]) : standard - 3600;
    rules = { standard, daylight: match[2] ? daylight : standard, start, end };
  }
  panelZoneCache = { source, rules };
  return rules;
}

// UTC accessors on this value return the panel's civil fields. Never use its epoch
// for elapsed durations. The settings clock already arrives as civil fields.
function panelCivilTime(milliseconds) {
  const time = editor.settings?.time || {};
  const rules = panelZoneRules(time.timezone_rules || "");
  if (rules) {
    let offset = rules.standard;
    if (rules.standard !== rules.daylight) {
      const year = new Date(milliseconds - offset * 1000).getUTCFullYear();
      const start = panelTransition(year, rules.start, rules.standard);
      const end = panelTransition(year, rules.end, rules.daylight);
      const summer = start < end ? milliseconds >= start && milliseconds < end : milliseconds >= start || milliseconds < end;
      if (summer) offset = rules.daylight;
    }
    return new Date(milliseconds - offset * 1000);
  }
  // Older/offline API responses may only provide an IANA name.
  const zone = time.active_timezone || time.timezone;
  if (zone) {
    try {
      const parts = new Intl.DateTimeFormat("en-GB", { timeZone: zone, year: "numeric", month: "2-digit",
        day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit", hourCycle: "h23" }).formatToParts(milliseconds);
      const p = Object.fromEntries(parts.map(part => [part.type, part.value]));
      return new Date(Date.UTC(+p.year, +p.month - 1, +p.day, +p.hour, +p.minute, +p.second));
    } catch { /* A location-free/unknown timezone uses UTC. */ }
  }
  return new Date(milliseconds);
}

function panelCivilText(civil, { compact = false, seconds = false } = {}) {
  if (!Number.isFinite(civil.getTime())) return "";
  const date = panelDateText(civil.getUTCFullYear(), civil.getUTCMonth() + 1, civil.getUTCDate(), compact);
  let clock = civil.toISOString().slice(11, seconds ? 19 : 16);
  if (editor.settings?.hardware?.clock?.format_24h === false) {
    const hour = civil.getUTCHours();
    clock = String(hour % 12 || 12).padStart(2, "0") + clock.slice(2) + (hour < 12 ? " AM" : " PM");
  }
  return date + " " + clock;
}
