/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Room-air binding metadata, slot extraction and compatibility defaults.
 */
#pragma once
#include "app_config.h"
#include "cJSON.h"
#include "ui/widgets/room_air_model.h"

/** @brief Persisted field, pipe-separated allowed domains, and its destination slot. */
typedef struct
{
    const char *key, *domains;
    int slot;
} air_binding_t;

/** @brief Map roles to primary/secondary/tertiary slots (negative) or extra slots (nonnegative). */
extern const air_binding_t air_bindings[AIR_ROLE_COUNT];
/** @brief Clear the destination slots and copy bounded entity IDs from the widget.
 *
 * The three direct slots and six extra slots must each hold APP_MAX_ENTITY_ID_LEN bytes.
 * When outdoor inputs are explicitly disabled, their slots remain empty.
 */
void air_slots_from_json(const cJSON *json, char *primary, char *secondary, char *tertiary, char (*extra)[APP_MAX_ENTITY_ID_LEN]);
/** @brief Initialize the fixed evaluation bands in the supplied destination.
 *
 * Older layouts may still contain custom thresholds; this reader ignores them.
 * @return Always true after initializing a valid output pointer.
 */
bool air_config_from_json(const cJSON *json, air_config_t *out);
