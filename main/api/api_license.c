/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/api_license.h"
#include "api/http_guard.h"
#include "settings/license_manager.h"
#include "settings/license_crypto.h"
#include "settings/user_password.h"
#include <stdio.h>

static esp_err_t license_reply(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t license_get(httpd_req_t *req)
{
    license_status_t s;
    license_manager_status(&s);
    const char *states[] = {"missing", "valid", "invalid", "error", "unconfigured"};
    char body[256];
    snprintf(body, sizeof(body), "{\"device_id\":\"%s\",\"key_state\":\"%s\","
        "\"trial_seconds\":%lu,\"status_only\":true,\"trial_per_boot\":true}",
        s.device_id, states[s.key_state], (unsigned long)s.trial_seconds);
    return license_reply(req, "200 OK", body);
}

static esp_err_t license_post(httpd_req_t *req)
{
    if (req->content_len < LICENSE_FILE_LENGTH || req->content_len > LICENSE_FILE_MAX) {
        httpd_resp_set_hdr(req, "Connection", "close");
        return license_reply(req, "400 Bad Request", "{\"error\":\"invalid_license\"}");
    }
    char key[LICENSE_FILE_MAX]; size_t used = 0, length = (size_t)req->content_len;
    while (used < length) {
        int n = httpd_req_recv(req, key + used, length - used);
        if (n <= 0) { user_password_wipe(key, sizeof(key)); return ESP_FAIL; }
        used += (size_t)n;
    }
    esp_err_t err = license_manager_import(key, used);
    user_password_wipe(key, sizeof(key));
    if (err == ESP_ERR_INVALID_ARG)
        return license_reply(req, "400 Bad Request", "{\"error\":\"invalid_license\"}");
    if (err != ESP_OK)
        return license_reply(req, "503 Service Unavailable", "{\"error\":\"license_storage\"}");
    return license_get(req);
}

static esp_err_t license_delete(httpd_req_t *req)
{
    if (license_manager_remove() != ESP_OK)
        return license_reply(req, "503 Service Unavailable", "{\"error\":\"license_storage\"}");
    return license_get(req);
}

static esp_err_t guarded_license_get(httpd_req_t *req) { return http_guard_handle(req, license_get); }
static esp_err_t guarded_license_post(httpd_req_t *req) { return http_guard_handle(req, license_post); }
static esp_err_t guarded_license_delete(httpd_req_t *req) { return http_guard_handle(req, license_delete); }

esp_err_t api_license_register(httpd_handle_t server)
{
    const httpd_uri_t routes[] = {
        {.uri = "/api/license", .method = HTTP_GET, .handler = guarded_license_get},
        {.uri = "/api/license", .method = HTTP_POST, .handler = guarded_license_post},
        {.uri = "/api/license", .method = HTTP_DELETE, .handler = guarded_license_delete},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); ++i) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}
