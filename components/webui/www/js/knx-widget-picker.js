/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
const knxControlWidgetTypes = ["cover_tile", "climate_tile", "fan_tile"];
const knxWidgetTypes = ["sensor", "gauge", "graph", "binary_tile", "contact_tile", "button", "slider", "light_tile", "scene_tile", "select_tile", ...knxControlWidgetTypes];
const knxTileRoles = {
  sensor: ["value_feedback"], gauge: ["number_feedback"], graph: ["number_feedback"],
  binary_tile: ["switch_feedback"], contact_tile: ["switch_feedback"],
  button: ["switch_command", "switch_feedback"], slider: ["brightness_command", "brightness_feedback", "angle_command", "angle_feedback"],
  select_tile: ["preset_command", "preset_feedback"],
  light_tile: ["switch_command", "switch_feedback", "brightness_command", "brightness_feedback"], scene_tile: ["action"],
  cover_tile: ["move_command", "stop_command", "position_command", "position_feedback"],
  climate_tile: ["target_temperature_command", "target_temperature_feedback", "current_temperature_feedback", "hvac_mode_command", "hvac_mode_feedback"],
  fan_tile: ["switch_command", "switch_feedback", "fan_speed_command", "fan_speed_feedback"],
};
const knxField = id => document.getElementById(id);
let knxPickerRequest = null;
let knxPickerTarget = null;
let knxPickerObjects = [];
let knxPreview = new Map();
let knxRegistry = new Map();
let knxPreviewRequest = 0;
let knxLatestSnapshot;
let knxPreviewLoading = false;
let knxRoutingActive = false;

function knxPreviewKey(type, id) { return `knx:${type}:${id}`; }

function rememberKnxPreview(objects, routing) {
  const views = new Map();
  knxRoutingActive = routing === "running";
  // Pages arrive in order. A later alias may already include a write or report
  // that arrived after the earlier page. Keep the two sources independent.
  const reads = new Map(), writes = new Map();
  for (const o of objects) {
    if (!o.group_address) continue;
    if (o.readable) reads.set(o.group_address, o);
    if (o.writable) writes.set(o.group_address, o);
  }
  knxRegistry = new Map();
  for (const entry of objects) {
    const read = (entry.readable && reads.get(entry.group_address)) || entry;
    const write = (entry.writable && writes.get(entry.group_address)) || entry;
    const o = read === entry && write === entry ? entry : {...entry, valid: read.valid, live_feedback: read.live_feedback,
      value: read.value, last_update_ms: read.last_update_ms, last_command: write.last_command};
    knxRegistry.set(o.id, o);
    const types = new Set(o.widget_types || []);
    for (const [type, roles] of Object.entries(knxTileRoles))
      if (roles.some(role => o.widget_roles?.includes(role))) types.add(type);
    for (const type of types) {
      const live = routing === "running" && o.live_feedback && o.value != null;
      let state = "unavailable", percent = null;
      if (type === "scene_tile" && routing === "running" && !live) state = "ready";
      else if (live) {
        if (typeof o.value === "boolean") state = o.value ? "on" : "off";
        else state = typeof o.value === "object" ? JSON.stringify(o.value) : String(o.value);
        if (type === "light_tile" || type === "slider") {
          const level = typeof o.value === "boolean" ? (o.value ? 100 : 0)
            : typeof o.value === "number" ? o.value : Math.max(o.value.red, o.value.green, o.value.blue) * 100 / 255;
          percent = Math.round(level);
          if (type === "light_tile") state = level > 0 ? "on" : "off";
        }
      }
      const scaling = o.datapoint_type === "DPT-5.001";
      const unit = o.unit || (scaling ? "%" : "");
      views.set(knxPreviewKey(type, o.id), {state, percent, unit, name: o.name, live,
        writable: knxRoutingActive && o.widget_roles?.includes("switch_command"),
        rgb: o.datapoint_type?.startsWith("DPT-232"), dimmer: scaling || o.datapoint_type?.startsWith("DPT-232"),
        precision: o.display_precision, scalingSensor: scaling && type === "sensor"});
    }
  }
  knxPreview = views;
}

// Separate composite keys keep lights sharing a switch but using different dimmers independent.
function knxRoleObject(roles, role) {
  const object = knxRegistry.get(roles[role]);
  return object?.widget_roles?.includes(role) ? object : null;
}

