/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
 * Alarm dashboard editor. PINs are entered only on the panel, never in layout JSON. */
function alarmPanelValidateSlots(key, rows) {
  const domains = key === "alarm_zones" ? ["binary_sensor"] : ["switch", "input_boolean", "button", "input_button"];
  const max = key === "alarm_zones" ? 16 : 8;
  if (!Array.isArray(rows) || rows.length > max) return false;
  const seen = new Set();
  return rows.every(row => {
    if (!row || typeof row.entity_id !== "string" || !/^[a-z_]+\.[a-z0-9_]+$/.test(row.entity_id) ||
        !domains.includes(row.entity_id.split(".")[0]) || new TextEncoder().encode(row.entity_id).length >= 96 ||
        (row.name !== undefined && (typeof row.name !== "string" || new TextEncoder().encode(row.name).length >= 48)) || seen.has(row.entity_id)) return false;
    seen.add(row.entity_id); return true;
  });
}

function alarmPanelReadInspector() {
  const section = document.getElementById("alarmPanelOptions");
  if (!section || section.hidden) return null;
  const value = { alarm_log_hours: Number(section.querySelector("[data-alarm-hours]")?.value || 24) };
  for (const key of ["alarm_zones", "alarm_actions"]) {
    value[key] = [...section.querySelectorAll(`[data-alarm-list="${key}"] .alarm-binding-row`)].map(row => ({
      entity_id: row.querySelector("[data-alarm-entity]").value.trim(),
      name: row.querySelector("[data-alarm-name]").value.trim(),
    })).filter(row => row.entity_id);
    if (!alarmPanelValidateSlots(key, value[key])) throw new Error(t("alarm.panel.invalid"));
  }
  if (!Number.isInteger(value.alarm_log_hours) || value.alarm_log_hours < 1 || value.alarm_log_hours > 168)
    throw new Error(t("alarm.panel.invalid"));
  return value;
}

function alarmPanelSaveInspector(widget, type, value) {
  if (type !== "alarm_panel") { delete widget.alarm_zones; delete widget.alarm_log_hours; return; }
  if (value) Object.assign(widget, value);
  delete widget.code; delete widget.pin; delete widget.alarm_code;
}

function alarmPanelLoadInspector(widget) {
  const section = document.getElementById("alarmPanelOptions");
  if (!section) return;
  section.hidden = widget?.type !== "alarm_panel";
  section.replaceChildren();
  if (section.hidden) return;
  const heading = document.createElement("h4"); heading.textContent = t("layout.widgets.add_alarm_panel"); section.append(heading);
  const hint = document.createElement("p"); hint.className = "hint"; hint.textContent = t("alarm.panel.hint"); section.append(hint);
  const hours = document.createElement("label"); hours.textContent = t("alarm.panel.hours");
  const input = document.createElement("input"); input.type = "number"; input.min = "1"; input.max = "168";
  input.value = String(widget.alarm_log_hours || 24); input.dataset.alarmHours = "";
  input.onchange = () => autoApplyInspector({softEntityValidation:true}); hours.append(input); section.append(hours);
  for (const key of ["alarm_zones", "alarm_actions"]) {
    const title = document.createElement("h4"); title.textContent = t(key === "alarm_zones" ? "alarm.zones" : "alarm.controls"); section.append(title);
    const list = document.createElement("div"); list.dataset.alarmList = key; section.append(list);
    const domains = key === "alarm_zones" ? ["binary_sensor"] : ["switch", "input_boolean", "button", "input_button"];
    const suggestions = document.createElement("datalist"); suggestions.id = `alarm-${key}-choices`;
    for (const entity of editor.entities || []) {
      if (!domains.includes(String(entity.id).split(".")[0])) continue;
      const option = document.createElement("option"); option.value = entity.id; option.label = entity.name || entity.id; suggestions.append(option);
    }
    section.append(suggestions);
    const add = document.createElement("button"); add.type = "button"; add.textContent = t("alarm.panel.add");
    const appendRow = (entry = {}) => {
      const row = document.createElement("div"); row.className = "alarm-binding-row";
      const id = document.createElement("input"); id.dataset.alarmEntity = ""; id.value = entry.entity_id || "";
      id.setAttribute("list", suggestions.id); id.placeholder = domains.join(" / "); id.setAttribute("aria-label", t("alarm.panel.entity"));
      const name = document.createElement("input"); name.dataset.alarmName = ""; name.value = entry.name || "";
      name.placeholder = t("alarm.panel.name"); name.setAttribute("aria-label", name.placeholder);
      const remove = document.createElement("button"); remove.type = "button"; remove.textContent = "×";
      remove.setAttribute("aria-label", t("entity_picker.remove"));
      remove.onclick = () => { row.remove(); add.disabled = false; autoApplyInspector({softEntityValidation:true}); };
      id.onchange = name.onchange = () => autoApplyInspector({softEntityValidation:true});
      row.append(id, name, remove); list.append(row);
      add.disabled = list.children.length >= (key === "alarm_zones" ? 16 : 8);
    };
    for (const row of widget[key] || []) appendRow(row);
    add.onclick = () => appendRow(); section.append(add);
  }
}

