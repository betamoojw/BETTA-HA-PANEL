// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_widget_model.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
bool scaling(const KnxDpt& dpt) { return dpt.main == 5 && dpt.has_subtype && dpt.subtype == 1; }
bool subtype(const KnxDpt& dpt, unsigned main, unsigned sub) {
    return dpt.main == main && dpt.has_subtype && dpt.subtype == sub;
}
bool numeric(const KnxDpt& dpt) {
    return (dpt.main >= 5 && dpt.main <= 9 && !(dpt.main == 6 && dpt.subtype == 20)) ||
           (dpt.main >= 12 && dpt.main <= 14) || dpt.main == 29;
}

void set_text(char *dest, size_t size, const char *text) {
    size_t n = strlen(text);
    if (n >= size) {
        n = size - 1;
        while (n && (static_cast<unsigned char>(text[n]) & 0xc0) == 0x80) --n;
    }
    memcpy(dest, text, n);
    dest[n] = 0;
}

int brightest(const knx_dpt232_color_t& rgb) { return std::max({rgb.red, rgb.green, rgb.blue}); }
}

bool KnxWidgetRoleSupports(const KnxCommunicationObject& o, knx_role_t role) {
    switch (role) {
        case KNX_SWITCH_COMMAND: return o.writable && o.datapoint_type.main == 1;
        case KNX_SWITCH_FEEDBACK: return o.readable && o.datapoint_type.main == 1;
        case KNX_BRIGHTNESS_COMMAND: return o.writable && scaling(o.datapoint_type);
        case KNX_BRIGHTNESS_FEEDBACK: return o.readable && scaling(o.datapoint_type);
        case KNX_VALUE_FEEDBACK: return o.readable;
        case KNX_NUMBER_FEEDBACK: return o.readable && numeric(o.datapoint_type);
        case KNX_FIXED_ACTION: return o.writable;
        case KNX_MOVE_COMMAND: return o.writable && (subtype(o.datapoint_type, 1, 8) || subtype(o.datapoint_type, 1, 9));
        case KNX_STOP_COMMAND: return o.writable && (subtype(o.datapoint_type, 1, 10) || subtype(o.datapoint_type, 3, 8));
        case KNX_POSITION_COMMAND: case KNX_FAN_SPEED_COMMAND: return o.writable && scaling(o.datapoint_type);
        case KNX_POSITION_FEEDBACK: case KNX_FAN_SPEED_FEEDBACK: return o.readable && scaling(o.datapoint_type);
        case KNX_TARGET_TEMPERATURE_COMMAND: return o.writable && subtype(o.datapoint_type, 9, 1);
        case KNX_TARGET_TEMPERATURE_FEEDBACK: case KNX_CURRENT_TEMPERATURE_FEEDBACK:
            return o.readable && subtype(o.datapoint_type, 9, 1);
        case KNX_HVAC_MODE_COMMAND: return o.writable && subtype(o.datapoint_type, 20, 105);
        case KNX_HVAC_MODE_FEEDBACK: return o.readable && subtype(o.datapoint_type, 20, 105);
        case KNX_PRESET_COMMAND: return o.writable && subtype(o.datapoint_type, 20, 102);
        case KNX_PRESET_FEEDBACK: return o.readable && subtype(o.datapoint_type, 20, 102);
        case KNX_ANGLE_COMMAND: return o.writable && subtype(o.datapoint_type, 5, 3);
        case KNX_ANGLE_FEEDBACK: return o.readable && subtype(o.datapoint_type, 5, 3);
        default: return false;
    }
}

// Compatibility of the original single-object layout and widget_types API field.
bool KnxWidgetSupports(const KnxCommunicationObject& o, knx_tile_kind_t kind) {
    const auto& d = o.datapoint_type;
    switch (kind) {
        case KNX_TILE_VALUE: return o.readable;
        case KNX_TILE_NUMBER: return o.readable && numeric(d);
        case KNX_TILE_BINARY: return o.readable && d.main == 1;
        case KNX_TILE_SWITCH: return o.readable && o.writable && d.main == 1;
        case KNX_TILE_SLIDER: return o.readable && o.writable && scaling(d);
        case KNX_TILE_LIGHT: return o.readable && o.writable && (d.main == 1 || scaling(d) || d.main == 232);
        case KNX_TILE_ACTION: return o.writable;
        default: return false;
    }
}

