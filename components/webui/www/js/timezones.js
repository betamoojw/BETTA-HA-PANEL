// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 betamoojw; adapted by andre68723 from commit 4c8e40e
const timezonePicker = { names: [], matches: [], index: -1, bound: false, timer: null,
  clock: NaN, received: 0, requested: -Infinity, busy: false, active: "" };

function timezoneSelectionLabel() {
  const value = el.settingsTimezone.value;
  return timezonePicker.names.includes(value) ? value : "";
}

function closeTimezoneOptions() {
  document.getElementById("settingsTimezoneOptions").hidden = true;
  const search = document.getElementById("settingsTimezoneSearch");
  search.setAttribute("aria-expanded", "false");
  search.removeAttribute("aria-activedescendant");
  document.getElementById("settingsTimezoneToggle").setAttribute("aria-expanded", "false");
  search.value = timezoneSelectionLabel();
}

function selectTimezone(name) {
  el.settingsTimezone.value = name;
  document.getElementById("settingsTimezoneHint").textContent = t("settings.time.search_hint");
  closeTimezoneOptions();
  el.settingsTimezone.dispatchEvent(new Event("change", { bubbles: true }));
}

function showTimezoneOptions(query = "") {
  const search = document.getElementById("settingsTimezoneSearch");
  const list = document.getElementById("settingsTimezoneOptions");
  timezonePicker.matches = timezonePicker.names.filter(name => name.toLowerCase().includes(query.trim().toLowerCase()));
  timezonePicker.index = -1;
  search.removeAttribute("aria-activedescendant");
  list.replaceChildren(...timezonePicker.matches.map((name, index) => {
    const option = document.createElement("div");
    option.className = "timezone-option";
    option.id = `timezone-option-${index}`;
    option.setAttribute("role", "option");
    option.setAttribute("aria-selected", String(name === el.settingsTimezone.value));
    option.textContent = name;
    option.onmousedown = event => event.preventDefault();
    option.onclick = () => selectTimezone(name);
    return option;
  }));
  if (!timezonePicker.matches.length) {
    const empty = document.createElement("div");
    empty.className = "timezone-empty";
    empty.setAttribute("role", "status");
    empty.textContent = t("settings.time.no_matches");
    list.append(empty);
  }
  list.hidden = false;
  search.setAttribute("aria-expanded", "true");
  document.getElementById("settingsTimezoneToggle").setAttribute("aria-expanded", "true");
}

function timezoneKeydown(event) {
  const list = document.getElementById("settingsTimezoneOptions");
  const search = document.getElementById("settingsTimezoneSearch");
  if (event.key === "Escape" || event.key === "Tab") {
    closeTimezoneOptions();
    if (event.key === "Escape") event.preventDefault();
    return;
  }
  if (event.key === "Enter" && !list.hidden) {
    event.preventDefault();
    const name = timezonePicker.matches[timezonePicker.index] ||
      (timezonePicker.matches.length === 1 ? timezonePicker.matches[0] : null);
    if (name) selectTimezone(name);
    return;
  }
  if (!["ArrowDown", "ArrowUp"].includes(event.key)) return;
  event.preventDefault();
  if (list.hidden) showTimezoneOptions();
  const count = timezonePicker.matches.length;
  if (!count) return;
  const down = event.key === "ArrowDown";
  timezonePicker.index = timezonePicker.index < 0 ? (down ? 0 : count - 1)
    : (timezonePicker.index + (down ? 1 : -1) + count) % count;
  [...list.children].forEach((option, i) => option.classList.toggle("active", i === timezonePicker.index));
  const option = list.children[timezonePicker.index];
  search.setAttribute("aria-activedescendant", option.id);
  option.scrollIntoView({ block: "nearest" });
}

function receiveTimezoneClock(time) {
  // The server's civil time reflects its actual POSIX rules, including legacy TZ.
  // UTC formatting below avoids applying the browser's own timezone a second time.
  timezonePicker.clock = time.clock_valid && /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$/.test(time.local_time || "")
    ? Date.parse(`${time.local_time}Z`) : NaN;
  timezonePicker.received = performance.now();
  timezonePicker.requested = timezonePicker.received;
  timezonePicker.active = time.active_timezone || "";
  updateTimezoneClock();
}