function knxRoleValue(roles, feedback) {
  if (!knxRoutingActive) return null;
  if (roles[feedback]) {
    const object = knxRoleObject(roles, feedback);
    return object?.live_feedback ? object.value ?? null : null;
  }
  const command = feedback.replace(/_feedback$/, "_command");
  return knxRoleObject(roles, command)?.last_command ?? null;
}

const knxPresetNames = ["Auto", "Comfort", "Standby", "Economy", "Building Protection"];

function knxWidgetPreviewKey(widget) {
  const {type, binding} = widget, roles = binding.roles;
  if (knxControlWidgetTypes.includes(type)) return knxControlPreviewKey(widget);
  if (!roles) {
    if (type === "light_tile" && !knxRegistry.get(binding.object_id)?.widget_types?.includes(type)) return "knx:missing";
    return knxPreviewKey(type, binding.object_id);
  }
  const feedbackRole = knxTileRoles[type]?.find(r => r.endsWith("_feedback")) || "action";
  if (!["button", "slider", "select_tile", "light_tile"].includes(type)) {
    const id = roles[feedbackRole];
    const valid = knxRegistry.get(id)?.widget_roles?.includes(feedbackRole);
    return valid ? knxPreviewKey(type, id) : "knx:missing";
  }
  const key = `knx:roles:${type}:${JSON.stringify(knxTileRoles[type].map(r => roles[r] || null))}`;
  const angle = type === "slider" && !!(roles.angle_command || roles.angle_feedback);
  const power = knxRoleValue(roles, "switch_feedback");
  const raw = knxRoleValue(roles, angle ? "angle_feedback" : "brightness_feedback");
  const level = typeof raw === "number" && Number.isFinite(raw) ? Math.round(raw) : null;
  const unknownState = (command, feedback) => knxRoutingActive && knxRoleObject(roles, command) && !roles[feedback]
    ? "unknown" : "unavailable";
  const view = {state: typeof power === "boolean" ? (power ? "on" : "off") : unknownState("switch_command", "switch_feedback"),
    live: typeof power === "boolean" || level !== null, percent: level,
    dimmer: !!(roles.brightness_command || roles.brightness_feedback)};
  if (type === "button") {
    view.writable = knxRoutingActive && !!knxRoleObject(roles, "switch_command");
  } else if (type === "slider") {
    view.state = level === null ? (angle ? unknownState("angle_command", "angle_feedback")
      : unknownState("brightness_command", "brightness_feedback")) : String(level);
    if (angle) { view.unit = "°"; view.percent = level === null ? null : level * 100 / 360; }
  } else if (type === "select_tile") {
    const code = knxRoleValue(roles, "preset_feedback");
    view.live = Number.isInteger(code) && !!knxPresetNames[code];
    view.state = view.live ? knxPresetNames[code] : knxRoutingActive ? "unknown" : "unavailable";
    view.writable = knxRoutingActive && !!knxRoleObject(roles, "preset_command");
  }
  knxPreview.set(key, view);
  return key;
}

function previewSourceState(key) {
  return key?.startsWith("knx:") ? knxPreview.get(key)?.state ?? "unavailable" : editor.states.get(key);
}

function publishKnxPreview(request, page) {
  if (!knxLatestSnapshot || request > knxLatestSnapshot.request) {
    rememberKnxPreview(page.objects, page.state);
    knxLatestSnapshot = {request, ...page};
  }
  return knxLatestSnapshot;
}

async function refreshKnxWidgetPreview() {
  if (knxPreviewLoading || !editor.layout?.pages?.some(p => p.widgets?.some(w => w.binding?.source === "knx"))) return;
  knxPreviewLoading = true;
  const seq = ++knxPreviewRequest;
  try {
    const page = await fetchKnxWidgetObjects();
    publishKnxPreview(seq, page);
  } catch (_) {
    publishKnxPreview(seq, {objects: [], state: "disabled"});
  } finally { knxPreviewLoading = false; }
  if (seq === knxLatestSnapshot.request) renderCanvas();
}

function cancelKnxWidgetPicker() {
  knxPickerRequest?.abort();
  knxPickerRequest = null;
  knxPickerTarget = null;
  knxPickerObjects = [];
}

function showWidgetSource(source) {
  const select = knxField("addTileSource");
  if (!select) return;
  select.value = source;
  select.disabled = !!knxPickerTarget?.widget;
  knxField("addTileHaPane")?.classList.toggle("hidden", source !== "ha");
  knxField("addTileKnxPane")?.classList.toggle("hidden", source !== "knx");
}

function replaceKnxOptions(select, entries) {
  const options = entries.map(([value, label]) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    return option;
  });
  select.replaceChildren(...options);
}