bool KnxWidgetProject(const KnxCommunicationObject& o, knx_tile_kind_t kind, ha_state_t& state) {
    if (!KnxWidgetSupports(o, kind)) return false;
    if (kind == KNX_TILE_ACTION && !o.live_feedback) {
        strcpy(state.state, "ready");
        return true;
    }
    if (!o.live_feedback || std::holds_alternative<std::monostate>(o.current_value)) return false;
    state.last_changed_unix_ms = o.last_update_ms;
    if (kind == KNX_TILE_LIGHT) {
        int level = 0;
        if (auto rgb = std::get_if<knx_dpt232_color_t>(&o.current_value)) {
            level = (brightest(*rgb) * 100 + 127) / 255;
            snprintf(state.attributes_json, sizeof(state.attributes_json),
                     "{\"supported_color_modes\":[\"rgb\"],\"rgb_color\":[%u,%u,%u],\"brightness_pct\":%d}",
                     rgb->red, rgb->green, rgb->blue, level);
            strcpy(state.state, brightest(*rgb) ? "on" : "off");
        } else if (auto percent = std::get_if<float>(&o.current_value)) {
            level = static_cast<int>(*percent + .5f);
            snprintf(state.attributes_json, sizeof(state.attributes_json),
                     "{\"supported_color_modes\":[\"brightness\"],\"brightness_pct\":%d}", level);
            strcpy(state.state, *percent > 0 ? "on" : "off");
        } else {
            strcpy(state.state, std::get<bool>(o.current_value) ? "on" : "off");
            strcpy(state.attributes_json, "{\"supported_color_modes\":[\"onoff\"]}");
        }
        return true;
    }
    cJSON *attrs = cJSON_CreateObject();
    if (!attrs) return false;
    if (!AddValue(attrs, o.current_value)) { cJSON_Delete(attrs); return false; }
    cJSON *value = cJSON_DetachItemFromObjectCaseSensitive(attrs, "value");
    const bool scaling_sensor = kind == KNX_TILE_VALUE && scaling(o.datapoint_type);
    bool ok = value && cJSON_AddStringToObject(attrs, "unit_of_measurement",
                   o.unit.empty() && scaling(o.datapoint_type) ? "%" : o.unit.c_str()) &&
              (!scaling_sensor || cJSON_AddTrueToObject(attrs, "knx_scaling")) &&
              (!scaling_sensor || o.display_precision < 0 || cJSON_AddNumberToObject(attrs, "display_precision", o.display_precision)) &&
              cJSON_PrintPreallocated(attrs, state.attributes_json, sizeof(state.attributes_json), false);
    if (ok) {
        if (cJSON_IsBool(value)) strcpy(state.state, cJSON_IsTrue(value) ? "on" : "off");
        else if (cJSON_IsString(value)) set_text(state.state, sizeof(state.state), value->valuestring);
        else {
            char *formatted = cJSON_PrintUnformatted(value);
            ok = formatted != nullptr;
            if (ok) set_text(state.state, sizeof(state.state), formatted);
            cJSON_free(formatted);
        }
    }
    cJSON_Delete(value);
    cJSON_Delete(attrs);
    return ok;
}

