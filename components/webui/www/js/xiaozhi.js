/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
(() => {
  const node = (id) => document.getElementById(id);
  const setText = (id, value) => {
    const element = node(id), text = String(value ?? "");
    if (element.textContent !== text) element.textContent = text;
  };
  let busy = 0, talking = false, pending = Promise.resolve(), refreshing = false, revision = 0;
  let wakeDirty = false, wakePending = null, wakeRevision = 0, pendingRevision = 0, wakePendingAt = 0;
  let savedWakePhrase = "ni hao xiao zhi";
  let voiceActive = false, spectrumAt = 0, spectrumVisible = false;
  const spectrumNow = () => typeof performance !== "undefined" ? performance.now() : Date.now();
  const spectrumBars = [];
  const spectrum = node("xiaozhiSpectrum");
  if (spectrum && document.createElement) for (let i = 0; i < 24; i++) {
    const bar = document.createElement("span");
    spectrum.appendChild(bar); spectrumBars.push(bar);
  }
  function pageVisible() {
    const section = node("settingsXiaozhiSection");
    return !document.hidden && section && !section.classList.contains("hidden") &&
      (!section.checkVisibility || section.checkVisibility());
  }
  function clearSpectrum() {
    spectrumAt = 0;
    if (!spectrumVisible) return;
    for (const bar of spectrumBars) { bar.style.height = "4px"; bar.style.opacity = "0.3"; }
    spectrumVisible = false;
  }
  function updateSpectrum(s, requestedAt) {
    if (!voiceActive || !pageVisible() || spectrumNow() - requestedAt >= 350) { clearSpectrum(); return; }
    const data = s.spectrum;
    const bands = data?.speaker_active ? data.speaker : data?.mic_active ? data.microphone : null;
    if (!Array.isArray(bands) || bands.length !== 24 || bands.some(n => !Number.isFinite(n) || n < 0 || n > 255)) {
      clearSpectrum(); return;
    }
    spectrumAt = requestedAt; spectrumVisible = true;
    spectrumBars.forEach((bar, i) => {
      // Keep the central segment and grow equally upward/downward in whole cells.
      const rows = Math.round(bands[i] / 255 * 5);
      bar.style.height = `${4 + rows * 12}px`;
      bar.style.opacity = rows ? "0.9" : "0.3";
    });
  }
  function conversationHint(s) {
    if (!s.enabled) return "";
    if (s.state === "connection_error" || s.state === "audio_or_connection_error")
      return t(s.wake_enabled && s.wake_ready ? "xiaozhi.hint_error" : "xiaozhi.talk");
    if (s.duplex_active && s.speaking && s.listening) return t("xiaozhi.hint_duplex");
    if (s.listening) return t("xiaozhi.hint_listening");
    if (s.state === "waiting" || s.state === "connecting") return t("xiaozhi.hint_thinking");
    if (s.speaking) return "";
    return t(s.wake_enabled && s.wake_ready ? "xiaozhi.hint_ready" : "xiaozhi.talk");
  }
  const normalizePhrase = (value) => value.trim().toLowerCase().replace(/\s+/g, " ");
  const validPhrase = (phrase) => phrase.length <= 95 && /^[a-z]{1,6}( [a-z]{1,6}){2,11}$/.test(phrase);
  function wakeFields() {
    node("xiaozhiCustomFields").hidden = Number(node("xiaozhiWakeWord").value) !== 3;
  }
  async function refresh() {
    const section = node("settingsXiaozhiSection");
    if (!section || !pageVisible()) {
      voiceActive = false; clearSpectrum(); stop(); node("xiaozhiOtto").removeAttribute("src"); return;
    }
    if (busy || refreshing) return;
    refreshing = true;
    const started = revision;
    const requestedAt = spectrumNow();
    const controller = new AbortController();
    const deadline = setTimeout(() => controller.abort(), 8000);
    try {
      const s = await apiGet("/api/xiaozhi", controller.signal);
      // A delayed poll must not undo a user's newer edit or re-open hidden UI.
      if (started !== revision || busy || !pageVisible()) return;
      node("xiaozhiEnabled").checked = s.enabled;
      node("xiaozhiEnabled").disabled = !s.supported;
      node("xiaozhiReset").disabled = !s.supported;
      node("xiaozhiFields").hidden = !s.enabled || !s.supported;
      node("xiaozhiTalk").disabled = !s.paired;
      if (document.activeElement !== node("xiaozhiVolume")) node("xiaozhiVolume").value = s.volume;
      node("xiaozhiCode").textContent = s.code;
      node("xiaozhiWake").checked = !!s.wake_enabled;
      node("xiaozhiDuplex").checked = !!s.duplex_enabled;
      node("xiaozhiWakeSettings").hidden = !s.wake_enabled;
      node("xiaozhiModelInstall").hidden = !!s.wake_model_ready;
      if (validPhrase(s.wake_phrase || "")) savedWakePhrase = s.wake_phrase;
      if (wakePending && s.wake_config_revision !== pendingRevision && !s.wake_config_failed &&
          s.wake_word === wakePending.word && s.wake_phrase === wakePending.phrase) {
        // A hidden, invalid custom draft must not block saving a preset or be lost.
        wakeDirty = wakePending.word !== 3 && normalizePhrase(node("xiaozhiWakePhrase").value) !== wakePending.phrase;
        wakePending = null;
        node("xiaozhiWakeResult").textContent = t("xiaozhi.wake_saved");
      } else if (wakePending && ((s.wake_config_revision !== pendingRevision && s.wake_config_failed) || Date.now() - wakePendingAt > 30000)) {
        wakePending = null;
        node("xiaozhiWakeResult").textContent = t("xiaozhi.state.wake_config_error");
      }
      if (!wakeDirty && !wakePending) {
        node("xiaozhiWakeWord").value = s.wake_word || 0;
        node("xiaozhiWakePhrase").value = s.wake_phrase || "ni hao xiao zhi";
      }
      wakeRevision = s.wake_config_revision || 0;
      node("xiaozhiWakeWord").disabled = node("xiaozhiWakePhrase").disabled = !!wakePending;
      node("xiaozhiWakeSave").disabled = !!wakePending;
      wakeFields();
      node("xiaozhiWakeHint").hidden = !s.wake_enabled;
      node("xiaozhiTalk").hidden = !!s.wake_enabled && !!s.wake_ready;
      if (document.activeElement !== node("xiaozhiTimeout")) node("xiaozhiTimeout").value = s.dialog_timeout || 30;
      if (document.activeElement !== node("xiaozhiEmoji")) node("xiaozhiEmoji").value = s.emoji_style === 1 ? 1 : 0;
      setText("xiaozhiRecognized", s.recognized);
      setText("xiaozhiText", s.reply ?? s.text);
      node("xiaozhiConversation").hidden = false;
      node("xiaozhiUserBubble").hidden = !s.enabled || !s.recognized;
      node("xiaozhiReplyBubble").hidden = !s.enabled || !(s.reply ?? s.text);
      node("xiaozhiSpectrum").hidden = !s.enabled || !s.supported;
      setText("xiaozhiConversationHint", conversationHint(s));
      voiceActive = !!(s.supported && s.enabled && (s.conversation || s.listening || s.speaking) &&
        ["listening", "speaking", "speaking_listening", "waiting"].includes(s.state));
      updateSpectrum(s, requestedAt);
      const emotions = {neutral:"😐",happy:"🙂",laughing:"😆",funny:"😂",sad:"😔",angry:"😠",crying:"😭",loving:"😍",embarrassed:"😳",surprised:"😮",shocked:"😱",thinking:"🤔",winking:"😉",cool:"😎",relaxed:"😌",delicious:"😋",kissy:"😘",confident:"😏",sleepy:"😴",silly:"😜",confused:"😕"};
      setText("xiaozhiEmotion", emotions[s.emotion] || emotions.neutral);
      const otto = s.supported && s.enabled && s.emoji_style !== 1;
      node("xiaozhiEmotion").hidden = otto;
      node("xiaozhiOtto").hidden = !otto;
      const emotion = Object.hasOwn(emotions, s.emotion) ? s.emotion : "neutral";
      const src = `/api/xiaozhi/emoji?emotion=${emotion}`;
      if (otto && node("xiaozhiOtto").getAttribute("src") !== src) node("xiaozhiOtto").setAttribute("src", src);
      if (!otto) node("xiaozhiOtto").removeAttribute("src");
      const key = `xiaozhi.state.${s.state}`;
      setText("xiaozhiStatus", t(key) === key ? s.state : t(key));
    } catch (_) {
      voiceActive = false; clearSpectrum();
      if (started === revision && !busy && pageVisible()) {
        node("xiaozhiConversation").hidden = false;
        node("xiaozhiStatus").textContent = t("xiaozhi.state.connection_error");
        node("xiaozhiConversationHint").textContent = "";
      }
    } finally { clearTimeout(deadline); refreshing = false; }
  }
  function command(action, value = 0) {
    revision++;
    busy++;
    clearSpectrum();
    if (["disable", "disconnect", "reset"].includes(action)) voiceActive = false;
    // Keep rapid edits and start/release in order; never silently discard an edit.
    pending = pending.then(async () => {
      const controller = new AbortController();
      const deadline = setTimeout(() => controller.abort(), 10000);
      try {
        const response = await fetch("/api/xiaozhi", {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({action, value}), signal: controller.signal});
        if (!response.ok) throw new Error(t("xiaozhi.failed"));
        return true;
      } catch (_) { node("xiaozhiStatus").textContent = t("xiaozhi.failed"); return false; }
      finally { clearTimeout(deadline); busy--; }
    });
    return pending;
  }
  function stop() { if (talking) { talking = false; return command("stop"); } }
  function start(event) {
    if (event.type === "keydown" && ![" ", "Enter"].includes(event.key)) return;
    if (talking || node("xiaozhiTalk").disabled || node("xiaozhiTalk").hidden) return;
    event.preventDefault(); talking = true; return command("start");
  }
  node("xiaozhiWakeWord").addEventListener("change", () => { wakeDirty = true; revision++; wakeFields(); });
  node("xiaozhiWakePhrase").addEventListener("input", () => { wakeDirty = true; revision++; });
  node("xiaozhiWakeSave").addEventListener("click", async () => {
    let phrase = normalizePhrase(node("xiaozhiWakePhrase").value);
    const word = Number(node("xiaozhiWakeWord").value);
    if (!Number.isInteger(word) || word < 0 || word > 3 || (word === 3 && !validPhrase(phrase))) {
      node("xiaozhiWakeResult").textContent = t("xiaozhi.phrase_help"); return;
    }
    if (word !== 3 && !validPhrase(phrase)) phrase = savedWakePhrase;
    wakePendingAt = Date.now();
    pendingRevision = wakeRevision;
    wakePending = {word, phrase}; wakeDirty = true;
    node("xiaozhiWakeWord").disabled = node("xiaozhiWakePhrase").disabled = true;
    node("xiaozhiWakeSave").disabled = true;
    node("xiaozhiWakeResult").textContent = t("xiaozhi.wake_saving");
    if (!await command("wake_config", wakePending)) {
      wakePending = null; node("xiaozhiWakeSave").disabled = false;
      node("xiaozhiWakeWord").disabled = node("xiaozhiWakePhrase").disabled = false;
      node("xiaozhiWakeResult").textContent = t("xiaozhi.state.wake_config_error");
    }
  });
  async function modelRequest(options = {}) {
    const controller = new AbortController();
    const deadline = setTimeout(() => controller.abort(), 20000);
    try {
      const response = await fetch("/api/xiaozhi/wake-model", {...options, signal: controller.signal, cache: "no-store"});
      if (!response.ok) throw new Error("upload");
      return await response.json();
    } finally { clearTimeout(deadline); }
  }
  node("xiaozhiModelFile").addEventListener("change", async (event) => {
    const file = event.target.files?.[0]; if (!file) return;
    busy++; revision++; event.target.disabled = true;
    try {
      const meta = await modelRequest();
      if (file.size !== meta.bytes) throw new Error("size");
      if (!meta.ready) for (let offset = 0; offset < file.size; offset += 32768) {
        const state = await modelRequest({method:"PUT", headers:{"Content-Type":"application/octet-stream", "X-Model-Offset":String(offset)}, body:file.slice(offset, offset + 32768)});
        if (offset + 32768 >= file.size && !state.ready) throw new Error("checksum");
        node("xiaozhiWakeResult").textContent = `${Math.min(100, Math.round((offset + 32768) * 100 / file.size))}%`;
        await new Promise(resolve => setTimeout(resolve, 150));
      }
      node("xiaozhiWakeResult").textContent = t("xiaozhi.model_ready");
    } catch (_) { node("xiaozhiWakeResult").textContent = t("xiaozhi.model_failed"); }
    finally { busy--; event.target.disabled = false; event.target.value = ""; }
  });
  node("xiaozhiEnabled").addEventListener("change", (e) => command(e.target.checked ? "enable" : "disable"));
  node("xiaozhiWake").addEventListener("change", (e) => command("wake", e.target.checked ? 1 : 0));
  node("xiaozhiDuplex").addEventListener("change", (e) => command("duplex", e.target.checked ? 1 : 0));
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
  node("xiaozhiReset").addEventListener("click", async () => { if (await command("reset")) { wakeDirty = false; wakePending = null; node("xiaozhiWakeResult").textContent = ""; } });
  node("xiaozhiStop").addEventListener("click", () => { talking = false; command("disconnect"); });
  node("xiaozhiVolume").addEventListener("change", (e) => command("volume", Number(e.target.value)));
  node("xiaozhiTalk").addEventListener("pointerdown", start);
  node("xiaozhiTalk").addEventListener("keydown", start);
  node("xiaozhiTalk").addEventListener("keyup", stop);
  window.addEventListener("pointerup", stop);
  window.addEventListener("pointercancel", stop);
  window.addEventListener("blur", stop);
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) { voiceActive = false; clearSpectrum(); stop(); node("xiaozhiOtto").removeAttribute("src"); }
  });
  // Idle settings poll slowly. Active conversations share the same bounded,
  // single-flight status request; no additional audio or WebSocket connection.
  setInterval(() => { if (!voiceActive) return refresh(); }, 1000);
  setInterval(() => { if (voiceActive) return refresh(); }, 200);
  setInterval(() => {
    if (spectrumVisible && (!pageVisible() || spectrumNow() - spectrumAt >= 350)) clearSpectrum();
  }, 100);
})();
