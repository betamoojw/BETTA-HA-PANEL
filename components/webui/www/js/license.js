/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
let licenseState = null, licenseBusy = false, licensePoll = null;
const licenseNode = id => document.getElementById(id);

function licenseRender() {
  if (!licenseState) return;
  const state = licenseState.key_state;
  const known = ["valid", "missing", "invalid", "error", "unconfigured"].includes(state) ? state : "error";
  licenseNode("licenseStatus").textContent = t(`license.${known}`);
  licenseNode("licenseOpen").textContent = `${t("license.title")}: ${t(`license.${known}`)}`;
  licenseNode("licenseDevice").value = licenseState.device_id || "";
  const seconds = Math.max(0, Number(licenseState.trial_seconds) || 0);
  licenseNode("licenseTrial").textContent = state === "valid" ? "" : seconds > 0
    ? t("license.trial", { minutes: Math.ceil(seconds / 60) }) : t("license.expired");
  licenseNode("licenseActions").hidden = !userIsAdmin();
  licenseNode("licenseAdminHint").hidden = userIsAdmin();
  licenseNode("licenseImport").disabled = licenseBusy || !userIsAdmin() || state === "unconfigured" || userAccess.local;
  licenseNode("licenseRemove").disabled = licenseBusy || !userIsAdmin() || userAccess.local;
}

async function licenseRequest(method = "GET", body) {
  const abort = new AbortController(), timer = window.setTimeout(() => abort.abort(), 10000);
  try {
    const response = await fetch("/api/license", { method, body, signal: abort.signal,
      headers: method === "POST" ? { "Content-Type": "application/octet-stream" } : {} });
    if (!response.ok) throw new Error(t(response.status === 400 ? "license.invalid_file" : response.status === 403
      ? "users.denied" : "license.unavailable"));
    return await response.json();
  } finally { window.clearTimeout(timer); }
}

async function licenseLoad() {
  if (licenseBusy || !userAccess.authenticated || userAccess.local) return;
  licenseBusy = true;
  try {
    licenseState = await licenseRequest();
    if (!userAccess.authenticated) { licenseState = null; return; }
    licenseNode("licenseMessage").textContent = "";
  } catch (_) { licenseNode("licenseMessage").textContent = t("license.unavailable"); }
  finally { licenseBusy = false; licenseRender(); }
}

async function licenseChange(remove = false) {
  if (licenseBusy || !userIsAdmin() || userAccess.local) return;
  if (remove && !window.confirm(t("license.confirm_remove"))) return;
  licenseBusy = true; licenseRender();
  try {
    let text;
    if (!remove) {
      const file = licenseNode("licenseFile").files?.[0];
      if (!file || file.size < 200 || file.size > 202) throw new Error(t("license.invalid_file"));
      text = await file.text();
      if (!/^BETTA1:[0-9a-f]{64}:[0-9a-f]{128}(?:\r?\n)?$/.test(text)) throw new Error(t("license.invalid_file"));
    }
    const result = await licenseRequest(remove ? "DELETE" : "POST", text);
    if (!userAccess.authenticated) return;
    licenseState = result;
    licenseNode("licenseMessage").textContent = t(remove ? "license.removed" : "license.saved");
  } catch (error) { licenseNode("licenseMessage").textContent = error.name === "AbortError" ? t("license.unavailable") : error.message; }
  finally { licenseNode("licenseFile").value = ""; licenseBusy = false; licenseRender(); }
}

function licenseBind() {
  licenseNode("licenseOpen").onclick = () => {
    licenseNode("licenseDialog").showModal(); void licenseLoad();
  };
  licenseNode("licenseClose").onclick = () => licenseNode("licenseDialog").close();
  licenseNode("licenseDialog").addEventListener("close", () => { licenseNode("licenseFile").value = ""; });
  licenseNode("licenseRefresh").onclick = licenseLoad;
  licenseNode("licenseImport").onclick = () => licenseChange();
  licenseNode("licenseRemove").onclick = () => licenseChange(true);
  licenseNode("licenseCopy").onclick = async () => {
    try {
      await navigator.clipboard.writeText(licenseNode("licenseDevice").value);
      licenseNode("licenseMessage").textContent = t("license.copied");
    } catch (_) {
      licenseNode("licenseDevice").select(); licenseNode("licenseMessage").textContent = t("license.copy_failed");
    }
  };
  void licenseLoad();
  if (!licensePoll && !userAccess.local) licensePoll = window.setInterval(() => {
    if (!document.hidden) void licenseLoad();
  }, 60000);
}