bool KnxWidgetCommandValue(const KnxCommunicationObject& o, knx_widget_action_t action,
                           int value, const char *preset, std::string& text) {
    if (!o.writable) return false;
    if (action == KNX_SET_PRESET || action == KNX_SET_ANGLE) {
        const bool preset_value = action == KNX_SET_PRESET;
        if (!KnxWidgetRoleSupports(o, preset_value ? KNX_PRESET_COMMAND : KNX_ANGLE_COMMAND) ||
            value < 0 || value > (preset_value ? 4 : 360)) return false;
        text = std::to_string(value);
        return true;
    }
    if (action == KNX_COVER_OPEN || action == KNX_COVER_CLOSE) {
        if (!KnxWidgetRoleSupports(o, KNX_MOVE_COMMAND)) return false;
        text = action == KNX_COVER_OPEN ? "off" : "on";
        return true;
    }
    if (action == KNX_COVER_STOP) {
        if (!KnxWidgetRoleSupports(o, KNX_STOP_COMMAND)) return false;
        text = o.datapoint_type.main == 1 ? "off" : "false,0";
        return true;
    }
    if (action == KNX_SET_TARGET_TEMPERATURE) {
        if (!KnxWidgetRoleSupports(o, KNX_TARGET_TEMPERATURE_COMMAND)) return false;
        char temperature[32];
        snprintf(temperature, sizeof(temperature), "%.1f", value / 10.0);
        KnxValue parsed;
        if (!KnxParseValue(o.datapoint_type, temperature, parsed)) return false;
        text = temperature;
        return true;
    }
    if (action == KNX_SET_HVAC_MODE) {
        if (!KnxWidgetRoleSupports(o, KNX_HVAC_MODE_COMMAND) || !knx_hvac_mode_name(value)) return false;
        text = std::to_string(value);
        return true;
    }
    if (action == KNX_SET_FAN_PERCENT) {
        if (!KnxWidgetRoleSupports(o, KNX_FAN_SPEED_COMMAND) || value < 0 || value > 100) return false;
        text = std::to_string(value);
        return true;
    }
    if (action == KNX_SEND_VALUE) {
        if (!preset || strlen(preset) > KNX_COMMAND_VALUE_MAX) return false;
        text = preset;
        return true; // The queue validates the complete configured DPT.
    }
    if (action == KNX_TOGGLE) {
        if (!o.readable || !o.live_feedback || o.datapoint_type.main != 1) return false;
        auto b = std::get_if<bool>(&o.current_value);
        if (!b) return false;
        text = *b ? "off" : "on";
        return true;
    }
    if (action == KNX_SET_POWER && o.datapoint_type.main == 1) {
        text = value ? "on" : "off";
        return true;
    }
    if (action == KNX_SET_POWER) value = value ? 100 : 0;
    const bool level = action == KNX_SET_PERCENT || action == KNX_SET_POWER;
    if (level && (value < 0 || value > 100)) return false;
    if (level && scaling(o.datapoint_type)) {
        text = std::to_string(value);
        return true;
    }
    if (o.datapoint_type.main != 232) return false;
    int red, green, blue;
    if (level) {
        auto rgb = std::get_if<knx_dpt232_color_t>(&o.current_value);
        if (!o.readable || !o.live_feedback || !rgb) return false;
        const int peak = brightest(*rgb);
        const int target = (value * 255 + 50) / 100;
        auto channel = [peak, target](int component) { return peak ? (component * target + peak / 2) / peak : target; };
        red = channel(rgb->red); green = channel(rgb->green); blue = channel(rgb->blue);
    } else if (action == KNX_SET_RGB && value >= 0 && value <= 0xffffff) {
        red = value >> 16; green = (value >> 8) & 255; blue = value & 255;
    } else return false;
    char color[16];
    snprintf(color, sizeof(color), "%d,%d,%d", red, green, blue);
    text = color;
    return true;
}

std::optional<float> KnxControlScalar(const KnxValue& value) {
    if (const auto* v = std::get_if<float>(&value); v && std::isfinite(*v)) return *v;
    if (const auto* v = std::get_if<bool>(&value)) return *v;
    if (const auto* v = std::get_if<uint8_t>(&value)) return *v;
    return std::nullopt;
}

std::optional<float> KnxWidgetDisplayValue(const knx_control_binding_t& binding, knx_role_t feedback_role,
    const KnxCommunicationObject* feedback, const KnxCommunicationObject* command) {
    if (feedback || binding.id[feedback_role][0])
        return feedback && feedback->live_feedback ? KnxControlScalar(feedback->current_value) : std::nullopt;
    if (command && command->command_revision) return command->last_command;
    return std::nullopt;
}

