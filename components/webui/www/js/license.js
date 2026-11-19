/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
let licenseState = null, licenseBusy = false, licensePoll = null, licenseAbort = null;
let licenseBound = false, licenseVisible = false, licenseEpoch = 0, licenseLoadedAt = 0, licenseAttemptAt = 0;
let licenseMessageKey = "";
const licenseNode = id => document.getElementById(id);

function licenseSetMessage(key) {
  licenseMessageKey = key;
  licenseNode("licenseMessage").textContent = key ? t(key) : "";
}
function licenseRenderFile() {
  licenseNode("licenseFileName").textContent = licenseNode("licenseFile").files?.[0]?.name || t("license.no_file");
}

async function licenseCopyDeviceId() {
  const field = licenseNode("licenseDevice"), text = field.value, epoch = licenseEpoch;
  if (!text || !licenseVisible || !userAccess.authenticated || userAccess.local) return;
  let copied = false;
  try {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(text);
      copied = true;
    }
  } catch (_) { /* Permission denied: try the selected-input compatibility path. */ }
  if (epoch !== licenseEpoch || !licenseVisible || !userAccess.authenticated || field.value !== text) return;
  if (!copied) {
    // LAN HTTP does not expose the secure-context Clipboard API. Keep this
    // fallback synchronous with the click when that API is absent.
    field.focus({ preventScroll: true });
    field.select();
    field.setSelectionRange(0, text.length);
    try { copied = document.execCommand?.("copy") === true; } catch (_) { /* Manual copy remains available. */ }
  }
  licenseSetMessage(copied ? "license.copied" : "license.copy_failed");
}

