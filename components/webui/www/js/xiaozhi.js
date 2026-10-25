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
      node("xiaozhiWake").checked = !!s.wake_enabled;
      node("xiaozhiWakeHint").hidden = !s.wake_enabled;
      node("xiaozhiTalk").hidden = !!s.wake_enabled && !!s.wake_ready;
      if (document.activeElement !== node("xiaozhiTimeout")) node("xiaozhiTimeout").value = s.dialog_timeout || 30;
      if (document.activeElement !== node("xiaozhiEmoji")) node("xiaozhiEmoji").value = s.emoji_style === 1 ? 1 : 0;
      node("xiaozhiRecognized").textContent = s.recognized || "";
      node("xiaozhiText").textContent = s.reply ?? s.text;
      const emotions = {neutral:"😐",happy:"🙂",laughing:"😆",funny:"😂",sad:"😔",angry:"😠",crying:"😭",loving:"😍",embarrassed:"😳",surprised:"😮",shocked:"😱",thinking:"🤔",winking:"😉",cool:"😎",relaxed:"😌",delicious:"😋",kissy:"😘",confident:"😏",sleepy:"😴",silly:"😜",confused:"😕"};
      node("xiaozhiEmotion").textContent = emotions[s.emotion] || emotions.neutral;
      const otto = s.supported && s.enabled && s.emoji_style !== 1;
      node("xiaozhiEmotion").hidden = otto;
      node("xiaozhiOtto").hidden = !otto;
      const emotion = Object.hasOwn(emotions, s.emotion) ? s.emotion : "neutral";
      const src = `/api/xiaozhi/emoji?emotion=${emotion}`;
      if (otto && node("xiaozhiOtto").getAttribute("src") !== src) node("xiaozhiOtto").setAttribute("src", src);
      if (!otto) node("xiaozhiOtto").removeAttribute("src");
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
    if (talking || node("xiaozhiTalk").disabled || node("xiaozhiTalk").hidden) return;
    event.preventDefault(); talking = true; return command("start");
  }
  node("xiaozhiEnabled").addEventListener("change", (e) => command(e.target.checked ? "enable" : "disable"));
  node("xiaozhiWake").addEventListener("change", (e) => command("wake", e.target.checked ? 1 : 0));
  node("xiaozhiEmoji").addEventListener("change", (e) => {
    const value = Number(e.target.value);
    if (value === 0 || value === 1) return command("emoji", value);
  });
  node("xiaozhiTimeout").addEventListener("change", (e) => {
    const n = Number(e.target.value);
    if (Number.isInteger(n) && n >= 10 && n <= 300) return command("timeout", n);
    node("xiaozhiStatus").textContent = t("xiaozhi.failed");
  });
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
