/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
const knxWidgetTypes = ["sensor", "gauge", "graph", "binary_tile", "contact_tile", "button", "slider", "light_tile", "scene_tile"];
const knxField = id => document.getElementById(id);
let knxPickerRequest = null;
let knxPickerTarget = null;
let knxPickerObjects = [];
let knxPreview = new Map();
let knxPreviewRequest = 0;
let knxPreviewLoading = false;

function knxPreviewKey(type, id) { return `knx:${type}:${id}`; }

function rememberKnxPreview(objects, routing) {
  const views = new Map();
  for (const o of objects) {
    for (const type of o.widget_types || []) {
      const live = routing === "running" && o.live_feedback && o.value != null;
      let state = "unavailable", percent = 0;
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
      views.set(knxPreviewKey(type, o.id), {state, percent, unit, name: o.name, live, scalingSensor: scaling && type === "sensor"});
    }
  }
  knxPreview = views;
}

function previewSourceState(key) {
  return key?.startsWith("knx:") ? knxPreview.get(key)?.state ?? "unavailable" : editor.states.get(key);
}

async function refreshKnxWidgetPreview() {
  if (knxPreviewLoading || !editor.layout?.pages?.some(p => p.widgets?.some(w => w.binding?.source === "knx"))) return;
  knxPreviewLoading = true;
  const seq = ++knxPreviewRequest;
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 10000);
  try {
    const page = await fetchKnxWidgetObjects(controller.signal);
    if (seq === knxPreviewRequest) rememberKnxPreview(page.objects, page.state);
  } catch (_) {
    if (seq === knxPreviewRequest) knxPreview.clear();
  } finally { clearTimeout(timer); knxPreviewLoading = false; }
  if (seq === knxPreviewRequest) renderCanvas();
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

// The device advertises compatible tiles using the same DPT/permission rules as the LCD.
async function fetchKnxWidgetObjects(signal) {
  const objects = [];
  let offset = 0, generation;
  for (;;) {
    const page = await apiGet(`/api/knx/objects?offset=${offset}`, signal);
    if (!Array.isArray(page.objects) || (offset && page.generation !== generation)) throw new Error(t("knx.failed"));
    generation = page.generation;
    objects.push(...page.objects);
    if (!page.has_more) return {objects, state: page.state};
    if (!Number.isInteger(page.next_offset) || page.next_offset <= offset || page.next_offset >= 128)
      throw new Error(t("knx.failed"));
    offset = page.next_offset;
  }
}

function renderKnxWriteValue(reset) {
  const object = knxPickerObjects[Number(knxField("knxTileObject").value)];
  const action = knxField("knxTileType").value === "scene_tile";
  knxField("knxTileValueWrap").classList.toggle("hidden", !action);
  const input = knxField("knxTileValue");
  if (reset) {
    const existing = knxPickerTarget?.widget?.binding;
    input.value = existing?.object_id === object?.id ? existing?.value ?? "" : "";
  }
  knxField("knxTileFormat").textContent = action ? (object?.value_format || "") : "";
  const validValue = !action || layoutJsonBytes(input.value) <= 255 &&
    (input.value.length > 0 || ["DPT-16", "DPT-24", "DPT-28"].includes(object?.datapoint_type?.split(".")[0]));
  knxField("knxTileApply").disabled = !object || !knxField("knxTileType").value || !validValue;
}

function renderKnxTileTypes(preferred) {
  const object = knxPickerObjects[Number(knxField("knxTileObject").value)];
  const select = knxField("knxTileType");
  const types = (object?.widget_types || []).filter(type =>
    knxWidgetTypes.includes(type) && (!knxPickerTarget?.widget || type === knxPickerTarget.widget.type));
  replaceKnxOptions(select, types.map(type => [type, widgetTypeLabel(type)]));
  if (types.includes(preferred)) select.value = preferred;
  renderKnxWriteValue(true);
}