function licenseRemaining() {
  return Math.max(0, Math.ceil((Number(licenseState?.trial_seconds) || 0) - (performance.now() - licenseLoadedAt) / 1000));
}
function licenseRenderTime() {
  const seconds = licenseRemaining(), valid = licenseState?.key_state === "valid";
  const time = [Math.floor(seconds / 3600), Math.floor(seconds / 60) % 60, seconds % 60]
    .map(value => String(value).padStart(2, "0")).join(":");
  licenseNode("licenseTrial").textContent = !licenseState || valid ? "" : seconds > 0
    ? t("license.trial", { time }) : t("license.expired");
  const badge = !licenseState ? "unknown" : valid ? "activated" : seconds > 0 ? "trial_badge" : "ended";
  licenseNode("licenseBadge").textContent = t(`license.${badge}`);
}
function licenseAgreementKey() {
  if (!licenseState?.device_id || userAccess.local || !userAccess.authenticated) return null;
  return `betta-license-read:${LICENSE_AGREEMENT.revision}:${licenseState.device_id}:${encodeURIComponent(userAccess.username || "@open")}`;
}
function licenseAgreementRender() {
  const key = licenseAgreementKey(), checkbox = licenseNode("licenseAgreementRead");
  checkbox.disabled = !key;
  try {
    const record = key ? JSON.parse(window.localStorage.getItem(key) || "null") : null;
    checkbox.checked = !!record?.readAt;
    licenseNode("licenseAgreementMessage").textContent = record?.readAt
      ? t("license.read_saved", { date: panelCivilText(panelCivilTime(Date.parse(record.readAt)), { seconds: true }) }) : "";
  } catch (_) {
    checkbox.checked = false;
    licenseNode("licenseAgreementMessage").textContent = t("license.read_failed");
  }
}
function licenseAgreementChange() {
  const key = licenseAgreementKey();
  if (!key) { licenseAgreementRender(); return; }
  try {
    if (licenseNode("licenseAgreementRead").checked) window.localStorage.setItem(key, JSON.stringify({ readAt: new Date().toISOString() }));
    else window.localStorage.removeItem(key);
    licenseAgreementRender();
  } catch (_) {
    licenseAgreementRender();
    licenseNode("licenseAgreementMessage").textContent = t("license.read_failed");
  }
}
function licenseRender() {
  if (!licenseBound) return;
  const state = licenseState?.key_state;
  const known = ["valid", "missing", "invalid", "error", "unconfigured"].includes(state) ? state : "error";
  licenseNode("licenseStatus").textContent = userAccess.local ? t("license.local")
    : licenseState ? t(`license.${known}`) : t("license.unknown");
  licenseNode("licenseBanner").dataset.state = licenseState ? known : "unknown";
  licenseNode("licenseBadge").dataset.state = licenseState ? known : "unknown";
  licenseNode("licenseDevice").value = licenseState?.device_id || "";
  licenseNode("licenseCopy").disabled = !licenseState?.device_id;
  licenseNode("licenseActions").hidden = !userIsAdmin();
  licenseNode("licenseAdminHint").hidden = userIsAdmin();
  licenseNode("licenseActivation").hidden = state === "valid";
  // Choosing a local file needs no issuer key; verification and storage do.
  const cannotEdit = licenseBusy || !licenseState || !userAccess.authenticated || !userIsAdmin() || userAccess.local;
  for (const id of ["licenseKey", "licenseFile", "licenseChooseFile"]) licenseNode(id).disabled = cannotEdit;
  licenseNode("licenseImport").disabled = cannotEdit || state === "unconfigured";
  licenseNode("licenseIssuerHint").hidden = state !== "unconfigured";
  licenseNode("licenseRemove").hidden = !["valid", "invalid", "error"].includes(state);
  licenseNode("licenseRemove").disabled = licenseBusy || !userIsAdmin() || userAccess.local;
  licenseNode("licenseRefresh").disabled = licenseBusy || userAccess.local;
  licenseSetMessage(licenseMessageKey);
  licenseRenderFile();
  licenseRenderTime(); licenseAgreementRender();
}
async function licenseRequest(method = "GET", body) {
  const abort = new AbortController(), timer = window.setTimeout(() => abort.abort(), 10000);
  licenseAbort = abort;
  try {
    const response = await fetch("/api/license", { method, body, signal: abort.signal,
      headers: method === "POST" ? { "Content-Type": "application/octet-stream" } : {} });
    if (!response.ok) throw new Error(response.status === 400 ? "license.invalid_file" : response.status === 403 ? "users.denied" : "license.unavailable");
    return await response.json();
  } finally {
    window.clearTimeout(timer);
    if (licenseAbort === abort) licenseAbort = null;
  }
}
async function licenseLoad() {
  if (licenseBusy || !userAccess.authenticated || userAccess.local) return;
  const epoch = licenseEpoch;
  licenseAttemptAt = performance.now();
  licenseBusy = true; licenseRender();
  try {
    const result = await licenseRequest();
    if (epoch !== licenseEpoch || !userAccess.authenticated) return;
    licenseState = result; licenseLoadedAt = performance.now();
    licenseWarningUpdate(result.key_state);
    licenseSetMessage("");
  } catch (_) {
    if (epoch === licenseEpoch) licenseSetMessage("license.unavailable");
  } finally {
    if (epoch === licenseEpoch) { licenseBusy = false; licenseRender(); }
  }
}
async function licenseChange(remove = false) {
  if (licenseBusy || !userAccess.authenticated || !userIsAdmin() || userAccess.local || !licenseState) return;
  if (!remove && licenseState.key_state === "unconfigured") return;
  if (remove && !window.confirm(t("license.confirm_remove"))) return;
  const epoch = licenseEpoch;
  licenseBusy = true; licenseRender();
  try {
    let text;
    if (!remove) {
      const file = licenseNode("licenseFile").files?.[0];
      if (file && (file.size < 200 || file.size > 202)) throw new Error("license.invalid_file");
      text = file ? await file.text() : licenseNode("licenseKey").value;
      if (!/^BETTA1:[0-9a-f]{64}:[0-9a-f]{128}(?:\r?\n)?$/.test(text)) throw new Error("license.invalid_file");
      if (epoch !== licenseEpoch || !userAccess.authenticated || !userIsAdmin()) return;
    }
    const result = await licenseRequest(remove ? "DELETE" : "POST", text);
    if (epoch !== licenseEpoch || !userAccess.authenticated) return;
    licenseState = result; licenseLoadedAt = performance.now();
    licenseWarningUpdate(result.key_state);
    licenseSetMessage(remove ? "license.removed" : "license.saved");
  } catch (error) {
    if (epoch === licenseEpoch) licenseSetMessage(["license.invalid_file", "users.denied"].includes(error?.message)
      ? error.message : "license.unavailable");
  } finally {
    if (epoch === licenseEpoch) { licenseClearInput(); licenseBusy = false; licenseRender(); }
  }
}
function licenseClearInput() {
  licenseNode("licenseFile").value = "";
  licenseNode("licenseKey").value = "";
  licenseRenderFile();
}
function licenseSetVisible(visible) {
  if (!licenseBound || licenseVisible === visible) return;
  licenseVisible = visible;
  if (visible) { void licenseLoad(); return; }
  ++licenseEpoch; licenseAbort?.abort(); licenseBusy = false;
  licenseClearInput();
}
function licenseReset() {
  if (!licenseBound) return;
  licenseSetVisible(false);
  ++licenseEpoch; licenseAbort?.abort(); licenseBusy = false; licenseState = null;
  window.clearInterval(licensePoll); licensePoll = null;
  licenseSetMessage("");
  licenseClearInput(); licenseRender();
}
function licenseSelectTab(agreement, focus = false) {
  for (const [id, selected] of [["licenseTab", !agreement], ["agreementTab", agreement]]) {
    const tab = licenseNode(id);
    tab.setAttribute("aria-selected", String(selected)); tab.tabIndex = selected ? 0 : -1;
    if (selected && focus) tab.focus();
  }
  licenseNode("licensePanel").hidden = agreement;
  licenseNode("agreementPanel").hidden = !agreement;
}
function licenseBind() {
  if (licenseBound) return;
  licenseBound = true;
  licenseNode("licenseAgreementText").textContent = LICENSE_AGREEMENT.text;
  licenseNode("licenseAgreementRead").onchange = licenseAgreementChange;
  licenseNode("licenseTab").onclick = () => licenseSelectTab(false);
  licenseNode("agreementTab").onclick = () => licenseSelectTab(true);
  for (const id of ["licenseTab", "agreementTab"]) licenseNode(id).onkeydown = event => {
    if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
    event.preventDefault();
    licenseSelectTab(event.key === "End" || (event.key !== "Home" && id === "licenseTab"), true);
  };
  licenseNode("licenseRefresh").onclick = licenseLoad;
  licenseNode("licenseImport").onclick = () => licenseChange();
  licenseNode("licenseRemove").onclick = () => licenseChange(true);
  licenseNode("licenseChooseFile").onclick = () => licenseNode("licenseFile").click();
  licenseNode("licenseFile").onchange = () => {
    if (licenseNode("licenseFile").files?.length) licenseNode("licenseKey").value = "";
    licenseRenderFile();
  };
  licenseNode("licenseKey").oninput = () => { licenseNode("licenseFile").value = ""; licenseRenderFile(); };
  licenseNode("licenseCopy").onclick = licenseCopyDeviceId;
  // Read-only roles reuse the section without bootstrapping the editor or admin settings.
  if (!userCanEdit()) {
    licenseNode("userLicenseMount").appendChild(licenseNode("settingsLicenseSection"));
    const showSettings = visible => {
      licenseNode("userViewerScreen").hidden = visible;
      licenseNode("userViewerSettings").hidden = !visible;
      licenseSetVisible(visible);
    };
    licenseNode("userSettingsOpen").onclick = () => showSettings(true);
    licenseNode("userSettingsClose").onclick = () => showSettings(false);
    licenseNode("userLicenseNav").onclick = () => licenseSelectTab(false, true);
  }
  licenseRender();
  licenseSetVisible(userCanEdit() && editor.activePane === "settings" && editor.activeSettingsSection === "settingsLicenseSection");
  if (!userAccess.local) licensePoll = window.setInterval(() => {
    if (!licenseVisible || document.hidden || !userAccess.authenticated) return;
    if (!licenseBusy && performance.now() - licenseAttemptAt >= 60000) void licenseLoad();
    licenseRenderTime();
  }, 1000);
}
