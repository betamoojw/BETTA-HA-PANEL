/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
// Shared by login, setup, editor and viewer; never reads identity or changes device state.
let licenseWarningState = null, licenseWarningAbort = null, licenseWarningEpoch = 0;
let licenseWarningTimer = null, licenseWarningObserver = null, licenseWarningStarted = false;
let licenseWarningAttempt = -Infinity;
const licenseWarningStates = ["missing", "valid", "invalid", "error", "unconfigured"];

function licenseWarningMeasure() {
  const banner = document.getElementById("licenseWarning");
  if (!banner) return;
  const height = banner.hidden ? 0 : Math.ceil(banner.getBoundingClientRect().height);
  const style = document.documentElement.style;
  if (style.getPropertyValue("--license-warning-height") === `${height}px`) return;
  style.setProperty("--license-warning-height", `${height}px`);
  if (typeof scheduleInspectorReflow === "function") scheduleInspectorReflow();
}
function licenseWarningRender() {
  const banner = document.getElementById("licenseWarning");
  if (!banner) return;
  banner.hidden = licenseWarningState === null || licenseWarningState === "valid";
  const text = banner.querySelector("span");
  const message = banner.hidden ? "" : `${t(`license.${licenseWarningState}`)} — ${t("license.warning_usage")}`;
  if (text.textContent !== message) text.textContent = message;
  licenseWarningMeasure();
}
function licenseWarningUpdate(state) {
  // An authoritative import/remove response must win over an older public GET.
  ++licenseWarningEpoch; licenseWarningAbort?.abort(); licenseWarningAbort = null;
  licenseWarningState = licenseWarningStates.includes(state) ? state : "unknown";
  licenseWarningAttempt = performance.now();
  licenseWarningRender();
}
async function licenseWarningRefresh() {
  if (licenseWarningAbort || document.hidden || document.body.dataset.localUi) return;
  const epoch = ++licenseWarningEpoch, abort = new AbortController();
  licenseWarningAbort = abort; licenseWarningAttempt = performance.now();
  const timeout = window.setTimeout(() => abort.abort(), 10000);
  try {
    const response = await fetch("/api/license/status", { signal: abort.signal, cache: "no-store" });
    if (!response.ok) throw new Error("status");
    const state = (await response.json()).key_state;
    if (epoch !== licenseWarningEpoch) return;
    licenseWarningState = licenseWarningStates.includes(state) ? state : "unknown";
  } catch (_) {
    // A failed request cannot establish that a device has no license.
    if (epoch === licenseWarningEpoch) licenseWarningState = "unknown";
  } finally {
    window.clearTimeout(timeout);
    if (epoch === licenseWarningEpoch) { licenseWarningAbort = null; licenseWarningRender(); }
  }
}
function licenseWarningResume() {
  if (!document.hidden && performance.now() - licenseWarningAttempt >= 60000) void licenseWarningRefresh();
}
function licenseWarningStop() {
  ++licenseWarningEpoch; licenseWarningAbort?.abort(); licenseWarningAbort = null;
  window.clearInterval(licenseWarningTimer); licenseWarningTimer = null;
  licenseWarningObserver?.disconnect(); licenseWarningObserver = null;
  document.removeEventListener("visibilitychange", licenseWarningResume);
  window.removeEventListener("resize", licenseWarningMeasure);
  window.removeEventListener("pagehide", licenseWarningStop);
  licenseWarningStarted = false;
}
function licenseWarningStart() {
  if (licenseWarningStarted || document.body.dataset.localUi) return;
  licenseWarningStarted = true;
  const banner = document.getElementById("licenseWarning");
  if (typeof ResizeObserver === "function") {
    licenseWarningObserver = new ResizeObserver(licenseWarningMeasure);
    licenseWarningObserver.observe(banner);
  }
  window.addEventListener("resize", licenseWarningMeasure);
  window.addEventListener("pagehide", licenseWarningStop);
  document.addEventListener("visibilitychange", licenseWarningResume);
  licenseWarningTimer = window.setInterval(licenseWarningResume, 60000);
  void licenseWarningRefresh();
}
window.addEventListener("pageshow", licenseWarningStart);