async function loadKnxWidgetPicker() {
  knxPickerRequest?.abort();
  const request = new AbortController();
  knxPickerRequest = request;
  const target = knxPickerTarget;
  const select = knxField("knxTileObject"), status = knxField("knxTileStatus");
  const previous = knxPickerObjects[Number(select.value)]?.id || target?.widget?.binding?.object_id;
  const preferredType = target?.widget?.type || knxField("knxTileType").value;
  knxPickerObjects = [];
  replaceKnxOptions(select, []);
  renderKnxTileTypes();
  status.textContent = t("entity_picker.pending");
  const timer = setTimeout(() => request.abort(), 12000);
  try {
    const {objects, state} = await fetchKnxWidgetObjects(request.signal);
    if (request !== knxPickerRequest) return;
    ++knxPreviewRequest;
    rememberKnxPreview(objects, state);
    knxPickerObjects = objects.filter(o => Array.isArray(o.widget_types) &&
      (target?.widget ? o.widget_types.includes(target.widget.type) : o.widget_types.length));
    replaceKnxOptions(select, knxPickerObjects.map((o, i) =>
      [String(i), [o.name, o.id, o.group_address, o.datapoint_type].join(" · ")]));
    const index = knxPickerObjects.findIndex(o => o.id === previous);
    if (index >= 0) select.value = String(index);
    renderKnxTileTypes(preferredType);
    status.textContent = knxPickerObjects.length ? t(`knx.state.${state}`) : t("knx.widget_empty");
  } catch (error) {
    if (request === knxPickerRequest) status.textContent = t("knx.failed");
  } finally {
    clearTimeout(timer);
    if (request === knxPickerRequest) knxPickerRequest = null;
  }
}

function startKnxWidgetPicker(widget = null) {
  closeAddTileDialog(); // Cancel HA discovery and ignore late results after a source switch.
  knxPickerTarget = {page: selectedPage(), widget};
  showWidgetSource("knx");
  el.addTileOverlay.classList.remove("hidden");
  knxField("knxTileObject").focus();
  return loadKnxWidgetPicker();
}

function useKnxWidgetObject() {
  const target = knxPickerTarget;
  const o = knxPickerObjects[Number(knxField("knxTileObject").value)];
  const type = knxField("knxTileType").value;
  if (!target || selectedPage() !== target.page || !o?.widget_types.includes(type) ||
      knxField("knxTileApply").disabled) return;
  const binding = {source: "knx", object_id: o.id};
  if (type === "scene_tile") binding.value = knxField("knxTileValue").value;
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
  } else if (!addWidget(type, {entityId: "", title: o.name, binding})) return;
  closeAddTileDialog();
}

function renderKnxInspector(widget) {
  knxField("knxInspectorBinding")?.classList.toggle("hidden", !knxWidgetTypes.includes(widget?.type));
  const binding = widget?.binding, current = knxField("knxInspectorCurrent");
  knxField("knxInspectorHa")?.classList.toggle("hidden", binding?.source !== "knx");
  if (current) current.textContent = binding?.source === "knx" ? `KNX · ${binding.object_id}` : "";
  for (const input of [el.fHistoryPopup, el.fGraphHaHistory, el.fSliderEntityDomain, el.fButtonMode, el.fSensorView])
    if (input) input.disabled = binding?.source === "knx";
}

function bindKnxWidgetPicker() {
  const source = knxField("addTileSource");
  if (!source) return;
  source.onchange = () => source.value === "knx" ? startKnxWidgetPicker() : openAddTileDialog();
  knxField("knxInspectorChoose").onclick = () => startKnxWidgetPicker(selectedWidget());
  knxField("knxInspectorHa").onclick = () => openInspectorEntityPicker("primary");
  knxField("knxTileObject").onchange = () => renderKnxTileTypes();
  knxField("knxTileType").onchange = () => renderKnxWriteValue(true);
  knxField("knxTileValue").oninput = () => renderKnxWriteValue(false);
  knxField("knxTileApply").onclick = useKnxWidgetObject;
  knxField("knxTileReload").onclick = loadKnxWidgetPicker;
}