// The device advertises the roles actually accepted by the command and feedback paths.
async function fetchKnxWidgetObjects(request = new AbortController()) {
  const objects = [];
  let offset = 0, first, restarted = false;
  for (;;) {
    // A full registry needs 26 HTTP requests. Bound each request while it makes progress.
    const timer = setTimeout(() => request.abort(), 10000);
    let page;
    try { page = await apiGet(`/api/knx/objects?offset=${offset}`, request.signal); }
    finally { clearTimeout(timer); }
    if (!Array.isArray(page.objects)) throw new Error(t("knx.failed"));
    if (!offset) first = page;
    else if (page.generation !== first.generation || page.routing_epoch !== first.routing_epoch || page.state !== first.state) {
      if (restarted) throw new Error(t("knx.failed"));
      restarted = true;
      objects.length = 0;
      offset = 0;
      continue;
    }
    const next = offset + page.objects.length;
    if (typeof page.has_more !== "boolean" || page.next_offset !== next || next > KNX_MAX_OBJECTS ||
        (page.has_more && (next === offset || next === KNX_MAX_OBJECTS))) throw new Error(t("knx.failed"));
    objects.push(...page.objects);
    if (!page.has_more) return {objects, state: page.state};
    offset = next;
  }
}

function knxLegacyRoles(widget, objects) {
  const {type, binding} = widget;
  if (binding.roles) return Object.fromEntries(Object.entries(binding.roles).filter(([, id]) => id != null && id !== ""));
  if (knxControlWidgetTypes.includes(type)) return {}; // These widgets require purpose-specific roles.
  let roles = knxTileRoles[type];
  if (type === "light_tile") {
    const dpt = objects.find(o => o.id === binding.object_id)?.datapoint_type;
    if (dpt?.startsWith("DPT-1.") || dpt === "DPT-1") roles = knxTileRoles.button;
    else if (dpt === "DPT-5.001") roles = ["brightness_command", "brightness_feedback"];
    else return {}; // Keep an existing RGB or missing light intact until explicitly rebound.
  }
  if (type === "slider") roles = ["brightness_command", "brightness_feedback"];
  return Object.fromEntries(roles.map(role => [role, binding.object_id]));
}

function knxObjectLabel(o) {
  const access = [o.readable && "R", o.writable && "W"].filter(Boolean).join("/");
  return [o.name, o.id, o.group_address, o.datapoint_type, access].join(" · ");
}

function knxObjectField(id, caption, candidates, current) {
  const label = document.createElement("label"), title = document.createElement("span");
  title.textContent = caption;
  const select = document.createElement("select");
  select.id = id;
  const entries = [["", t("knx.unbound")], ...candidates.map(o => [o.id, knxObjectLabel(o)])];
  const unavailable = current && !candidates.some(o => o.id === current);
  if (unavailable) entries.push([current, current + " · " + t("knx.unknown")]);
  replaceKnxOptions(select, entries);
  if (unavailable) select.options[select.options.length - 1].disabled = true;
  select.value = current;
  label.append(title, select);
  return {label, select, unavailable};
}

function knxRoleHint(role) {
  const read = role.endsWith("_feedback");
  const dpt = role === "move_command" ? "DPT 1.008 / 1.009" : role === "stop_command" ? "DPT 1.010 / 3.008"
    : role.startsWith("angle_") ? "DPT 5.003" : role.startsWith("preset_") ? "DPT 20.102"
    : role.includes("temperature_") ? "DPT 9.001" : role.startsWith("hvac_mode_") ? "DPT 20.105"
    : /^(brightness|position|fan_speed)_/.test(role) ? "DPT 5.001" : role.startsWith("switch_") ? "DPT 1"
    : t(role === "number_feedback" ? "knx.numeric_dpt" : "knx.any_dpt");
  return t(read ? "knx.need_read" : "knx.need_write", {dpt});
}

function knxCoverPositionField(target) {
  const label = document.createElement("label"), row = document.createElement("span");
  row.className = "row";
  const input = document.createElement("input"), title = document.createElement("span");
  input.type = "checkbox";
  input.id = "knxPositionOpenAt100";
  input.checked = target.positionOpenAt100 === true;
  title.textContent = t("knx.position_open_at_100");
  input.onchange = () => { target.positionOpenAt100 = input.checked; target.edited = true; };
  row.append(input, title);
  const hint = document.createElement("span");
  hint.id = "knxPositionOpenAt100Hint";
  hint.className = "meta no-shorten";
  hint.textContent = t("knx.position_direction_hint");
  input.setAttribute("aria-describedby", hint.id);
  label.append(row, hint);
  return label;
}

