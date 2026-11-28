/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Capability descriptions and command execution for permitted HA entities.
 */
#pragma once
#include "ha/ha_model.h"
#include "cJSON.h"

/* Capability descriptions and dispatch share the same policy. The caller must
 * first verify the entity belongs to the current saved layout. */
/** @brief Describe controls for an entity; advertise no actions while the model is not ready.
 * @return Caller-owned JSON, or NULL on allocation failure.
 */
cJSON *xiaozhi_ha_control_info(const ha_state_t *state, bool ready);

/** @brief Submit a validated service call. The caller must enforce the current layout allowlist.
 * @param state Borrowed, usable snapshot of the selected entity.
 * @param args Borrowed action and optional value from the tool request.
 * @param[out] error Buffer for a failure diagnostic.
 * @param error_size Capacity of error in bytes.
 * @return Caller-owned submission receipt, never confirmation of entity state; NULL on failure.
 */
cJSON *xiaozhi_ha_control_execute(const ha_state_t *state, const cJSON *args, char *error, size_t error_size);
