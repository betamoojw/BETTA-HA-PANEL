/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Registry-independent parsing of persisted KNX widget bindings.
 */
#pragma once

#include "cJSON.h"
#include "layout/knx_binding.h"

/** @brief Parsed binding view; its strings are borrowed from the original JSON tree. */
typedef struct
{
    const char *primary, *preset;
    const char *controls[KNX_CONTROL_ROLES];
    bool position_open_at_100;
} knx_layout_binding_t;

/** @return True for a nonempty ID within the byte limit, without consulting the registry. */
static inline bool knx_valid_id(const char *id)
{
    return id && *id && strlen(id) <= KNX_OBJECT_ID_MAX;
}

/** @brief Validate legacy single-object or role-based binding syntax without a registry.
 *
 * A missing or retyped object can recover by ID after restore. Runtime
 * resolution separately checks its current datapoint type and permissions.
 *
 * @param binding Borrowed binding object.
 * @param type Saved widget type, which selects the allowed roles.
 * @param[out] out Required destination for borrowed references. Ignore it on failure.
 * @return True for an accepted binding with at least one usable ID.
 */
static inline bool knx_layout_binding(const cJSON *binding, const char *type, knx_layout_binding_t *out)
{
    memset(out, 0, sizeof(*out));
    knx_tile_kind_t kind = knx_tile_kind(type);
    const char *source = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(binding, "source"));
    const cJSON *object = cJSON_GetObjectItemCaseSensitive(binding, "object_id");
    const cJSON *roles = cJSON_GetObjectItemCaseSensitive(binding, "roles");
    const cJSON *preset = cJSON_GetObjectItemCaseSensitive(binding, "value");
    const cJSON *position = cJSON_GetObjectItemCaseSensitive(binding, "position_open_at_100");
    out->preset = cJSON_GetStringValue(preset);
    if (!cJSON_IsObject(binding) || !source || strcmp(source, "knx") || kind == KNX_TILE_NONE || (!!object == !!roles) ||
        cJSON_GetArraySize(binding) != (kind == KNX_TILE_ACTION ? 3 : 2) + (position != NULL) ||
        (position && (kind != KNX_TILE_COVER || !cJSON_IsBool(position))) ||
        (kind == KNX_TILE_ACTION ? !out->preset || strlen(out->preset) > KNX_COMMAND_VALUE_MAX : preset != NULL))
    {
        return false;
    }
    out->position_open_at_100 = cJSON_IsTrue(position);

    if (object)
    {
        if (kind == KNX_TILE_COVER || kind == KNX_TILE_CLIMATE || kind == KNX_TILE_FAN || kind == KNX_TILE_SELECT)
        {
            return false;
        }
        out->primary = cJSON_GetStringValue(object);
        if (!knx_valid_id(out->primary))
        {
            return false;
        }
        if (kind == KNX_TILE_SWITCH || kind == KNX_TILE_SLIDER)
        {
            for (int i = 0; i <= KNX_BRIGHTNESS_FEEDBACK; ++i)
            {
                if (knx_tile_has_role(kind, (knx_role_t) i))
                {
                    out->controls[i] = out->primary;
                }
            }
        }
        return true;
    }
    if (!cJSON_IsObject(roles))
    {
        return false;
    }
    unsigned seen = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, roles)
    {
        int i = 0;
        while (i < KNX_ROLE_COUNT && strcmp(item->string, knx_role_name((knx_role_t) i)))
        {
            ++i;
        }
        const char *id = cJSON_GetStringValue(item);
        if (i == KNX_ROLE_COUNT || !knx_tile_has_role(kind, (knx_role_t) i) || (seen & (1u << i)))
        {
            return false;
        }
        seen |= 1u << i;
        if (!knx_role_is_command((knx_role_t) i) && (cJSON_IsNull(item) || (id && !*id)))
        {
            continue;
        }
        if (!knx_valid_id(id))
        {
            return false;
        }
        if (knx_tile_has_controls(kind))
        {
            out->controls[i] = id;
        }
        if (!out->primary || i == KNX_SWITCH_FEEDBACK || (kind == KNX_TILE_SLIDER && i == KNX_BRIGHTNESS_FEEDBACK))
        {
            out->primary = id;
        }
    }
    if (kind == KNX_TILE_SLIDER && (out->controls[KNX_ANGLE_COMMAND] || out->controls[KNX_ANGLE_FEEDBACK]) &&
        (out->controls[KNX_BRIGHTNESS_COMMAND] || out->controls[KNX_BRIGHTNESS_FEEDBACK]))
    {
        return false;
    }
    return out->primary != NULL;
}
