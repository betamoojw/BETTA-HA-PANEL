/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "knx/knx.h"
#include "knx/knx_mcp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *knx_status(void) { return cJSON_CreateObject(); }
cJSON *knx_objects(int offset, int limit) { (void)offset; (void)limit; return cJSON_CreateObject(); }
cJSON *knx_object(const char *id) { (void)id; return NULL; }
bool knx_command(const char *id, const char *address, const char *value,
                 bool write, char *error, size_t size)
{
    (void)id; (void)address; (void)value; (void)write; (void)error; (void)size;
    assert(!"Discovery must not send KNX commands");
    return false;
}

int main(void)
{
    /* Reference only the dispatcher, as the firmware worker does. The device
     * adapter must be pulled from the archive without a direct main reference. */
    cJSON *request = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
    cJSON *response = knx_mcp_handle(request);
    char *text = cJSON_PrintUnformatted(response);
    assert(text && strstr(text, "self.get_device_status"));
    free(text); cJSON_Delete(response); cJSON_Delete(request);

    request = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{}}}");
    response = knx_mcp_handle(request);
    text = cJSON_PrintUnformatted(response);
    assert(text && strstr(text, "archive_adapter_called"));
    free(text); cJSON_Delete(response); cJSON_Delete(request);
    puts("MCP static archive adapter discovery and dispatch passed");
}
