/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once

#include "cJSON.h"
#include "layout/knx_binding.h"

/* Saved IDs can be resolved again after an offline layout restore. */
static inline bool knx_layout_binding(const cJSON *binding, const char *type,
                                      const char **object_id, const char **write_value)
{
    const char *source = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(binding, "source"));
    *object_id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(binding, "object_id"));
    const cJSON *preset = cJSON_GetObjectItemCaseSensitive(binding, "value");
    *write_value = cJSON_GetStringValue(preset);
    knx_tile_kind_t kind = knx_tile_kind(type);
    if (!cJSON_IsObject(binding) || !source || strcmp(source, "knx") ||
        !*object_id || !**object_id || strlen(*object_id) > KNX_OBJECT_ID_MAX ||
        kind == KNX_TILE_NONE) return false;
    if (kind == KNX_TILE_ACTION)
        return cJSON_GetArraySize(binding) == 3 && *write_value && strlen(*write_value) <= KNX_COMMAND_VALUE_MAX;
    return cJSON_GetArraySize(binding) == 2 && !preset;
}
