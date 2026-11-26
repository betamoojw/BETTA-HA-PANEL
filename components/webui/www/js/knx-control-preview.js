/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */

// Keep each role's confirmed feedback separate from the commands it can send.
// These records use HA-shaped attributes so the existing preview geometry can be reused.
const knxHvacModes = {0: "auto", 1: "heat", 3: "cool", 6: "off", 9: "fan_only", 14: "dry"};

function knxControlPreviewKey(widget) {
  const {type, binding} = widget, roles = binding.roles || {};
  const key = `knx:roles:${type}:${JSON.stringify([knxTileRoles[type].map(role => roles[role]), binding.position_open_at_100 === true])}`;
  const object = role => {
    const found = knxRegistry.get(roles[role]);
    return found?.widget_roles?.includes(role) ? found : null;
  };
  const value = role => knxRoleValue(roles, role);
  const number = role => {
    const raw = value(role);
    return typeof raw === "number" && Number.isFinite(raw) ? raw : null;
  };
  const percent = role => {
    const raw = number(role);
    return raw !== null && raw >= 0 && raw <= 100 ? Math.round(raw) : null;
  };
  const command = role => knxRoutingActive && object(role)?.writable === true;
  const view = {state: knxRoutingActive ? "unknown" : "unavailable", live: false};
  if (type === "cover_tile") {
    const raw = number("position_feedback");
    const position = raw !== null && raw >= 0 && raw <= 100 ? raw : null;
    view.current_position = position === null ? null : Math.round(binding.position_open_at_100 === true ? position : 100 - position);
    view.canMove = command("move_command") || command("position_command");
    view.canStop = command("stop_command");
    view.hasStop = view.canStop;
    view.supported_features = (view.canMove ? 3 : 0) | (view.canStop ? 8 : 0);
    view.live = view.current_position !== null;
    if (view.live) view.state = view.current_position > 0 ? "open" : "closed";
  } else if (type === "climate_tile") {
    view.temperature = number("target_temperature_feedback");
    view.current_temperature = number("current_temperature_feedback");
    view.temperature_unit = "°C";
    view.canTemperature = command("target_temperature_command") && view.temperature !== null;
    view.canMode = command("hvac_mode_command");
    view.hasMode = !!(object("hvac_mode_command") || object("hvac_mode_feedback"));
    const mode = number("hvac_mode_feedback");
    if (mode !== null) view.state = knxHvacModes[mode] || "unknown";
    view.live = view.temperature !== null || view.current_temperature !== null || mode !== null;
  } else if (type === "fan_tile") {
    const power = value("switch_feedback");
    const knownPower = typeof power === "boolean";
    if (knownPower) view.state = power ? "on" : "off";
    view.percentage = percent("fan_speed_feedback");
    view.percentage_step = 25;
    view.canPower = command("switch_command") && (knownPower || !roles.switch_feedback);
    view.canSpeed = command("fan_speed_command");
    view.canStep = view.canSpeed && view.percentage !== null;
    view.hasSpeed = view.canSpeed;
    view.supported_features = view.canSpeed ? 1 : 0;
    view.live = knownPower || view.percentage !== null;
  }
  knxPreview.set(key, view);
  return key;
}

function knxControlPreviewMarkup(widget) {
  const view = knxPreview.get(widget.entity_id) || {};
  const name = (tag, attributes = "") => widgetNamePreview(widget, tag, attributes);
  const w = Number(widget.rect?.w) || 0, h = Number(widget.rect?.h) || 0;
  const handle = '<div class="resize-handle"></div>';
  const unknown = escapeHtml(t(view.state === "unavailable" ? "knx.unavailable" : "knx.unknown"));
  if (widget.type === "cover_tile") {
    const position = view.current_position;
    const status = position == null ? unknown : `${position} %`;
    return `<div class="tp tp-blind" style="--btn:${clamp(Math.min(h - 24, w < 300 ? 40 : 44), 32, 44)}px"><div class="tp-blind-text">${name("b")}<small>${status}</small><div class="tp-bar"><i style="width:${position ?? 0}%"></i></div></div>
      <div class="tp-btns"><span class="${previewControlClass(false, view.canMove)}">↑</span>${view.hasStop ? `<span class="${previewControlClass(false, view.canStop)}">■</span>` : ""}<span class="${previewControlClass(false, view.canMove)}">↓</span></div></div>${handle}`;
  }
  if (widget.type === "climate_tile") {
    const temperature = number => number == null ? "--" : `${number.toFixed(1)}°C`;
    const mode = ["unknown", "unavailable"].includes(view.state) ? "--" : escapeHtml(t(`knx.hvac_${view.state}`));
    const current = view.state === "unavailable" ? unknown : view.current_temperature == null ? "" : pvText("climate_now", {temp: temperature(view.current_temperature)});
    return `<div class="tp tp-climate">${name("div", 'class="tp-name" style="text-align:left"')}<small>${current}</small>
      <div class="tp-clim-mid" style="--btn:${h >= 220 ? 64 : 48}px"><span class="${previewControlClass(false, view.canTemperature)}">−</span><b>${temperature(view.temperature)}</b><span class="${previewControlClass(false, view.canTemperature)}">+</span></div>
      <div class="tp-clim-modes"><i class="${previewControlClass(false, view.canMode)}">${mode}</i></div></div>${handle}`;
  }
  const on = view.state === "on", powerKnown = on || view.state === "off";
  const power = powerKnown ? escapeHtml(t(on ? "layout.preview.on" : "layout.preview.off")) : "";
  const status = view.percentage == null ? power || unknown : [power, `${view.percentage} %`].filter(Boolean).join(" · ");
  if (h >= 220 && w >= 200) {
    const speeds = view.hasSpeed ? Array.from({length: 4}, (_, i) => `<i class="${previewControlClass(view.percentage === (i + 1) * 25, view.canSpeed)}">${(i + 1) * 25}%</i>`).join("") : "";
    const columns = 4 * (previewTextWidth("100%", textRoleSize("meta", 20)) + 8) + 24 > w - 24 ? 2 : 4;
    return `<div class="tp tp-fanbig tp-knx-fan${on ? "" : " fan-off"}"><div class="tp-fanbig-top">${name("b")}<small>${status}</small><span class="tp-fan-power ${previewControlClass(on, view.canPower)}">⏻</span></div><div class="tp-rotor"><svg viewBox="0 0 100 100" aria-hidden="true">
      <circle class="tp-fan-guard" cx="50" cy="50" r="46"/>
      <g class="tp-fan-blades">${[0, 120, 240].map(angle => `<path transform="rotate(${angle} 50 50)" d="M47 46 C34 39 30 25 38 16 C43 10 57 12 66 18 C75 26 67 40 53 47 Z"/>`).join("")}</g>
      <circle class="tp-fan-hub" cx="50" cy="50" r="8"/></svg></div>${speeds ? `<div class="tp-chips tp-knx-speeds" style="--knx-speed-columns:${columns}">${speeds}</div>` : ""}</div>${handle}`;
  }
  const speedButton = symbol => `<span class="${previewControlClass(false, view.canStep)}">${symbol}</span>`;
  return `<div class="tp tp-blind" style="--btn:${clamp(h - 24, 32, 44)}px"><div class="tp-blind-text">${name("b")}<small>${status}</small><div class="tp-bar"><i style="width:${view.percentage ?? 0}%"></i></div></div>
    <div class="tp-btns">${view.hasSpeed ? speedButton("−") : ""}<span class="tp-primary ${previewControlClass(on, view.canPower)}">⏻</span>${view.hasSpeed ? speedButton("+") : ""}</div></div>${handle}`;
}
