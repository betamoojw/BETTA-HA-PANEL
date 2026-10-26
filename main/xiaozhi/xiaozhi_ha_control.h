/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "ha/ha_model.h"
#include "cJSON.h"

/* Capability descriptions and dispatch share the same policy. The caller must
 * first verify the entity belongs to the current saved layout. */
cJSON *xiaozhi_ha_control_info(const ha_state_t *state, bool ready);
cJSON *xiaozhi_ha_control_execute(const ha_state_t *state, const cJSON *args,
                                char *error, size_t error_size);
