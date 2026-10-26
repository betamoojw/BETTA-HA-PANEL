/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha_service_request.h"
#include <string.h>

cJSON *ha_service_request_create(uint32_t id, const char *domain, const char *service, const char *json_data)
{
    if (!domain || !*domain || !service || !*service) return NULL;
    cJSON *data = json_data && *json_data ?
        cJSON_ParseWithLengthOpts(json_data, strlen(json_data) + 1, NULL, true) : cJSON_CreateObject();
    if (!cJSON_IsObject(data)) { cJSON_Delete(data); return NULL; }
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddNumberToObject(root, "id", id) ||
        !cJSON_AddStringToObject(root, "type", "call_service") ||
        !cJSON_AddStringToObject(root, "domain", domain) ||
        !cJSON_AddStringToObject(root, "service", service) ||
        !cJSON_AddItemToObject(root, "service_data", data)) {
        cJSON_Delete(data); cJSON_Delete(root); return NULL;
    }
    return root;
}
