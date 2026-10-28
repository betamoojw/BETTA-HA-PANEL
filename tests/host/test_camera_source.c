/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_output.h"
#include "camera/camera_source_fetch.h"
#include "camera/camera_store.h"
#include "camera_test_platform.h"
#include "ha/ha_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, opens, ha_contexts, local_calls;
static int code = 200, expected = 8, step = 8;
static size_t position;
static bool ha_ready = true, alter_generation, stall;
static bool heavy;
static uint32_t generation;
static int64_t clock_us;
static camera_source_t source;
static const char jpeg_data[] = "\xff\xd8"
                                "ABCD"
                                "\xff\xd9";
static const char *payload = jpeg_data;
static char auth[600], target[640], username[64], password[96];
#define CHECK(c)                                                          \
    do                                                                    \
    {                                                                     \
        checks++;                                                         \
        if (!(c))                                                         \
        {                                                                 \
            fprintf(stderr, "Camera source line %d: %s\n", __LINE__, #c); \
            exit(1);                                                      \
        }                                                                 \
    } while (0)
void *heap_caps_malloc(size_t n, unsigned caps)
{
    (void) caps;
    return malloc(n);
}
void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
{
    (void) caps;
    return calloc(n, size);
}
int64_t esp_timer_get_time(void)
{
    return clock_us;
}
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 64000;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 32000;}
bool ha_client_is_connected(void){return ha_ready;}
bool ha_client_heavy_gate_is_busy(void){return heavy;}
uint32_t camera_store_generation(void)
{
    return generation;
}
bool camera_store_source(const char *id, camera_source_t *out)
{
    if (strcmp(id, "door"))
        return false;
    *out = source;
    return true;
}
esp_err_t camera_output_snapshot(uint8_t **jpeg, size_t *length)
{
    local_calls++;
    *jpeg = NULL;
    *length = 0;
    return ESP_ERR_INVALID_STATE;
}
bool ha_client_get_http_context(ha_client_http_ctx_t *ctx)
{
    ha_contexts++;
    memset(ctx, 0, sizeof(*ctx));
    strcpy(ctx->base_url, "http://ha.test:8123");
    strcpy(ctx->bearer_token, "synthetic-ha-secret");
    return ha_ready;
}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg)
{
    CHECK(cfg->disable_auto_redirect && cfg->timeout_ms <= 3000 && cfg->method == HTTP_METHOD_GET);
    strcpy(target, cfg->url);
    snprintf(username, sizeof(username), "%s", cfg->username ? cfg->username : "");
    snprintf(password, sizeof(password), "%s", cfg->password ? cfg->password : "");
    auth[0] = 0;
    position = 0;
    return (void *) 1;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h, const char *key, const char *value)
{
    (void) h;
    if (!strcmp(key, "Authorization"))
        snprintf(auth, sizeof(auth), "%s", value);
    return ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t h, int size)
{
    (void) h;
    (void) size;
    opens++;
    return ESP_OK;
}
int esp_http_client_fetch_headers(esp_http_client_handle_t h)
{
    (void) h;
    return expected;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h)
{
    (void) h;
    return code;
}
int esp_http_client_read(esp_http_client_handle_t h, char *out, int length)
{
    (void) h;
    clock_us += 1000;
    if (alter_generation)
        generation++;
    if (stall)
        return 0;
    size_t n = 8 - position;
    if (n > (size_t) step)
        n = (size_t) step;
    if (n > (size_t) length)
        n = (size_t) length;
    memcpy(out, payload + position, n);
    position += n;
    return (int) n;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h)
{
    (void) h;
    return position == 8;
}
void esp_http_client_close(esp_http_client_handle_t h)
{
    (void) h;
}
void esp_http_client_cleanup(esp_http_client_handle_t h)
{
    (void) h;
}
static esp_err_t fetch(const char *id)
{
    uint8_t *data = (void *) 1;
    size_t length = 999;
    esp_err_t err = camera_source_fetch(id, &data, &length);
    if (err == ESP_OK)
        CHECK(data && length == 8 && !memcmp(data, jpeg_data, 8));
    else
        CHECK(!data && !length);
    free(data);
    return err;
}
int main(void)
{
    strcpy(source.id, "door");
    strcpy(source.source, "http");
    strcpy(source.snapshot_url, "http://camera.test/snapshot");
    strcpy(source.username, "viewer");
    strcpy(source.password, "synthetic-camera-secret");
    source.enabled = true;
    source.refresh_ms = 2000;
    CHECK(fetch("door") == ESP_OK);
    CHECK(!strcmp(target, source.snapshot_url) && !auth[0] && ha_contexts == 0);
    CHECK(!strcmp(username, "viewer") && !strcmp(password, "synthetic-camera-secret"));
    CHECK(camera_source_refresh_ms("door") == 2000 && camera_source_refresh_ms("local") == 500);
    expected = 0;
    step = 2;
    CHECK(fetch("door") == ESP_OK);
    expected = 8;
    for (int i = 0; i < 3; i++)
    {
        code = (int[]) { 302, 401, 500 }[i];
        unsigned before = opens;
        CHECK(fetch("door") != ESP_OK);
        CHECK(opens == before + 1);
    }
    code = 200;
    expected = 1024 * 1024 + 1;
    CHECK(fetch("door") != ESP_OK);
    expected = 8;
    payload = "not JPEG";
    CHECK(fetch("door") != ESP_OK);
    payload = jpeg_data;
    expected = 9;
    CHECK(fetch("door") != ESP_OK);
    expected = 8;
    alter_generation = true;
    CHECK(fetch("door") != ESP_OK);
    alter_generation = false;
    stall = true;
    CHECK(fetch("door") == ESP_ERR_TIMEOUT);
    stall = false;
    source.enabled = false;
    unsigned before = opens;
    CHECK(fetch("door") != ESP_OK && opens == before);
    source.enabled = true;
    CHECK(fetch("missing") != ESP_OK && opens == before);
    strcpy(source.source, "ha");
    strcpy(source.entity_id, "camera.entry");
    CHECK(fetch("door") == ESP_OK);
    CHECK(!strcmp(target, "http://ha.test:8123/api/camera_proxy/camera.entry"));
    CHECK(!strcmp(auth, "Bearer synthetic-ha-secret") && !username[0] && !password[0]);
    ha_ready = false;
    before = opens;
    CHECK(fetch("door") != ESP_OK && opens == before);
    CHECK(fetch("local") == ESP_ERR_INVALID_STATE && local_calls == 1);
    printf("Camera source: %u checks passed (credentials, redirects, JPEG bounds, cancellation, opt-in)\n", checks);
    return 0;
}
