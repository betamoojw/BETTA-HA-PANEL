/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
const CLOCK_SETTINGS = [
  ["Analog", "analog", false, null, null, "clock_analog"],
  ["Start", "start_seconds", 60, 15, 7200, "clock_start"],
  ["Brightness", "brightness", 5, 1, 100, "clock_brightness"],
  ["Off", "off_enabled", true, null, null, "clock_off"],
  ["OffSeconds", "off_seconds", 300, 30, 7200, "clock_off_seconds"],
  ["24h", "format_24h", true, null, null, "clock_24h"],
  ["Seconds", "show_seconds", true, null, null, "clock_seconds"],
  ["Date", "show_date", true, null, null, "clock_date"],
];

function fillClockSettings(clock = {}) {
  CLOCK_SETTINGS.forEach(([id, key, fallback, min, max]) => {
    const control = el[`settingsClock${id}`];
    if (!control) return;
    if (min == null) control.checked = clock[key] ?? fallback;
    else control.value = String(clamp(Number(clock[key] ?? fallback), min, max));
  });
  updateClockSettingsVisibility();
}

function readClockSettings() {
  return Object.fromEntries(CLOCK_SETTINGS.map(([id, key, fallback, min, max]) => {
    const control = el[`settingsClock${id}`];
    return [key, min == null ? (control?.checked ?? fallback) :
      clamp(Math.round(Number(control?.value || fallback)), min, max)];
  }));
}

function updateClockSettingsVisibility() {
  if (el.settingsClockFields) el.settingsClockFields.hidden = el.settingsSaverMode?.value !== "clock";
  if (el.settingsClockOffSecondsRow) el.settingsClockOffSecondsRow.hidden = !el.settingsClockOff?.checked;
}

function bindClockSettings() {
  CLOCK_SETTINGS.forEach(([id]) => el[`settingsClock${id}`]?.addEventListener("change", () => {
    updateClockSettingsVisibility();
    pushHardwareLive();
  }));
}

function translateClockSettings() {
  CLOCK_SETTINGS.forEach(([id, , , , , key]) => setTextById(`settingsClock${id}Label`, `settings.screen.${key}`));
  setSelectOptionText(el.settingsSaverMode, "clock", "settings.screen.clock");
}