bool KnxWidgetProjectRoles(const KnxCommunicationObject *const objects[KNX_ROLE_COUNT], knx_tile_kind_t kind,
                          bool active, knx_control_binding_t& binding, ha_state_t& state) {
    auto object = [&](knx_role_t role) -> const KnxCommunicationObject* {
        const auto* o = objects[role];
        return o && knx_tile_has_role(kind, role) && KnxWidgetRoleSupports(*o, role) ? o : nullptr;
    };
    auto display = [&](knx_role_t role) -> std::optional<float> {
        if (!active) return std::nullopt;
        const auto preceding = static_cast<knx_role_t>(role - 1);
        const auto* command = knx_command_feedback(preceding) == role ? object(preceding) : nullptr;
        return KnxWidgetDisplayValue(binding, role, object(role), command);
    };
    binding.writable_roles = 0;
    state.attributes_json[0] = '\0';
    state.last_changed_unix_ms = 0;
    for (int i = 0; i < KNX_ROLE_COUNT; ++i) {
        auto role = static_cast<knx_role_t>(i);
        const auto* o = object(role);
        if (active && o && knx_role_is_command(role)) binding.writable_roles |= UINT32_C(1) << i;
        if (active && o && !knx_role_is_command(role) && o->live_feedback)
            state.last_changed_unix_ms = std::max<int64_t>(state.last_changed_unix_ms, o->last_update_ms);
    }
    binding.switch_writable = knx_control_can_write(&binding, KNX_SWITCH_COMMAND);
    binding.brightness_writable = knx_control_can_write(&binding, KNX_BRIGHTNESS_COMMAND);
    const auto percent = display(KNX_BRIGHTNESS_FEEDBACK);
    const auto on = display(KNX_SWITCH_FEEDBACK);
    binding.brightness = percent ? static_cast<int>(*percent + .5f) : -1;

    if (kind == KNX_TILE_LIGHT || kind == KNX_TILE_SWITCH || kind == KNX_TILE_SLIDER) {
        const auto level = knx_angle_slider(&binding) ? display(KNX_ANGLE_FEEDBACK) : percent;
        if (kind == KNX_TILE_SLIDER && level)
            snprintf(state.state, sizeof(state.state), "%d", static_cast<int>(*level + .5f));
        else if (on) strcpy(state.state, *on ? "on" : "off");
        else {
            const auto cmd = kind != KNX_TILE_SLIDER ? KNX_SWITCH_COMMAND :
                (knx_angle_slider(&binding) ? KNX_ANGLE_COMMAND : KNX_BRIGHTNESS_COMMAND);
            strcpy(state.state, knx_control_can_write(&binding, cmd) && !binding.id[knx_command_feedback(cmd)][0]
                ? "unknown" : "unavailable");
        }
        return true;
    }
    strcpy(state.state, active ? "unknown" : "unavailable");
    cJSON* attrs = cJSON_CreateObject();
    if (!attrs) return false;
    bool ok = true;
    if (kind == KNX_TILE_SELECT) {
        static const char* const names[] = {"Auto", "Comfort", "Standby", "Economy", "Building Protection"};
        const auto code = display(KNX_PRESET_FEEDBACK);
        if (code && *code >= 0 && *code < 5) strcpy(state.state, names[static_cast<unsigned>(*code)]);
        if (knx_control_can_write(&binding, KNX_PRESET_COMMAND))
            ok = cJSON_AddItemToObject(attrs, "options", cJSON_CreateStringArray(names, 5));
    } else if (kind == KNX_TILE_COVER) {
        const auto position = display(KNX_POSITION_FEEDBACK);
        if (position) {
            const float raw_open = binding.position_open_at_100 ? *position : 100 - *position;
            const int open = std::clamp(static_cast<int>(raw_open + .5f), 0, 100);
            strcpy(state.state, open ? "open" : "closed");
            ok = cJSON_AddNumberToObject(attrs, "current_position", open);
        }
        unsigned features = knx_control_can_write(&binding, KNX_MOVE_COMMAND) ||
            knx_control_can_write(&binding, KNX_POSITION_COMMAND) ? 3U : 0U;
        if (knx_control_can_write(&binding, KNX_POSITION_COMMAND)) features |= 4U;
        if (knx_control_can_write(&binding, KNX_STOP_COMMAND)) features |= 8U;
        ok = ok && cJSON_AddNumberToObject(attrs, "supported_features", features);
    } else if (kind == KNX_TILE_CLIMATE) {
        const auto current = display(KNX_CURRENT_TEMPERATURE_FEEDBACK);
        const auto target = display(KNX_TARGET_TEMPERATURE_FEEDBACK);
        const auto mode_code = display(KNX_HVAC_MODE_FEEDBACK);
        const char* mode = mode_code ? knx_hvac_mode_name(*mode_code) : nullptr;
        if (mode) strcpy(state.state, mode);
        ok = (!current || cJSON_AddNumberToObject(attrs, "current_temperature", *current)) &&
             (!target || cJSON_AddNumberToObject(attrs, "temperature", *target)) &&
             cJSON_AddStringToObject(attrs, "temperature_unit", "C") &&
             cJSON_AddNumberToObject(attrs, "min_temp", 7) && cJSON_AddNumberToObject(attrs, "max_temp", 35);
        cJSON* modes = cJSON_AddArrayToObject(attrs, "hvac_modes");
        ok = ok && modes;
        if (modes && knx_control_can_write(&binding, KNX_HVAC_MODE_COMMAND))
            for (int code : {0, 1, 3, 6, 9, 14})
                ok = cJSON_AddItemToArray(modes, cJSON_CreateString(knx_hvac_mode_name(code))) && ok;
    } else if (kind == KNX_TILE_FAN) {
        const auto speed = display(KNX_FAN_SPEED_FEEDBACK);
        if (on) strcpy(state.state, *on ? "on" : "off");
        ok = (!speed || cJSON_AddNumberToObject(attrs, "percentage", std::clamp(static_cast<int>(*speed + .5f), 0, 100))) &&
             cJSON_AddNumberToObject(attrs, "percentage_step", 25) &&
             cJSON_AddNumberToObject(attrs, "supported_features", knx_control_can_write(&binding, KNX_FAN_SPEED_COMMAND) ? 1 : 0);
    } else ok = false;
    ok = ok && cJSON_PrintPreallocated(attrs, state.attributes_json, sizeof(state.attributes_json), false);
    cJSON_Delete(attrs);
    return ok;
}
