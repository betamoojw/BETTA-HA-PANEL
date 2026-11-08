/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_alarm_panel_client.h"
#include "ha/ha_client.h"
#include "net/net_mgr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static uint32_t next_owner;
uint32_t ha_alarm_panel_open(void) { return ++next_owner; }
void ha_alarm_panel_close(uint32_t owner) { (void)owner; }
void ha_alarm_panel_cancel(uint32_t owner) { (void)owner; }
esp_err_t ha_alarm_panel_submit(uint32_t owner, cJSON *command, bool history)
{
    (void)owner;
    esp_err_t result = ESP_OK;
    if (!history) {
        const char *domain = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(command,"domain"));
        const char *service = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(command,"service"));
        char *data = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(command,"service_data"));
        result = ha_client_call_service(domain,service,data);
        free(data);
    }
    cJSON_Delete(command);
    return result;
}
bool ha_alarm_panel_poll(uint32_t owner, bool *success, bool *history, cJSON **reply)
{ (void)owner; (void)success; (void)history; (void)reply; return false; }
bool net_mgr_uses_ethernet(void) { return false; }
bool net_mgr_is_connected(void) { return true; }
esp_err_t net_mgr_get_ip(char *out, size_t size) { snprintf(out,size,"192.0.2.10"); return ESP_OK; }
