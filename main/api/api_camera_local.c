/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/api_camera_local.h"

#include "api/http_guard.h"
#include "cJSON.h"
#include "camera/camera_output.h"
#include "camera/camera_source_fetch.h"
#include "camera/camera_store.h"
#include "drivers/ambient_camera.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Atomic bool s_image_busy;
static esp_err_t json_send(httpd_req_t *req, cJSON *root)
{
    char *text = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!text)
        return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text);
    free(text);
    return err;
}
static esp_err_t error(httpd_req_t *req, const char *status, const char *reason)
{
    httpd_resp_set_status(req, status);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", reason);
    return json_send(req, root);
}
static esp_err_t config_get(httpd_req_t *req)
{
    camera_settings_t *s = heap_caps_malloc(sizeof(*s), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s)
        return httpd_resp_send_500(req);
    bool ok = camera_store_get(s);
    cJSON *root = ok ? camera_config_json(s, false) : NULL;
    camera_config_defaults(s);
    cJSON *defaults = camera_config_json(s, false);
    memset(s, 0, sizeof(*s));
    free(s);
    if (!root || !defaults)
    {
        cJSON_Delete(root);
        cJSON_Delete(defaults);
        return httpd_resp_send_500(req);
    }
    cJSON_AddBoolToObject(root, "supported", camera_output_available());
    cJSON_AddBoolToObject(root, "motion_supported", ambient_camera_available());
    cJSON_AddNumberToObject(root, "rotation_degrees", camera_output_rotation());
    cJSON_AddItemToObject(root, "defaults", defaults);
    return json_send(req, root);
}
static esp_err_t config_put(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > CAMERA_CONFIG_MAX_JSON)
        return error(req, "400 Bad Request", "Invalid camera configuration size");
    char *text = heap_caps_malloc(req->content_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!text)
        return httpd_resp_send_500(req);
    int read = 0;
    while (read < req->content_len)
    {
        int n = httpd_req_recv(req, text + read, req->content_len - read);
        if (n <= 0)
        {
            free(text);
            return ESP_FAIL;
        }
        read += n;
    }
    text[read] = 0;
    cJSON *root = cJSON_ParseWithLength(text, read + 1);
    memset(text, 0, read);
    free(text);
    if (!root)
        return error(req, "400 Bad Request", "Invalid camera JSON");
    const cJSON *local = cJSON_GetObjectItemCaseSensitive(root, "local");
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(local, "enabled")) && !camera_output_available())
    {
        cJSON_Delete(root);
        return error(req, "400 Bad Request", "Image output is unavailable on this board");
    }
    esp_err_t err = camera_store_update(root);
    cJSON_Delete(root);
    if (err != ESP_OK)
        return error(req, err == ESP_ERR_INVALID_ARG ? "400 Bad Request" : "503 Service Unavailable", "Camera configuration was not saved");
    camera_output_invalidate();
    return config_get(req);
}
static esp_err_t status_get(httpd_req_t *req)
{
    ambient_camera_reading_t r = { 0 };
    ambient_camera_get(&r);
    camera_motion_status_t m = { 0 };
    ambient_camera_motion_status(&m);
    camera_config_t c = { 0 };
    camera_store_local(&c);
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return httpd_resp_send_500(req);
    cJSON_AddBoolToObject(root, "supported", camera_output_available());
    cJSON_AddBoolToObject(root, "ready", r.ready);
    cJSON_AddBoolToObject(root, "failed", r.failed);
    cJSON_AddBoolToObject(root, "enabled", c.enabled);
    cJSON_AddBoolToObject(root, "stream_enabled", c.enabled && c.stream_enabled);
    int rotation = camera_output_rotation(), width = c.resolution ? 400 : 800, height = c.resolution ? 320 : 640;
    cJSON_AddNumberToObject(root, "rotation_degrees", rotation);
    cJSON_AddNumberToObject(root, "width", rotation % 180 ? height : width);
    cJSON_AddNumberToObject(root, "height", rotation % 180 ? width : height);
    cJSON_AddNumberToObject(root, "frame_sequence", camera_output_sequence());
    int remaining = 0;
    cJSON_AddBoolToObject(root, "test_active", ambient_camera_test_active(&remaining));
    cJSON_AddNumberToObject(root, "test_remaining_s", remaining);
    cJSON_AddBoolToObject(root, "motion_advanced", c.motion.enabled);
    cJSON_AddBoolToObject(root, "valid", c.motion.enabled ? m.valid : r.motion_valid);
    cJSON_AddNumberToObject(root, "level", c.motion.enabled ? m.level : r.motion_level);
    cJSON_AddNumberToObject(root, "changed_pct", c.motion.enabled ? m.changed_pct : r.motion_pct);
    cJSON_AddBoolToObject(root, "active", m.valid && m.active);
    cJSON_AddBoolToObject(root, "ignored_lighting", m.ignored_lighting);
    cJSON_AddNumberToObject(root, "trigger_count", m.trigger_count);
    cJSON_AddNumberToObject(root, "last_trigger_ms", (double) m.last_trigger_ms);
    cJSON *zones = cJSON_AddArrayToObject(root, "zones");
    for (int i = 0; i < c.motion.zone_count; i++)
    {
        cJSON *z = cJSON_CreateObject();
        cJSON_AddItemToArray(zones, z);
        cJSON_AddNumberToObject(z, "level", m.zone_level[i]);
        cJSON_AddNumberToObject(z, "changed_pct", m.zone_changed_pct[i]);
    }
    return json_send(req, root);
}
typedef struct
{
        httpd_req_t *req;
        bool stream;
        char id[CAMERA_SOURCE_ID_LEN];
} image_request_t;
static bool output_enabled(bool stream)
{
    camera_config_t c;
    return camera_store_local(&c) && c.enabled && (!stream || c.stream_enabled);
}
static void image_task(void *arg)
{
    image_request_t *job = arg;
    httpd_req_t *req = job->req;
    bool sent = false;
    unsigned failures = 0;
    do
    {
        if (!strcmp(job->id, "local") && !output_enabled(job->stream))
        {
            if (!sent)
                error(req, "403 Forbidden", "Camera image output is disabled");
            break;
        }
        uint8_t *jpeg = NULL;
        size_t len = 0;
        int rotation = camera_output_rotation();
        esp_err_t err = camera_source_fetch(job->id, &jpeg, &len);
        if (err == ESP_OK && !strcmp(job->id, "local") && rotation != camera_output_rotation())
            err = ESP_ERR_INVALID_STATE; /* Do not label an old-pose frame as the new pose. */
        if (err != ESP_OK)
        {
            free(jpeg);
            if (!job->stream || ++failures >= 3)
            {
                if (!sent)
                    error(req, "503 Service Unavailable", "Camera frame unavailable");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (!strcmp(job->id, "local") && !output_enabled(job->stream))
        {
            free(jpeg);
            if (!sent)
                error(req, "403 Forbidden", "Camera image output is disabled");
            break;
        }
        failures = 0;
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        if (!job->stream)
        {
            char angle[12];
            snprintf(angle, sizeof(angle), "%d", !strcmp(job->id, "local") ? rotation : 0);
            httpd_resp_set_hdr(req, "X-Camera-Rotation", angle);
            httpd_resp_set_type(req, "image/jpeg");
            httpd_resp_send(req, (const char *) jpeg, len);
            free(jpeg);
            break;
        }
        if (!sent)
            httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=betta-frame");
        char header[112];
        int n = snprintf(header, sizeof(header), "--betta-frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned) len);
        err = httpd_resp_send_chunk(req, header, n);
        if (err == ESP_OK)
            err = httpd_resp_send_chunk(req, (const char *) jpeg, len);
        if (err == ESP_OK)
            err = httpd_resp_send_chunk(req, "\r\n", 2);
        free(jpeg);
        sent = true;
        if (err != ESP_OK)
            break;
        vTaskDelay(pdMS_TO_TICKS(500));
    } while (job->stream);
    if (job->stream && sent)
        httpd_resp_send_chunk(req, NULL, 0);
    httpd_req_async_handler_complete(req);
    free(job);
    atomic_store(&s_image_busy, false);
    vTaskDelete(NULL);
}
static esp_err_t image_get(httpd_req_t *req)
{
    bool stream = strncmp(req->uri, "/api/camera/stream", 18) == 0;
    char id[CAMERA_SOURCE_ID_LEN] = "local", query[80];
    if (httpd_req_get_url_query_len(req) > 0)
    {
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "id", id, sizeof(id)) != ESP_OK || (strcmp(id, "local") && !camera_source_id_valid(id)))
            return error(req, "400 Bad Request", "Invalid camera source");
    }
    if (stream && strcmp(id, "local"))
        return error(req, "400 Bad Request", "This endpoint streams the built-in camera only");
    if (!strcmp(id, "local") && (!camera_output_available() || !output_enabled(stream)))
        return error(req, "403 Forbidden", "Camera image output is disabled");
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_image_busy, &expected, true))
        return error(req, "503 Service Unavailable", "Camera image request already active");
    image_request_t *job = calloc(1, sizeof(*job));
    if (!job)
    {
        atomic_store(&s_image_busy, false);
        return httpd_resp_send_500(req);
    }
    job->stream = stream;
    snprintf(job->id, sizeof(job->id), "%s", id);
    if (httpd_req_async_handler_begin(req, &job->req) != ESP_OK)
    {
        free(job);
        atomic_store(&s_image_busy, false);
        return httpd_resp_send_500(req);
    }
    if (xTaskCreate(image_task, "camera_http", 6144, job, 2, NULL) != pdPASS)
    {
        error(job->req, "503 Service Unavailable", "Camera worker unavailable");
        httpd_req_async_handler_complete(job->req);
        free(job);
        atomic_store(&s_image_busy, false);
    }
    return ESP_OK;
}
static esp_err_t guarded_config_get(httpd_req_t *r)
{
    return http_guard_handle(r, config_get);
}
static esp_err_t guarded_config_put(httpd_req_t *r)
{
    return http_guard_handle(r, config_put);
}
static esp_err_t guarded_status_get(httpd_req_t *r)
{
    return http_guard_handle(r, status_get);
}
static esp_err_t guarded_image_get(httpd_req_t *r)
{
    return http_guard_handle(r, image_get);
}
esp_err_t api_camera_local_register(httpd_handle_t server)
{
    const httpd_uri_t routes[] = {
        { .uri = "/api/camera/config", .method = HTTP_GET, .handler = guarded_config_get },
        { .uri = "/api/camera/config", .method = HTTP_PUT, .handler = guarded_config_put },
        { .uri = "/api/cameras", .method = HTTP_GET, .handler = guarded_config_get },
        { .uri = "/api/cameras", .method = HTTP_PUT, .handler = guarded_config_put },
        { .uri = "/api/camera/status", .method = HTTP_GET, .handler = guarded_status_get },
        { .uri = "/api/camera/motion", .method = HTTP_GET, .handler = guarded_status_get },
        { .uri = "/api/camera/snapshot", .method = HTTP_GET, .handler = guarded_image_get },
        { .uri = "/api/camera/stream", .method = HTTP_GET, .handler = guarded_image_get },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
    {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK)
            return err;
    }
    return ESP_OK;
}