function updateKnxApply() {
  const target = knxPickerTarget;
  if (!target) return;
  const {type, roles} = target, action = type === "scene_tile";
  const o = knxPickerObjects.find(entry => entry.id === roles.action);
  knxField("knxTileValueWrap").classList.toggle("hidden", !action);
  knxField("knxTileFormat").textContent = action ? o?.value_format || "" : "";
  const input = knxField("knxTileValue");
  const saved = target.widget?.binding;
  const validValue = !action || layoutJsonBytes(input.value) <= 255 &&
    (input.value.length > 0 || ["DPT-16", "DPT-24", "DPT-28"].includes(o?.datapoint_type?.split(".")[0]) ||
     (!o && saved?.value === "" && roles.action === (saved.object_id || saved.roles?.action)));
  knxField("knxTileApply").disabled = !type || (!target.objectId && !Object.values(roles).some(Boolean)) || !validValue;
}

function knxLegacyRgbField(target) {
  if (target.type !== "light_tile" || !target.widget?.binding?.object_id) return null;
  const candidates = knxPickerObjects.filter(o =>
    o.datapoint_type?.split(".")[0] === "DPT-232" && o.widget_types?.includes("light_tile"));
  if (!candidates.length) return null;
  const current = Object.keys(target.roles).length ? "" : target.objectId;
  const field = knxObjectField("knxTileRgbObject", t("knx.object") + " · RGB", candidates, current);
  field.select.onchange = () => {
    const id = field.select.value;
    if (id === current || (id && !candidates.some(o => o.id === id))) return;
    target.objectId = id;
    target.roles = {};
    target.edited = true;
    renderKnxRoles();
    knxField("knxTileRgbObject").focus();
  };
  return field;
}

function renderKnxRoles() {
  const target = knxPickerTarget;
  if (!target) return;
  const rgb = knxLegacyRgbField(target);
  const rows = rgb ? [rgb.label] : [];
  const roles = rgb?.select.value ? [] : knxTileRoles[target.type] || [];
  for (const role of roles) {
    const candidates = knxPickerObjects.filter(o => o.widget_roles?.includes(role));
    const {label, select, unavailable} = knxObjectField("knxRole_" + role, t(`knx.role_${role}`),
      candidates, target.roles[role] || "");
    if (target.type === "slider") {
      const other = role.startsWith("angle_") ? "brightness" : "angle";
      select.disabled = !!(target.roles[other + "_command"] || target.roles[other + "_feedback"]);
    }
    const hint = document.createElement("span");
    hint.id = select.id + "_hint";
    hint.className = "meta no-shorten";
    select.setAttribute("aria-describedby", hint.id);
    const roleHint = () => [!candidates.length || unavailable ? knxRoleHint(role) : "",
      role === "stop_command" ? t("knx.stop_hint") :
      role.endsWith("_feedback") && roles.includes(role.replace("_feedback", "_command")) ? t("knx.optional_feedback") : ""].filter(Boolean).join(" ");
    hint.textContent = roleHint();
    select.onchange = () => {
      if (select.value === (target.roles[role] || "")) return;
      if (select.value && !candidates.some(o => o.id === select.value)) return;
      if (role === "action") knxField("knxTileValue").value = "";
      target.objectId = "";
      if (select.value) target.roles[role] = select.value;
      else delete target.roles[role];
      target.edited = true;
      hint.textContent = roleHint();
      if (target.type === "slider") renderKnxRoles();
      else updateKnxApply();
    };
    label.append(hint);
    rows.push(label);
  }
  if (target.type === "cover_tile") rows.push(knxCoverPositionField(target));
  knxField("knxTileRoles").replaceChildren(...rows);
  updateKnxApply();
}

async function loadKnxWidgetPicker() {
  knxPickerRequest?.abort();
  const request = new AbortController(), target = knxPickerTarget;
  const seq = ++knxPreviewRequest;
  knxPickerRequest = request;
  const status = knxField("knxTileStatus");
  status.textContent = t("entity_picker.pending");
  try {
    const page = await fetchKnxWidgetObjects(request);
    if (request !== knxPickerRequest) return;
    const {objects, state} = publishKnxPreview(seq, page);
    knxPickerObjects = objects;
    if (!target.edited && target.widget?.binding?.source === "knx")
      target.roles = knxLegacyRoles(target.widget, objects);
    renderKnxRoles();
    status.textContent = target.objectId && !objects.some(o => o.id === target.objectId)
      ? t("knx.single_binding", {id: target.objectId}) : t(`knx.state.${state}`);
  } catch (_) {
    if (request === knxPickerRequest) {
      knxPickerObjects = [];
      renderKnxRoles();
      status.textContent = t("knx.failed");
    }
  } finally {
    if (request === knxPickerRequest) knxPickerRequest = null;
  }
}

