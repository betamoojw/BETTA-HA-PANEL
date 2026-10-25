/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
(() => {
  const node = id => document.getElementById(id);
  let loaded = false, dirty = false, busy = false, polling = false;
  let currentStatus = null, selectedObjects = [];
  const message = text => { node("knxMessage").textContent = text; };
  const controls = ["knxEnabled", "knxImport", "knxExport", "knxFormat", "knxReload", "knxSave", "knxRead", "knxWrite", "knxRefresh"];
  function buttons() {
    for (const id of controls) node(id).disabled = busy || !loaded;
    node("knxConfig").disabled = busy || !loaded;
    const o = selectedObjects[Number(node("knxObject").value)];
    node("knxRead").disabled = busy || currentStatus?.state !== "running" || !o?.readable;
    node("knxWrite").disabled = busy || currentStatus?.state !== "running" || !o?.writable;
  }
  async function request(url, method = "GET", body, raw = false) {
    const controller = new AbortController(), deadline = setTimeout(() => controller.abort(), 10000);
    try {
      const options = {method, signal: controller.signal, cache: "no-store"};
      if (body !== undefined) { options.headers = {"Content-Type":"application/json"}; options.body = body; }
      const r = await fetch(url, options);
      if (!r.ok) throw new Error((await r.text()).slice(0, 300));
      return raw ? await r.text() : await r.json();
    } finally { clearTimeout(deadline); }
  }
  function showStatus(s) {
    currentStatus = s;
    node("knxEnabled").checked = !!s.enabled;
    node("knxOperations").hidden = !s.enabled;
    const key = "knx.state." + s.state;
    node("knxStatus").textContent = t(key) + " · " + s.object_count + " " + t("knx.objects") +
      (s.last_error ? " · " + s.last_error : "");
    buttons();
  }
  function objectInfo() {
    const o = selectedObjects[Number(node("knxObject").value)];
    node("knxObjectInfo").textContent = o ?
      [o.description, o.group_address, o.datapoint_type, o.unit].filter(Boolean).join(" · ") : "";
    buttons();
  }
  function setSaved(text) {
    // Keep malformed stored content editable/exportable so it can be repaired.
    // The device remains authoritative for validation and never starts it.
    let config;
    try { config = validate(text); } catch (_) { config = null; }
    selectedObjects = config ? config.comObjects.filter(o => o && typeof o === "object") : [];
    node("knxConfig").value = config ? JSON.stringify(config, null, 2) : text; dirty = false;
    node("knxObject").replaceChildren();
    selectedObjects.forEach((o, i) => {
      const option = document.createElement("option");
      option.value = String(i); option.textContent = o.name + " (" + o.group_address + ")";
      node("knxObject").appendChild(option);
    });
    node("knxObject").value = selectedObjects.length ? "0" : "";
    objectInfo();
  }
  function validate(text) {
    if (new TextEncoder().encode(text).length > 65535) throw new Error(t("knx.invalid"));
    const c = JSON.parse(text);
    if (!c || Array.isArray(c) || c.media_type !== "knx_ip" || !c.media_parameters ||
        !Array.isArray(c.comObjects) || c.comObjects.length > 128) throw new Error(t("knx.invalid"));
    return c;
  }
  async function action(fn) {
    if (busy || polling) return;
    busy = true; buttons();
    try { await fn(); }
    catch (e) { message(t("knx.failed") + ": " + (e.message || e)); }
    finally { busy = false; buttons(); }
  }
  async function poll() {
    const section = node("settingsKnxSection");
    if (!section || section.classList.contains("hidden") || document.hidden || polling || busy) return;
    polling = true;
    // Block mutations while a status read is pending, rather than let a stale
    // poll undo a newer enable/disable result.
    for (const id of controls) node(id).disabled = true;
    try {
      if (!loaded) {
        const config = await request("/api/knx/config", "GET", undefined, true);
        setSaved(config); loaded = true;
      }
      showStatus(await request("/api/knx"));
    } catch (e) { message(t("knx.failed") + ": " + (e.message || e)); }
    finally { polling = false; buttons(); }
  }
  node("knxConfig").addEventListener("input", () => { dirty = true; message(t("knx.unsaved")); });
  node("knxEnabled").addEventListener("change", () => action(async () => {
    const enabled = node("knxEnabled").checked;
    try { showStatus(await request("/api/knx", "PUT", JSON.stringify({enabled}))); }
    catch (e) { node("knxEnabled").checked = !!currentStatus?.enabled; throw e; }
  }));
  node("knxSave").addEventListener("click", () => action(async () => {
    validate(node("knxConfig").value);
    showStatus(await request("/api/knx/config", "PUT", node("knxConfig").value));
    setSaved(await request("/api/knx/config", "GET", undefined, true));
    message(t("knx.saved"));
  }));
  node("knxReload").addEventListener("click", () => action(async () => {
    if (dirty && !window.confirm(t("knx.discard"))) return;
    setSaved(await request("/api/knx/config", "GET", undefined, true)); message("");
  }));
  node("knxFormat").addEventListener("click", () => action(async () => {
    node("knxConfig").value = JSON.stringify(validate(node("knxConfig").value), null, 2);
    dirty = true; message(t("knx.unsaved"));
  }));
  node("knxImport").addEventListener("click", () => node("knxFile").click());
  node("knxFile").addEventListener("change", () => action(async () => {
    const file = node("knxFile").files?.[0]; if (!file) return;
    try {
      if (file.size > 65535) throw new Error(t("knx.invalid"));
      const text = await file.text(); validate(text);
      if (dirty && !window.confirm(t("knx.discard"))) return;
      node("knxConfig").value = text; dirty = true; message(t("knx.unsaved"));
    } finally { node("knxFile").value = ""; }
  }));
  node("knxExport").addEventListener("click", () => action(async () => {
    const url = URL.createObjectURL(new Blob([node("knxConfig").value], {type:"application/json"}));
    const a = document.createElement("a"); a.href = url; a.download = "knxConfig.json"; a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }));
  node("knxObject").addEventListener("change", objectInfo);
  async function command(write) {
    const o = selectedObjects[Number(node("knxObject").value)];
    if (!o || !(write ? o.writable : o.readable)) throw new Error(t("knx.invalid"));
    const body = {object_id:o.id, action:write ? "write":"read"};
    if (write) body.value = node("knxValue").value;
    const result = await request("/api/knx/command", "POST", JSON.stringify(body));
    message(result.local_only ? result.message : t("knx.queued"));
  }
  node("knxRead").addEventListener("click", () => action(() => command(false)));
  node("knxWrite").addEventListener("click", () => action(() => command(true)));
  node("knxRefresh").addEventListener("click", () => action(async () => {
    const o = selectedObjects[Number(node("knxObject").value)];
    if (!o) return;
    const offset = Math.floor(Number(node("knxObject").value)/20)*20;
    const r = await request("/api/knx/objects?offset=" + offset);
    const value = r.objects.find(item => item.id === o.id);
    node("knxCache").textContent = value ? JSON.stringify(value,null,2) : t("knx.unknown");
  }));
  setInterval(poll, 3000);
  buttons();
})();
