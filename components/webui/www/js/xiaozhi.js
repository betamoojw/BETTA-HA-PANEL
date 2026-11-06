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
  let statusReady = false;
  let dialogSeq = null, dialogEpoch = null, endedSeq = null, dialogActive = false, voiceOpened = false, capture = null;
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
  const manualMode = (s) => !s.wake_enabled || !s.wake_ready ||
    (s.dialog_active && s.dialog_voice_opened === false);
  function conversationHint(s) {
    if (!s.enabled) return "";
    if (s.state === "connection_error" || s.state === "audio_or_connection_error")
      return t(s.wake_enabled && s.wake_ready ? "xiaozhi.hint_error" : "xiaozhi.hint_manual_error");
    if (s.dialog_active && Number.isInteger(s.silence_remaining_ms) &&
        s.silence_remaining_ms > 0 && s.silence_remaining_ms <= 5000)
      return t(manualMode(s) ? "xiaozhi.hint_timeout_manual" : "xiaozhi.hint_timeout")
        .replace("%u", String(Math.ceil(s.silence_remaining_ms / 1000)));
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
  function acceptDialog(s) {
    // Older application images retain their existing controls after rollback.
    if (s.dialog_seq === undefined || s.dialog_epoch === undefined) {
      dialogSeq = dialogEpoch = endedSeq = null; return true;
    }
    if (!Number.isInteger(s.dialog_seq) || s.dialog_seq < 0 || s.dialog_seq > 0xffffffff) return false;
    if (!Number.isInteger(s.dialog_epoch) || s.dialog_epoch <= 0 || s.dialog_epoch > 0xffffffff) return false;
    if (s.dialog_epoch !== dialogEpoch) {
      // A restarted device owns a fresh generation space. Polls are single-flight
      // and revision-guarded, so an earlier boot cannot arrive after this one.
      dialogEpoch = s.dialog_epoch; dialogSeq = endedSeq = null;
      talking = false; capture = null;
    }
    const seq = s.dialog_seq;
    if (dialogSeq !== null && seq !== dialogSeq) {
      // Unsigned generations remain ordered across rollover. A completed old
      // request must never revive a closed or subsequently reopened dialogue.
      const distance = (seq - dialogSeq) >>> 0;
      if (distance >= 0x80000000) return false;
    }
    if (seq !== dialogSeq) { dialogSeq = seq; endedSeq = null; }
    if (!s.dialog_active && s.dialog_end_seq === seq) endedSeq = seq;
    return !(s.dialog_active && endedSeq === seq);
  }
  function reconcileCapture(s) {
    if (!talking || !capture || capture.legacy) return;
    if (s.dialog_epoch !== capture.epoch || (s.dialog_active && s.dialog_voice_opened) ||
        (!capture.pending && (s.dialog_seq !== capture.seq || !s.dialog_active))) {
      talking = false; capture = null; return;
    }
    if (!capture.pending) return;
    if (s.dialog_seq === capture.seq) {
      if (!s.dialog_active) { talking = false; capture = null; return; }
      capture.pending = false;
      command("start_turn", capture.seq, capture.epoch);
    } else if (s.dialog_seq !== capture.before) { talking = false; capture = null; }
  }
  function hideConversation() {
    voiceActive = false; clearSpectrum();
    node("xiaozhiConversation").hidden = true;
    node("xiaozhiStatus").hidden = false;
    node("xiaozhiOtto").removeAttribute("src");
    setText("xiaozhiRecognized", ""); setText("xiaozhiText", "");
    setText("xiaozhiConversationHint", "");
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
      if (!acceptDialog(s)) return;
      statusReady = true;
      if (typeof s.dialog_active !== "boolean") s.dialog_active = !!(s.conversation || s.listening || s.speaking);
      dialogActive = s.dialog_active; voiceOpened = !!s.dialog_voice_opened;
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
      node("xiaozhiTalk").hidden = !manualMode(s);
      if (document.activeElement !== node("xiaozhiEmoji")) node("xiaozhiEmoji").value = s.emoji_style === 1 ? 1 : 0;
      const failed = s.state === "connection_error" || s.state === "audio_or_connection_error";
      const visible = !!(s.enabled && s.supported && (s.dialog_active || failed));
      if (!visible) hideConversation();
      node("xiaozhiConversation").hidden = !visible;
      node("xiaozhiStatus").hidden = visible;
      setText("xiaozhiRecognized", s.dialog_active ? s.recognized : "");
      setText("xiaozhiText", s.dialog_active ? (s.reply ?? s.text) : "");
      node("xiaozhiUserBubble").hidden = !visible || !s.dialog_active || !s.recognized;
      node("xiaozhiReplyBubble").hidden = !visible || !s.dialog_active || !(s.reply ?? s.text);
      node("xiaozhiSpectrum").hidden = !visible || !s.dialog_active;
      setText("xiaozhiConversationHint", visible ? conversationHint(s) : "");
      voiceActive = !!(visible && s.dialog_active &&
        ["listening", "speaking", "speaking_listening", "waiting"].includes(s.state));
      updateSpectrum(s, requestedAt);
      const emotions = {neutral:"😐",happy:"🙂",laughing:"😆",funny:"😂",sad:"😔",angry:"😠",crying:"😭",loving:"😍",embarrassed:"😳",surprised:"😮",shocked:"😱",thinking:"🤔",winking:"😉",cool:"😎",relaxed:"😌",delicious:"😋",kissy:"😘",confident:"😏",sleepy:"😴",silly:"😜",confused:"😕"};
      setText("xiaozhiEmotion", emotions[s.emotion] || emotions.neutral);
      const otto = visible && s.emoji_style !== 1;
      node("xiaozhiEmotion").hidden = otto;
      node("xiaozhiOtto").hidden = !otto;
      const emotion = Object.hasOwn(emotions, s.emotion) ? s.emotion : "neutral";
      const src = `/api/xiaozhi/emoji?emotion=${emotion}`;
      if (otto && node("xiaozhiOtto").getAttribute("src") !== src) node("xiaozhiOtto").setAttribute("src", src);
      if (!otto) node("xiaozhiOtto").removeAttribute("src");
      const key = `xiaozhi.state.${s.state}`;
      const stateText = t(key) === key ? s.state : t(key);
      setText("xiaozhiStatus", stateText); setText("xiaozhiConversationState", stateText);
      reconcileCapture(s);
    } catch (_) {
      voiceActive = false; clearSpectrum();
      if (started === revision && !busy && pageVisible()) {
        hideConversation();
        node("xiaozhiStatus").textContent = t("xiaozhi.state.connection_error");
      }
    } finally { clearTimeout(deadline); refreshing = false; }
  }
  function command(action, value = 0, epoch = null) {
    revision++;
    busy++;
    clearSpectrum();
    const closing = ["disable", "disconnect", "dismiss", "reset"].includes(action);
    const closingSeq = dialogSeq, closingEpoch = dialogEpoch;
    if (closing) {
      talking = false; capture = null;
      dialogActive = false;
      hideConversation();
    }
    // Keep rapid edits and start/release in order; never silently discard an edit.
    pending = pending.then(async () => {
      const controller = new AbortController();
      const deadline = setTimeout(() => controller.abort(), 10000);
      try {
        const body = epoch === null ? {action, value} : {action, value, epoch};
        const response = await fetch("/api/xiaozhi", {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body), signal: controller.signal});
        if (!response.ok) throw new Error(t("xiaozhi.failed"));
        if (closing && dialogSeq === closingSeq && dialogEpoch === closingEpoch) endedSeq = closingSeq;
        return true;
      } catch (_) { node("xiaozhiStatus").textContent = t("xiaozhi.failed"); return false; }
      finally { clearTimeout(deadline); busy--; }
    });
    return pending;
  }
  function stop() {
    if (!talking) return;
    talking = false;
    const held = capture; capture = null;
    if (held?.legacy) return command("stop");
    if (held && !held.pending) return command("stop_turn", held.seq, held.epoch);
  }
  async function start(event) {
    if (event.type === "keydown" && ![" ", "Enter"].includes(event.key)) return;
    if (!statusReady || talking || node("xiaozhiTalk").disabled || node("xiaozhiTalk").hidden) return;
    event.preventDefault();
    if (dialogSeq !== null && dialogActive && voiceOpened) return;
    talking = true;
    if (dialogSeq === null) { capture = {legacy:true}; return command("start"); }
    if (dialogActive) {
      capture = {seq:dialogSeq, epoch:dialogEpoch};
      return command("start_turn", dialogSeq, dialogEpoch);
    }
    // Opening a manual dialogue is asynchronous. Do not start recording until
    // its own generation is confirmed, and do nothing if released while waiting.
    const held = {seq:(dialogSeq + 1) >>> 0, before:dialogSeq, epoch:dialogEpoch, pending:true};
    capture = held;
    if (!await command("dialog_turn", dialogSeq, dialogEpoch)) {
      if (capture === held) { talking = false; capture = null; }
      return;
    }
    if (capture === held && talking) await refresh();
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
  node("xiaozhiPair").addEventListener("click", () => command("pair"));
  node("xiaozhiReset").addEventListener("click", async () => { if (await command("reset")) { wakeDirty = false; wakePending = null; node("xiaozhiWakeResult").textContent = ""; } });
  node("xiaozhiStop").addEventListener("click", () => {
    if (!statusReady) return;
    talking = false;
    return command(dialogSeq === null ? "disconnect" : "dismiss", dialogSeq ?? 0, dialogEpoch);
  });
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
