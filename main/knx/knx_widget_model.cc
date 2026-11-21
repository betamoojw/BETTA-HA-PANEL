// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
#include "knx_widget_model.h"
#include <algorithm>
#include <cstdio>

namespace {
bool scaling(const KnxDpt& dpt) { return dpt.main == 5 && dpt.has_subtype && dpt.subtype == 1; }
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
    AddValue(attrs, o.current_value);
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
