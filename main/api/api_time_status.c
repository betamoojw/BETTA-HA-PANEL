/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723; adapted from betamoojw commit 4c8e40e */
#include "api/api_time_status.h"
#include "net/time_sync.h"
#include <time.h>
void api_time_status_add(cJSON *obj)
{
    time_t now = time(NULL);
    struct tm local = {0};
    char text[32] = {0};
    bool valid = localtime_r(&now, &local) != NULL && local.tm_year > 116;
    if (valid) strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &local);
    cJSON_AddStringToObject(obj, "active_timezone", time_sync_get_tz());
    cJSON_AddBoolToObject(obj, "clock_valid", valid);
    cJSON_AddStringToObject(obj, "local_time", text);
}
esp_err_t api_time_status_get(httpd_req_t *req)
{
    cJSON *clock = cJSON_CreateObject();
    if (!clock) return httpd_resp_send_500(req);
    api_time_status_add(clock);
    char *text = cJSON_PrintUnformatted(clock);
    cJSON_Delete(clock);
    if (!text) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text);
    cJSON_free(text);
    return err;
}
