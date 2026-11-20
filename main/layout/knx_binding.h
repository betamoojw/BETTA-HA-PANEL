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