function formatTimezoneLocalTime(milliseconds) {
  const options = { day: "2-digit", month: "short", year: "numeric", hour: "2-digit",
    minute: "2-digit", second: "2-digit", hourCycle: "h23", timeZone: "UTC" };
  const locale = ({"zh-cn":"zh-CN", "zh-tw":"zh-TW", en:"en-GB"})[editor.settings?.ui?.language] || editor.settings?.ui?.language || "en-GB";
  let formatter;
  try { formatter = new Intl.DateTimeFormat(locale, options); }
  catch { formatter = new Intl.DateTimeFormat("en-GB", options); }
  return formatter.format(new Date(milliseconds));
}

function updateTimezoneClock() {
  if (!document.getElementById("settingsLocalTime")) return;
  const age = performance.now() - timezonePicker.received;
  const valid = Number.isFinite(timezonePicker.clock) && age < 15000;
  document.getElementById("settingsLocalTime").textContent = valid
    ? formatTimezoneLocalTime(timezonePicker.clock + age)
    : t("settings.time.clock_unavailable");
  document.getElementById("settingsActiveTimezone").textContent = timezonePicker.names.includes(timezonePicker.active)
    ? timezonePicker.active : t("settings.time.legacy_active");
}

async function tickTimezoneClock() {
  const output = document.getElementById("settingsLocalTime");
  if (!output || document.hidden || output.offsetParent === null || editor.activePane !== "settings" || editor.activeSettingsSection !== "settingsUiSection") return;
  updateTimezoneClock();
  if (timezonePicker.busy || performance.now() - timezonePicker.requested < 5000) return;
  timezonePicker.busy = true;
  timezonePicker.requested = performance.now();
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), 4000);
  try { receiveTimezoneClock(await apiGet("/api/settings?time_only=1", controller.signal)); }
  catch { /* A stale clock becomes unavailable instead of running indefinitely. */ }
  finally { window.clearTimeout(timeout); timezonePicker.busy = false; }
}

function renderTimezoneSettings(time) {
  timezonePicker.names = Array.isArray(time.timezones) ? time.timezones.filter(name => typeof name === "string").sort() : [];
  el.settingsTimezone.value = time.timezone || "";
  const search = document.getElementById("settingsTimezoneSearch");
  if (!search) return;
  const toggle = document.getElementById("settingsTimezoneToggle");
  search.placeholder = t("settings.time.search");
  toggle.setAttribute("aria-label", t("settings.time.show_zones"));
  search.disabled = toggle.disabled = !timezonePicker.names.length;
  document.getElementById("settingsLocalTimeLabel").textContent = t("settings.time.local_time");
  document.getElementById("settingsTimezoneHint").textContent = !timezonePicker.names.length
    ? t("settings.time.list_unavailable") : timezoneSelectionLabel()
      ? t("settings.time.search_hint") : t("settings.time.legacy_hint");
  closeTimezoneOptions();
  if (timezonePicker.source !== time) {
    timezonePicker.source = time;
    receiveTimezoneClock(time);
  }
  if (timezonePicker.bound) return;
  timezonePicker.bound = true;
  search.onfocus = () => { showTimezoneOptions(); search.select(); };
  search.onclick = () => { if (document.getElementById("settingsTimezoneOptions").hidden) showTimezoneOptions(); };
  search.oninput = () => showTimezoneOptions(search.value);
  search.onkeydown = timezoneKeydown;
  toggle.onclick = () => {
    if (!document.getElementById("settingsTimezoneOptions").hidden) closeTimezoneOptions();
    else { search.focus(); showTimezoneOptions(); }
  };
  document.getElementById("settingsTimezonePicker").onfocusout = event => {
    if (!event.currentTarget.contains(event.relatedTarget)) closeTimezoneOptions();
  };
  timezonePicker.timer = window.setInterval(tickTimezoneClock, 1000);
}

function renderTimezonePresets() {
  if (document.getElementById("settingsTimezoneSearch") && editor.settings?.time) renderTimezoneSettings(editor.settings.time);
}

let timezoneSaveTimer = null;
function pushTimezoneLive() {
  clearTimeout(timezoneSaveTimer);
  const timezone = el.settingsTimezone.value.trim();
  timezoneSaveTimer = setTimeout(async () => {
    try {
      await putSettings({time: {timezone}, reboot: false});
      if (editor.settings?.time) editor.settings.time.timezone = timezone;
      timezonePicker.requested = -Infinity;
    } catch (error) { setStatus(String(error.message || error), true); }
  }, 400);
}
