/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "xiaozhi.h"
#include "xiaozhi_emoji.h"
#include "api/http_guard.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static esp_err_t get_status(httpd_req_t *req)
{
    xiaozhi_status_t s; xiaozhi_status(&s);
    cJSON *j = cJSON_CreateObject();
    if (!j) return httpd_resp_send_500(req);
    cJSON_AddBoolToObject(j, "supported", s.supported);
    cJSON_AddBoolToObject(j, "enabled", s.enabled);
    cJSON_AddBoolToObject(j, "paired", s.paired);
    cJSON_AddBoolToObject(j, "listening", s.listening);
    cJSON_AddNumberToObject(j, "volume", s.volume);
    cJSON_AddBoolToObject(j, "wake_enabled", s.wake_enabled);
    cJSON_AddBoolToObject(j, "wake_ready", s.wake_ready);
    cJSON_AddBoolToObject(j, "wake_running", s.wake_running);
    cJSON_AddNumberToObject(j, "wake_frames", s.wake_frames);
    cJSON_AddNumberToObject(j, "wake_hits", s.wake_hits);
    cJSON_AddNumberToObject(j, "wake_mic_peak", s.wake_mic_peak);
    cJSON_AddNumberToObject(j, "wake_ref_peak", s.wake_ref_peak);
    cJSON_AddNumberToObject(j, "wake_detect_us", s.wake_detect_us);
    cJSON_AddNumberToObject(j, "dialog_timeout", s.dialog_timeout);
    cJSON_AddNumberToObject(j, "emoji_style", s.emoji_style);
    cJSON_AddStringToObject(j, "recognized", s.recognized);
    cJSON_AddStringToObject(j, "reply", s.reply);
    cJSON_AddStringToObject(j, "emotion", s.emotion);
    cJSON_AddStringToObject(j, "state", s.state);
    cJSON_AddStringToObject(j, "code", s.code);
    cJSON_AddStringToObject(j, "text", s.text);
    char *body = cJSON_PrintUnformatted(j); cJSON_Delete(j);
    if (!body) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, body); free(body); return err;
}

static esp_err_t command(httpd_req_t *req)
{
    char body[128];
    if (req->content_len <= 0 || req->content_len >= sizeof(body)) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size");
    int used = 0;
    while (used < req->content_len) {
        int n = httpd_req_recv(req, body + used, req->content_len - used);
        if (n <= 0) return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        used += n;
    }
    body[used] = 0;
    cJSON *j = cJSON_Parse(body), *a = cJSON_GetObjectItemCaseSensitive(j, "action");
    cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "value");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(a) && !strcmp(a->valuestring, "emoji") && cJSON_IsNumber(v) && v->valuedouble == v->valueint)
        err = xiaozhi_set_emoji_style(v->valueint);
    else if (cJSON_IsString(a) && ((strcmp(a->valuestring, "volume") && strcmp(a->valuestring, "wake") && strcmp(a->valuestring, "timeout")) ||
        (cJSON_IsNumber(v) && v->valuedouble == v->valueint)))
        err = xiaozhi_command(a->valuestring, cJSON_IsNumber(v) ? v->valueint : 0);
    cJSON_Delete(j);
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Command unavailable or invalid");
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"accepted\":true}");
}

static esp_err_t guarded_get(httpd_req_t *r) { return http_guard_handle(r, get_status); }
static esp_err_t emoji_asset(httpd_req_t *req)
{
#if CONFIG_APP_PANEL_VARIANT_10INCH_1280
    char query[80], emotion[24] = "neutral";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
        httpd_query_key_value(query, "emotion", emotion, sizeof(emotion));
    const lv_image_dsc_t *asset = xiaozhi_otto_asset(emotion);
    httpd_resp_set_type(req, "image/gif");
    httpd_resp_set_hdr(req, "Cache-Control", "private, max-age=3600");
    return httpd_resp_send(req, (const char *)asset->data, asset->data_size);
#else
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unsupported");
#endif
}
static esp_err_t guarded_emoji(httpd_req_t *r) { return http_guard_handle(r, emoji_asset); }
static esp_err_t guarded_post(httpd_req_t *r) { return http_guard_handle(r, command); }
esp_err_t xiaozhi_api_register(httpd_handle_t server)
{
    httpd_uri_t get = {.uri = "/api/xiaozhi", .method = HTTP_GET, .handler = guarded_get};
    httpd_uri_t post = {.uri = "/api/xiaozhi", .method = HTTP_POST, .handler = guarded_post};
    httpd_uri_t emoji = {.uri = "/api/xiaozhi/emoji", .method = HTTP_GET, .handler = guarded_emoji};
    esp_err_t err = httpd_register_uri_handler(server, &get);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &post);
    return err == ESP_OK ? httpd_register_uri_handler(server, &emoji) : err;
}
