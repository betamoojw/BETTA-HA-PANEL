/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include <stddef.h>
#include "cJSON.h"

/* Live layout allowlist, shared HA model and bounded entity controls. */
cJSON *xiaozhi_mcp_ha_call(const char *name, const cJSON *arguments,
                          char *error, size_t error_size);
