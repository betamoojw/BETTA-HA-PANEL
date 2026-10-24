/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
(() => {
  const node = (id) => document.getElementById(id);
  let busy = 0, talking = false, pending = Promise.resolve();
  async function refresh() {
    const section = node("settingsXiaozhiSection");
    if (!section || section.classList.contains("hidden") || document.hidden) { stop(); return; }
    if (busy) return;
    try {
      const s = await apiGet("/api/xiaozhi");
      node("xiaozhiEnabled").checked = s.enabled;
      node("xiaozhiEnabled").disabled = !s.supported;
      node("xiaozhiReset").disabled = !s.supported;
      node("xiaozhiFields").hidden = !s.enabled || !s.supported;
      node("xiaozhiTalk").disabled = !s.paired;
      if (document.activeElement !== node("xiaozhiVolume")) node("xiaozhiVolume").value = s.volume;
      node("xiaozhiCode").textContent = s.code;
      node("xiaozhiText").textContent = s.text;
      const key = `xiaozhi.state.${s.state}`;
      node("xiaozhiStatus").textContent = t(key) === key ? s.state : t(key);
    } catch (_) { node("xiaozhiStatus").textContent = t("xiaozhi.state.connection_error"); }
  }
  function command(action, value = 0) {
    busy++;
    // Keep rapid edits and start/release in order; never silently discard an edit.
    pending = pending.then(async () => {
      try {
        const response = await fetch("/api/xiaozhi", {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({action, value})});
        if (!response.ok) throw new Error(t("xiaozhi.failed"));
      } catch (_) { node("xiaozhiStatus").textContent = t("xiaozhi.failed"); }
      finally { busy--; }
    });
    return pending;
  }
  function stop() { if (talking) { talking = false; return command("stop"); } }
  function start(event) {
    if (event.type === "keydown" && ![" ", "Enter"].includes(event.key)) return;
    if (talking || node("xiaozhiTalk").disabled) return;
    event.preventDefault(); talking = true; return command("start");
  }
  node("xiaozhiEnabled").addEventListener("change", (e) => command(e.target.checked ? "enable" : "disable"));
  node("xiaozhiPair").addEventListener("click", () => command("pair"));
  node("xiaozhiReset").addEventListener("click", () => command("reset"));
  node("xiaozhiStop").addEventListener("click", () => { talking = false; command("disconnect"); });
  node("xiaozhiVolume").addEventListener("change", (e) => command("volume", Number(e.target.value)));
  node("xiaozhiTalk").addEventListener("pointerdown", start);
  node("xiaozhiTalk").addEventListener("keydown", start);
  node("xiaozhiTalk").addEventListener("keyup", stop);
  window.addEventListener("pointerup", stop);
  window.addEventListener("pointercancel", stop);
  window.addEventListener("blur", stop);
  document.addEventListener("visibilitychange", () => { if (document.hidden) stop(); });
  setInterval(refresh, 1000);
})();
