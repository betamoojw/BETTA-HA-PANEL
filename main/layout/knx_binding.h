/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once

#include <stdbool.h>
#include <string.h>

/* The layout stores the original site-defined ID. Only the native widget
 * instances use this prefix, which cannot collide with an HA entity ID. */
#define KNX_BINDING_PREFIX "knx:"
#define KNX_OBJECT_ID_MAX 48
#define KNX_COMMAND_VALUE_MAX 255

static inline const char *knx_binding_id(const char *key)
{
    return key && strncmp(key, KNX_BINDING_PREFIX, sizeof(KNX_BINDING_PREFIX) - 1) == 0
        ? key + sizeof(KNX_BINDING_PREFIX) - 1 : NULL;
}

typedef enum {
    KNX_TILE_NONE, KNX_TILE_VALUE, KNX_TILE_NUMBER, KNX_TILE_BINARY,
    KNX_TILE_SWITCH, KNX_TILE_SLIDER, KNX_TILE_LIGHT, KNX_TILE_ACTION
} knx_tile_kind_t;

static inline knx_tile_kind_t knx_tile_kind(const char *type)
{
    if (!type) return KNX_TILE_NONE;
    if (!strcmp(type, "sensor")) return KNX_TILE_VALUE;
    if (!strcmp(type, "gauge") || !strcmp(type, "graph")) return KNX_TILE_NUMBER;
    if (!strcmp(type, "binary_tile") || !strcmp(type, "contact_tile")) return KNX_TILE_BINARY;
    if (!strcmp(type, "button")) return KNX_TILE_SWITCH;
    if (!strcmp(type, "slider")) return KNX_TILE_SLIDER;
    if (!strcmp(type, "light_tile")) return KNX_TILE_LIGHT;
    if (!strcmp(type, "scene_tile")) return KNX_TILE_ACTION;
    return KNX_TILE_NONE;
}

/* The first four roles are independently routable control endpoints. */
typedef enum {
    KNX_SWITCH_COMMAND, KNX_SWITCH_FEEDBACK, KNX_BRIGHTNESS_COMMAND, KNX_BRIGHTNESS_FEEDBACK,
    KNX_VALUE_FEEDBACK, KNX_NUMBER_FEEDBACK, KNX_FIXED_ACTION, KNX_ROLE_COUNT
} knx_role_t;
#define KNX_CONTROL_ROLES 4

static inline const char *knx_role_name(knx_role_t role)
{
    static const char *const names[] = {"switch_command", "switch_feedback", "brightness_command",
        "brightness_feedback", "value_feedback", "number_feedback", "action"};
    return names[role];
}

static inline bool knx_tile_has_role(knx_tile_kind_t kind, knx_role_t role)
{
    switch (kind) {
    case KNX_TILE_LIGHT: return role < KNX_CONTROL_ROLES;
    case KNX_TILE_SWITCH: return role == KNX_SWITCH_COMMAND || role == KNX_SWITCH_FEEDBACK;
    case KNX_TILE_SLIDER: return role == KNX_BRIGHTNESS_COMMAND || role == KNX_BRIGHTNESS_FEEDBACK;
    case KNX_TILE_BINARY: return role == KNX_SWITCH_FEEDBACK;
    case KNX_TILE_NUMBER: return role == KNX_NUMBER_FEEDBACK;
    case KNX_TILE_VALUE: return role == KNX_VALUE_FEEDBACK;
    case KNX_TILE_ACTION: return role == KNX_FIXED_ACTION;
    default: return false;
    }
}

/* Only interactive KNX widgets own this data. The LCD updates the capability
 * flags and confirmed brightness together with the state under its display lock. */
typedef struct {
    char id[KNX_CONTROL_ROLES][KNX_OBJECT_ID_MAX + 1];
    bool switch_writable, brightness_writable;
    int brightness; /* -1 until external brightness feedback */
} knx_control_binding_t;
