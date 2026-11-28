/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Assistant tool entry point for layout-scoped Home Assistant requests.
 */
#pragma once
#include <stddef.h>
#include "cJSON.h"

/** @brief Dispatch through the live layout allowlist, shared model and bounded entity controls.
 * @param name Exact tool name.
 * @param arguments Borrowed tool arguments; discovery continuation uses the preceding layout signature.
 * @param[out] error Buffer for a failure diagnostic.
 * @param error_size Capacity of error in bytes.
 * @return Caller-owned result, or NULL on failure. A control receipt indicates submission only.
 */
cJSON *xiaozhi_mcp_ha_call(const char *name, const cJSON *arguments, char *error, size_t error_size);
