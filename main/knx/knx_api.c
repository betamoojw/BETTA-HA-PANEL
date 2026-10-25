/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "knx.h"
#include "api/http_guard.h"
#include <stdlib.h>
#include <string.h>

static esp_err_t send_json(httpd_req_t* req, cJSON* j) {
    char* text = j ? cJSON_PrintUnformatted(j) : NULL;
    cJSON_Delete(j);
    if (!text) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t e = httpd_resp_sendstr(req, text); free(text); return e;
}
static char* body(httpd_req_t* req, size_t max) {
    if (req->content_len <= 0 || req->content_len > max) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size"); return NULL;
    }
    char* text = malloc(req->content_len + 1);
    if (!text) { httpd_resp_send_500(req); return NULL; }
    size_t used = 0;
    while (used < req->content_len) {
        int n = httpd_req_recv(req, text + used, req->content_len - used);
        if (n <= 0) { free(text); httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout"); return NULL; }
        used += n;
    }
    if (memchr(text, 0, used)) {
        free(text); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "NUL bytes are not allowed"); return NULL;
    }
    text[used] = 0;
    if (strstr(text, "\\u0000")) {
        free(text); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "NUL characters are not allowed"); return NULL;
    }
    return text;
}
static esp_err_t status_get(httpd_req_t* req) { return send_json(req, knx_status()); }
static esp_err_t status_put(httpd_req_t* req) {
    char* text = body(req, 64); if (!text) return ESP_OK;
    cJSON* j = cJSON_ParseWithLengthOpts(text, strlen(text)+1, NULL, true); free(text);
    cJSON* e = cJSON_GetObjectItemCaseSensitive(j, "enabled");
    char error[160] = "Expected only enabled: true/false";
    bool ok = cJSON_IsObject(j) && cJSON_GetArraySize(j) == 1 && cJSON_IsBool(e) &&
              knx_enable(cJSON_IsTrue(e), error, sizeof(error));
    cJSON_Delete(j);
    return ok ? send_json(req, knx_status()) : httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error);
}
static esp_err_t config_get(httpd_req_t* req) {
    char* json = knx_configuration(); if (!json) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"knxConfig.json\"");
    esp_err_t e = httpd_resp_sendstr(req, json); free(json); return e;
}
static esp_err_t config_put(httpd_req_t* req) {
    char* text = body(req, 65535); if (!text) return ESP_OK;
    char error[256]; bool ok = knx_configure(text, error, sizeof(error)); free(text);
    return ok ? send_json(req, knx_status()) : httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error);
}
static esp_err_t objects_get(httpd_req_t* req) {
    char query[192], offset[12] = "0";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // IDs in this commissioning endpoint are selected by index, avoiding URL
        // interpretation of site-defined Unicode IDs. MCP uses exact JSON IDs.
        httpd_query_key_value(query, "offset", offset, sizeof(offset));
    }
    char* end; long n = strtol(offset, &end, 10);
    if (*end || n < 0 || n > 128) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid offset");
    return send_json(req, knx_objects(n, 20));
}
static esp_err_t command_post(httpd_req_t* req) {
    char* text = body(req, 2048); if (!text) return ESP_OK;
    // Commands contain only flat string fields. Reject nested payloads before
    // cJSON recursion can consume the finite HTTP task stack.
    bool quoted = false, escaped = false; unsigned depth = 0;
    for (const char* p = text; *p; ++p) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (*p == '\\') escaped = true;
            else if (*p == '"') quoted = false;
        } else if (*p == '"') quoted = true;
        else if (*p == '[' || (*p == '{' && ++depth > 1)) {
            free(text); return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected a flat command object");
        } else if (*p == '}' && depth) --depth;
    }
    cJSON* j = cJSON_ParseWithLengthOpts(text, strlen(text)+1, NULL, true); free(text);
    const char* id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "object_id"));
    const char* action = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "action"));
    const char* value = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "value"));
    char error[160] = "Expected object_id, action (read/write) and string value for writes";
    bool write = action && !strcmp(action, "write");
    bool ok = cJSON_IsObject(j) && cJSON_GetArraySize(j) == (write ? 3 : 2) &&
        id && action && (!strcmp(action, "read") || write) && (write ? value != NULL : value == NULL) &&
        knx_command(id, NULL, value, !strcmp(action, "write"), error, sizeof(error));
    cJSON_Delete(j);
    if (!ok) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error);
    httpd_resp_set_status(req, "202 Accepted"); httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"queued\":true,\"completion\":\"asynchronous\",\"bus_confirmed\":false}");
}
#define GUARDED(name) static esp_err_t guarded_##name(httpd_req_t* r) { return http_guard_handle(r, name); }
GUARDED(status_get) GUARDED(status_put) GUARDED(config_get) GUARDED(config_put)
GUARDED(objects_get) GUARDED(command_post)
esp_err_t knx_api_register(httpd_handle_t server) {
    const httpd_uri_t routes[] = {
        {.uri="/api/knx", .method=HTTP_GET, .handler=guarded_status_get},
        {.uri="/api/knx", .method=HTTP_PUT, .handler=guarded_status_put},
        {.uri="/api/knx/config", .method=HTTP_GET, .handler=guarded_config_get},
        {.uri="/api/knx/config", .method=HTTP_PUT, .handler=guarded_config_put},
        {.uri="/api/knx/objects", .method=HTTP_GET, .handler=guarded_objects_get},
        {.uri="/api/knx/command", .method=HTTP_POST, .handler=guarded_command_post},
    };
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++) {
        esp_err_t e = httpd_register_uri_handler(server, &routes[i]); if (e != ESP_OK) return e;
    }
    return ESP_OK;
}
