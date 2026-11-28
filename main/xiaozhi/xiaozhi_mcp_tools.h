/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Capability query and dispatch interfaces for assistant device tools.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

/** @return True if the current board provides an adapter for the named tool. */
bool xiaozhi_mcp_device_tool_supported(const char *name);
/** @return Caller-owned Wi-Fi and network snapshot; unknown fields are JSON null. */
cJSON *xiaozhi_mcp_network_info(void);
/** @brief Dispatch an already authorized device tool using borrowed arguments.
 * @return Caller-owned JSON result, or NULL with a diagnostic on failure.
 */
cJSON *xiaozhi_mcp_device_tool_call(const char *name, const cJSON *arguments, char *error, size_t error_size);
