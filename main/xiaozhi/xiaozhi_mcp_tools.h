/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

bool xiaozhi_mcp_device_tool_supported(const char *name);
/* Read-only Wi-Fi/ESP-NETIF snapshot; unknown fields are JSON null. */
cJSON *xiaozhi_mcp_network_info(void);
cJSON *xiaozhi_mcp_device_tool_call(const char *name, const cJSON *arguments,
                                    char *error, size_t error_size);
