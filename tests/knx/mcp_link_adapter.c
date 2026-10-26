/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "xiaozhi/xiaozhi_mcp_tools.h"
#include <string.h>

bool xiaozhi_mcp_device_tool_supported(const char *name)
{
    return !strcmp(name, "self.get_device_status");
}

cJSON *xiaozhi_mcp_device_tool_call(const char *name, const cJSON *arguments,
                                  char *error, size_t error_size)
{
    (void)arguments; (void)error; (void)error_size;
    if (strcmp(name, "self.get_device_status")) return NULL;
    return cJSON_Parse("{\"archive_adapter_called\":true}");
}
