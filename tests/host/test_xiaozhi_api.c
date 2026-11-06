/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xiaozhi/xiaozhi.h"
#include "api/http_guard.h"
#include "cJSON.h"

static httpd_uri_t routes[3];
static unsigned route_count, calls, guard_calls;
static int last_value, response_code;
static char last_action[16], response[4096];
static xiaozhi_status_t snapshot;

void xiaozhi_status(xiaozhi_status_t *out) { *out = snapshot; }
esp_err_t xiaozhi_command(const char *action, int value)
{
    ++calls;
    snprintf(last_action, sizeof(last_action), "%s", action);
    last_value = value;
    return ESP_OK;
}
esp_err_t xiaozhi_configure_wake(int word, const char *phrase)
{ (void)word; (void)phrase; assert(!"Unexpected wake command"); return ESP_FAIL; }
esp_err_t xiaozhi_set_emoji_style(int value)
{ (void)value; assert(!"Unexpected emoji command"); return ESP_FAIL; }
esp_err_t http_guard_handle(httpd_req_t *req, http_guard_handler_t next)
{ ++guard_calls; return next(req); }
int httpd_req_recv(httpd_req_t *req, char *out, size_t len)
{
    /* Real request parsing must tolerate fragmented HTTP reads. */
    if (len > 7) len = 7;
    memcpy(out, req->body + req->offset, len);
    req->offset += len;
    return (int)len;
}
esp_err_t httpd_resp_send_500(httpd_req_t *req)
{ (void)req; response_code = 500; return ESP_FAIL; }
esp_err_t httpd_resp_send_err(httpd_req_t *req, int code, const char *message)
{ (void)req; (void)message; response_code = code; return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *body)
{ (void)req; assert(strlen(body) < sizeof(response)); strcpy(response, body); return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type)
{ (void)req; (void)type; return ESP_OK; }
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status)
{ (void)req; response_code = atoi(status); return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *name, const char *value)
{ (void)req; assert(!strcmp(name, "Cache-Control") && !strcmp(value, "no-store")); return ESP_OK; }
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *route)
{ (void)handle; assert(route_count < 3); routes[route_count++] = *route; return ESP_OK; }

static void request(const char *body, int code, unsigned forwarded)
{
    httpd_req_t req = {.content_len = strlen(body), .body = body};
    calls = 0;
    response_code = 200;
    assert(routes[1].handler(&req) == ESP_OK);
    assert(response_code == code && calls == forwarded);
}

int main(void)
{
    assert(xiaozhi_api_register(NULL) == ESP_OK && route_count == 3);
    assert(routes[0].method == HTTP_GET && routes[1].method == HTTP_POST);
    assert(!strcmp(routes[1].uri, "/api/xiaozhi"));
    request("{\"action\":\"duplex\",\"value\":1}", 202, 1);
    assert(!strcmp(last_action, "duplex") && last_value == 1);
    request("{\"action\":\"duplex\",\"value\":0}", 202, 1);
    assert(last_value == 0);
    const char *invalid[] = {
        "{\"action\":\"duplex\"}", "{\"action\":\"duplex\",\"value\":null}",
        "{\"action\":\"duplex\",\"value\":true}", "{\"action\":\"duplex\",\"value\":false}",
        "{\"action\":\"duplex\",\"value\":\"1\"}", "{\"action\":\"duplex\",\"value\":0.5}",
        "{\"action\":\"duplex\",\"value\":1.1}", "{\"action\":\"duplex\",\"value\":1e50}",
        "{\"action\":\"duplex\",\"value\":[]}", "{\"action\":\"duplex\",\"value\":{}}",
        "{\"action\":\"duplex\",\"value\":", "null", ""
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        request(invalid[i], 400, 0);
    snapshot = (xiaozhi_status_t){.supported = true, .enabled = true,
        .duplex_enabled = true, .duplex_active = true, .speaking = true, .listening = true,
        .audio_tx_frames = 100, .audio_rx_frames = 50, .audio_tx_during_playback = 35,
        .barge_ins = 2, .aec_ref_peak = 1234, .aec_frames = 100,
        .normal_closes = 4, .connection_errors = 1, .connection_close_code = 1000,
        .connection_error_type = 1, .connection_tls_error = -30848,
        .connection_socket_errno = 104, .connection_http_status = 401};
    strcpy(snapshot.connection_end, "transport_error");
    strcpy(snapshot.state, "speaking_listening");
    httpd_req_t req = {0};
    assert(routes[0].handler(&req) == ESP_OK);
    cJSON *j = cJSON_Parse(response);
    assert(j);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "listening")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "speaking")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "duplex_active")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "duplex_enabled")));
    assert(cJSON_GetObjectItem(j, "audio_tx_during_playback")->valueint == 35);
    assert(cJSON_GetObjectItem(j, "aec_ref_peak")->valueint == 1234);
    assert(cJSON_GetObjectItem(j, "barge_ins")->valueint == 2);
    assert(cJSON_GetObjectItem(j, "normal_closes")->valueint == 4);
    assert(cJSON_GetObjectItem(j, "connection_errors")->valueint == 1);
    assert(cJSON_GetObjectItem(j, "connection_close_code")->valueint == 1000);
    assert(cJSON_GetObjectItem(j, "connection_error_type")->valueint == 1);
    assert(cJSON_GetObjectItem(j, "connection_tls_error")->valueint == -30848);
    assert(cJSON_GetObjectItem(j, "connection_socket_errno")->valueint == 104);
    assert(cJSON_GetObjectItem(j, "connection_http_status")->valueint == 401);
    assert(!strcmp(cJSON_GetObjectItem(j, "connection_end")->valuestring, "transport_error"));
    assert(!strcmp(cJSON_GetObjectItem(j, "state")->valuestring, "speaking_listening"));
    cJSON_Delete(j);
    assert(guard_calls == 16);
    puts("Xiaozhi API: guarded duplex commands, malformed values and simultaneous audio status passed");
    return 0;
}
