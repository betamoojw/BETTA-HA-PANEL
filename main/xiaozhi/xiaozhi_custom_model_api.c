/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi.h"
#include "xiaozhi_custom_model_store.h"
#include "xiaozhi_custom_resource.h"
#include "api/http_guard.h"
#include "util/flash_gate.h"
#include "esp_littlefs.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static esp_err_t status(httpd_req_t *r)
{
    char json[320]; size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    snprintf(json, sizeof(json), "{\"ready\":%s,\"bytes\":%u,\"sha256\":\"%s\",\"received\":%u,\"free_bytes\":%u}",
        xiaozhi_custom_model_available() ? "true" : "false", XIAOZHI_CUSTOM_BYTES, XIAOZHI_CUSTOM_SHA256,
        (unsigned)xiaozhi_custom_model_received(), (unsigned)(total > used ? total - used : 0));
    httpd_resp_set_type(r, "application/json"); httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, json);
}
static esp_err_t upload(httpd_req_t *r)
{
    char value[24];
    if (r->content_len < 1 || r->content_len > 32768 ||
        httpd_req_get_hdr_value_str(r, "X-Model-Offset", value, sizeof(value)) != ESP_OK ||
        !value[0] || strspn(value, "0123456789") != strlen(value))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid model chunk");
    char *end; unsigned long offset = strtoul(value, &end, 10);
    if (*end || offset > XIAOZHI_CUSTOM_BYTES || (size_t)r->content_len > XIAOZHI_CUSTOM_BYTES - offset)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid model offset");
    if (xiaozhi_custom_model_available()) {
        httpd_resp_set_status(r, "409 Conflict"); return httpd_resp_sendstr(r, "Resource already installed");
    }
    if (!offset) {
        size_t total, used;
        if (esp_littlefs_info("storage", &total, &used) != ESP_OK || total < used ||
            total - used + xiaozhi_custom_model_pending_bytes() < XIAOZHI_CUSTOM_BYTES + 1048576U) {
            httpd_resp_set_status(r, "507 Insufficient Storage");
            return httpd_resp_sendstr(r, "Keep at least 1 MiB free after the model installation");
        }
    }
    unsigned char *buf = heap_caps_malloc(r->content_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return httpd_resp_send_500(r);
    int done = 0;
    int64_t deadline = esp_timer_get_time() + 10000000;
    while (done < r->content_len) {
        if (esp_timer_get_time() >= deadline) { free(buf); return httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, "Model chunk deadline"); }
        int n = httpd_req_recv(r, (char *)buf + done, r->content_len - done);
        if (n <= 0) { free(buf); return httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, "Model chunk interrupted"); }
        done += n;
    }
    if (!flash_gate_enter(2000)) {
        free(buf); httpd_resp_set_status(r, "503 Service Unavailable"); return httpd_resp_sendstr(r, "Storage busy");
    }
    bool ok = xiaozhi_custom_model_append(offset, buf, done);
    flash_gate_leave(); free(buf);
    if (!ok) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Model offset, write or checksum failed");
    return status(r);
}
static esp_err_t guarded_status(httpd_req_t *r) { return http_guard_handle(r, status); }
static esp_err_t guarded_upload(httpd_req_t *r) { return http_guard_handle(r, upload); }
esp_err_t xiaozhi_custom_model_api_register(httpd_handle_t server)
{
    httpd_uri_t get = {.uri="/api/xiaozhi/wake-model", .method=HTTP_GET, .handler=guarded_status};
    httpd_uri_t put = {.uri="/api/xiaozhi/wake-model", .method=HTTP_PUT, .handler=guarded_upload};
    esp_err_t err = httpd_register_uri_handler(server, &get);
    return err == ESP_OK ? httpd_register_uri_handler(server, &put) : err;
}