function alarmPanelPreview(widget, name) {
  const state = previewEntityState(widget.entity_id) || "unknown";
  const stateText = t(`preview.alarm.state.${state}`, {}, state.replace(/_/g, " "));
  const zoneRows = (widget.alarm_zones || []).map(row => `<div class="alarm-preview-row"><span>${escapeHtml(row.name || row.entity_id)}</span><b>${escapeHtml(previewEntityState(row.entity_id) || "—")}</b></div>`).join("");
  const controls = (widget.alarm_actions || []).map(row => {
    const hold = /^(input_)?button\./.test(row.entity_id), value = previewEntityState(row.entity_id);
    const available = hold ? previewControlAvailable(row.entity_id, true) : ["on", "off"].includes(value);
    const suffix = hold ? `<small>${escapeHtml(t("alarm.hold"))}</small>` : `: ${escapeHtml(available ? t(`preview.binary_state.${value}`) : t("evcc.unavailable"))}`;
    return `<div class="alarm-preview-control ${previewControlClass(value === "on", available)}">${escapeHtml(row.name || row.entity_id)}${suffix}</div>`;
  }).join("");
  const canDisarm = ["armed_home", "armed_away", "armed_night", "armed_vacation", "armed_custom_bypass", "arming", "pending", "triggered"].includes(state);
  const modes = [["away", 2], ["home", 1], ["night", 4], ["vacation", 32], ["custom_bypass", 16]].map(([key, bit]) =>
    `<span class="${previewControlClass(false, state === "disarmed" && previewSupports(widget.entity_id, bit, false))}">${escapeHtml(t(key === "custom_bypass" ? "layout.inspector.alarm_action_arm_custom_bypass" : `alarm.btn.${key}`))}</span>`).join("");
  const codeKey = editor.stateMeta.get(widget.entity_id)?.code_format ? "alarm.enter_code" : "alarm.no_code";
  const narrow = widget.rect.w < 900 ? " alarm-preview-narrow" : "";
  return `<div class="tp alarm-preview${narrow}"><section>${name("div")}<b>${escapeHtml(stateText)}</b><small>${escapeHtml(t(codeKey))}</small><div class="alarm-preview-keys">${[1,2,3,4,5,6,7,8,9,"C",0,"<"].map(key => `<span>${escapeHtml(String(key))}</span>`).join("")}</div><div class="alarm-preview-control ${previewControlClass(false, canDisarm)}">${escapeHtml(t("alarm.btn.disarm"))}</div><div class="alarm-preview-modes">${modes}</div></section><section><b>${escapeHtml(t("alarm.zones"))}</b>${zoneRows}<b>${escapeHtml(t("alarm.controls"))}</b>${controls}</section><section><b>${escapeHtml(t("alarm.history"))} (${widget.alarm_log_hours || 24}h)</b><small>${escapeHtml(t("alarm.panel.history_preview"))}</small></section></div><div class="resize-handle"></div>`;
}