function startKnxWidgetPicker(widget = null) {
  closeAddTileDialog(); // Cancel HA discovery and ignore late results after a source switch.
  const type = widget?.type || "";
  knxPickerTarget = {page: selectedPage(), widget, type, edited: false, objectId: widget?.binding?.object_id || "",
    positionOpenAt100: widget?.binding?.position_open_at_100 === true,
    roles: widget?.binding?.source === "knx" ? knxLegacyRoles(widget, []) : {}};
  const select = knxField("knxTileType");
  const entries = widget ? [[type, widgetTypeLabel(type)]]
    : [["", t("knx.widget_type")], ...knxWidgetTypes.map(id => [id, widgetTypeLabel(id)])];
  replaceKnxOptions(select, entries);
  select.value = type;
  select.disabled = !!widget;
  knxField("knxTileValue").value = widget?.binding?.value ?? "";
  renderKnxRoles();
  showWidgetSource("knx");
  el.addTileOverlay.classList.remove("hidden");
  select.focus();
  return loadKnxWidgetPicker();
}

function useKnxWidgetObject() {
  const target = knxPickerTarget;
  if (!target || selectedPage() !== target.page || knxField("knxTileApply").disabled) return;
  const {type, roles} = target;
  const binding = target.objectId
    ? {...target.widget.binding, object_id: target.objectId} : {source: "knx", roles: {...roles}};
  if (type === "cover_tile") binding.position_open_at_100 = target.positionOpenAt100 === true;
  if (type === "scene_tile") binding.value = knxField("knxTileValue").value;
  const first = knxPickerObjects.find(o => o.id === Object.values(roles)[0]);
  if (target.widget) {
    if (selectedWidget() !== target.widget || type !== target.widget.type) return;
    layoutHistoryBoundary();
    target.widget.binding = binding;
    target.widget.entity_id = "";
    delete target.widget.secondary_entity_id;
    delete target.widget.tertiary_entity_id;
    delete target.widget.sensor_entities;
    delete target.widget.sensor_view;
    delete target.widget.graph_ha_history;
    delete target.widget.history_popup;
    if (type === "button") target.widget.button_mode = DEFAULT_BUTTON_MODE;
    renderAll();
  } else if (!addWidget(type, {entityId: "", title: first?.name || widgetTypeLabel(type), binding})) return;
  closeAddTileDialog();
}

function renderKnxInspector(widget) {
  knxField("knxInspectorBinding")?.classList.toggle("hidden", !knxWidgetTypes.includes(widget?.type));
  const binding = widget?.binding, current = knxField("knxInspectorCurrent");
  knxField("knxInspectorHa")?.classList.toggle("hidden", binding?.source !== "knx");
  if (current) current.textContent = binding?.source === "knx" ? binding.object_id || Object.entries(binding.roles || {}).filter(([, id]) => id)
    .map(([role, id]) => t(`knx.role_${role}`) + " · " + id).join(" / ") : "";
  for (const input of [el.fHistoryPopup, el.fGraphHaHistory, el.fSliderEntityDomain, el.fButtonMode, el.fSensorView])
    if (input) input.disabled = binding?.source === "knx";
}

function bindKnxWidgetPicker() {
  const source = knxField("addTileSource");
  if (!source) return;
  source.onchange = () => source.value === "knx" ? startKnxWidgetPicker() : openAddTileDialog();
  knxField("knxInspectorChoose").onclick = () => startKnxWidgetPicker(selectedWidget());
  knxField("knxInspectorHa").onclick = () => openInspectorEntityPicker("primary");
  knxField("knxTileType").onchange = () => {
    if (!knxPickerTarget || knxPickerTarget.widget) return;
    knxPickerTarget.type = knxField("knxTileType").value;
    knxPickerTarget.roles = {};
    knxPickerTarget.positionOpenAt100 = false;
    knxField("knxTileValue").value = "";
    renderKnxRoles();
  };
  knxField("knxTileValue").oninput = updateKnxApply;
  knxField("knxTileApply").onclick = useKnxWidgetObject;
  knxField("knxTileReload").onclick = loadKnxWidgetPicker;
}
