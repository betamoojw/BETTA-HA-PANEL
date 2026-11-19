// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 betamoojw; adapted by andre68723 from commit 4c8e40e
const timezonePicker = { names: [], matches: [], index: -1, bound: false, timer: null,
  clock: NaN, received: 0, requested: -Infinity, busy: false, active: "",
  requestId: 0, appliedRequest: 0, snapshot: null, pending: null };

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

function renderTimezoneSelection(zone) {
  if (!el.settingsTimezone) return;
  el.settingsTimezone.value = zone;
  const search = document.getElementById("settingsTimezoneSearch");
  if (!search) return;
  const list = document.getElementById("settingsTimezoneOptions");
  if (list.hidden) search.value = timezoneSelectionLabel();
  else {
    // Keep the query, caret and keyboard highlight in place during device updates.
    timezonePicker.matches.forEach((name, i) => list.children[i].setAttribute("aria-selected", String(name === zone)));
  }
  document.getElementById("settingsTimezoneHint").textContent = !timezonePicker.names.length
    ? t("settings.time.list_unavailable") : timezoneSelectionLabel()
      ? t("settings.time.search_hint") : t("settings.time.legacy_hint");
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

function receiveTimezoneClock(time, request) {
  const previous = timezonePicker.snapshot;
  if (!time || timezonePicker.pending || request <= timezonePicker.appliedRequest) {
    // A full settings reload can replace the model while a newer clock is displayed.
    if (previous && editor.settings) Object.assign(editor.settings.time ||= {}, previous);
    return;
  }
  const timezone = time.active_timezone || time.timezone || previous?.timezone || "";
  const snapshot = {
    timezone, active_timezone: time.active_timezone || "",
    date_pattern: time.date_pattern, timezone_rules: time.timezone_rules,
    clock_valid: time.clock_valid, local_time: time.local_time,
  };
  timezonePicker.snapshot = snapshot;
  timezonePicker.appliedRequest = request;
  // The server's civil time reflects its actual POSIX rules, including legacy TZ.
  // UTC formatting below avoids applying the browser's own timezone a second time.
  timezonePicker.clock = time.clock_valid && /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$/.test(time.local_time || "")
    ? Date.parse(`${time.local_time}Z`) : NaN;
  timezonePicker.received = performance.now();
  timezonePicker.requested = timezonePicker.received;
  timezonePicker.active = snapshot.active_timezone;
  if (editor.settings) Object.assign(editor.settings.time ||= {}, snapshot);
  if (el.settingsTimezone?.value !== timezone) renderTimezoneSelection(timezone);
  if (previous && (previous.timezone !== timezone || previous.date_pattern !== snapshot.date_pattern ||
                   previous.timezone_rules !== snapshot.timezone_rules)) {
    renderBuildTime();
    renderFirmwareInfo();
    if (editor.activePane === "layout") renderCanvas();
  }
  updateTimezoneClock();
}

function formatTimezoneLocalTime(milliseconds) {
  return panelCivilText(new Date(milliseconds), { seconds: true });
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
  if (timezonePicker.pending || timezonePicker.busy || performance.now() - timezonePicker.requested < 5000) return;
  timezonePicker.busy = true;
  timezonePicker.requested = performance.now();
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), 4000);
  const request = ++timezonePicker.requestId;
  try {
    const clock = await apiGet("/api/settings?time_only=1", controller.signal);
    receiveTimezoneClock(clock, request);
  }
  catch { /* A stale clock becomes unavailable instead of running indefinitely. */ }
  finally { window.clearTimeout(timeout); timezonePicker.busy = false; }
}

function renderTimezoneSettings(time) {
  timezonePicker.names = Array.isArray(time.timezones) ? time.timezones.filter(name => typeof name === "string").sort() : [];
  renderTimezoneSelection(timezonePicker.pending?.zone ?? time.timezone ?? "");
  const search = document.getElementById("settingsTimezoneSearch");
  if (!search) return;
  const toggle = document.getElementById("settingsTimezoneToggle");
  search.placeholder = t("settings.time.search");
  toggle.setAttribute("aria-label", t("settings.time.show_zones"));
  search.disabled = toggle.disabled = !timezonePicker.names.length;
  document.getElementById("settingsLocalTimeLabel").textContent = t("settings.time.local_time");
  updateTimezoneClock();
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
let timezoneSaves = Promise.resolve();
function pushTimezoneLive() {
  const choice = { zone: el.settingsTimezone.value.trim() };
  timezonePicker.pending = choice;
  clearTimeout(timezoneSaveTimer);
  timezoneSaveTimer = setTimeout(() => {
    // Only one PUT may reach the panel at a time. Skip queued choices already replaced by the user.
    timezoneSaves = timezoneSaves.then(async () => {
      if (timezonePicker.pending !== choice) return;
      try {
        const saved = await putSettings({time: {timezone: choice.zone}, reboot: false});
        if (timezonePicker.pending !== choice) return;
        timezonePicker.pending = null;
        receiveTimezoneClock({ timezone: choice.zone, ...saved }, ++timezonePicker.requestId);
        if (!saved?.active_timezone) timezonePicker.requested = -Infinity;
      } catch (error) {
        if (timezonePicker.pending !== choice) return;
        timezonePicker.pending = null;
        timezonePicker.appliedRequest = ++timezonePicker.requestId;
        timezonePicker.requested = -Infinity;
        renderTimezoneSelection(timezonePicker.snapshot?.timezone || "");
        setStatus(String(error.message || error), true);
      }
    });
    return timezoneSaves;
  }, 400);
}
