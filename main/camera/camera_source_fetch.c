/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_source_fetch.h"

#include "camera/camera_output.h"
#include "camera/camera_store.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "ha/ha_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JPEG (1024U * 1024U)
typedef struct
{
        camera_source_t source;
        ha_client_http_ctx_t ha;
        char url[640], auth[600];
} fetch_context_t;
int camera_source_refresh_ms(const char *id)
{
    if (id && !strcmp(id, "local"))
        return 500;
    camera_source_t source;
    return camera_store_source(id, &source) ? source.refresh_ms : 10000;
}
esp_err_t camera_source_fetch(const char *id, uint8_t **jpeg, size_t *length)
{
    if (!id || !jpeg || !length)
        return ESP_ERR_INVALID_ARG;
    *jpeg = NULL;
    *length = 0;
    if (!strcmp(id, "local"))
        return camera_output_snapshot(jpeg, length);
    if (ha_client_heavy_gate_is_busy() ||
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 20U * 1024U ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 12U * 1024U)
        return ESP_ERR_NO_MEM;
    fetch_context_t *c = heap_caps_calloc(1, sizeof(*c), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!c)
        return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_ERR_INVALID_STATE;
    esp_http_client_handle_t client = NULL;
    uint8_t *data = NULL;
    uint32_t generation = camera_store_generation();
    if (!camera_store_source(id, &c->source) || !c->source.enabled)
        goto done;
    bool ha = !strcmp(c->source.source, "ha");
    if (ha)
    {
        if (!ha_client_is_connected() || !ha_client_get_http_context(&c->ha) || !c->ha.bearer_token[0])
            goto done;
        int n = snprintf(c->url, sizeof(c->url), "%s/api/camera_proxy/%s", c->ha.base_url, c->source.entity_id);
        if (n < 0 || (size_t) n >= sizeof(c->url))
        {
            err = ESP_ERR_INVALID_SIZE;
            goto done;
        }
        snprintf(c->auth, sizeof(c->auth), "Bearer %s", c->ha.bearer_token);
    }
    else
        snprintf(c->url, sizeof(c->url), "%s", c->source.snapshot_url);
    esp_http_client_config_t cfg = {
        .url = c->url,
        .timeout_ms = 3000,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .disable_auto_redirect = true,
        .keep_alive_enable = false,
        .method = HTTP_METHOD_GET,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };
    if (ha && c->ha.cert_common_name[0])
        cfg.common_name = c->ha.cert_common_name;
    if (!ha && c->source.username[0])
    {
        cfg.username = c->source.username;
        cfg.password = c->source.password;
        cfg.auth_type = HTTP_AUTH_TYPE_BASIC;
    }
    client = esp_http_client_init(&cfg);
    if (!client)
    {
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    if (ha)
    {
        esp_http_client_set_header(client, "Authorization", c->auth);
        if (c->ha.host_header[0])
            esp_http_client_set_header(client, "Host", c->ha.host_header);
    }
    esp_http_client_set_header(client, "Accept", "image/jpeg");
    esp_http_client_set_header(client, "Connection", "close");
    int64_t deadline = esp_timer_get_time() + 10000000;
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
        goto done;
    int64_t expected = esp_http_client_fetch_headers(client);
    if (expected < 0 || expected > MAX_JPEG || esp_http_client_get_status_code(client) != 200)
    {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    size_t cap = expected > 0 ? (size_t) expected : MAX_JPEG;
    data = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data)
    {
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    size_t used = 0;
    while (used < cap && esp_timer_get_time() < deadline)
    {
        int n = esp_http_client_read(client, (char *) data + used, (int) (cap - used > 4096 ? 4096 : cap - used));
        if (n < 0)
        {
            err = ESP_FAIL;
            goto done;
        }
        used += (size_t) n;
        if (esp_http_client_is_complete_data_received(client))
            break;
        if (n == 0)
        {
            err = ESP_ERR_TIMEOUT;
            goto done;
        }
        if (generation != camera_store_generation())
        {
            err = ESP_ERR_INVALID_STATE;
            goto done;
        }
    }
    if (!esp_http_client_is_complete_data_received(client) || used < 4 || (expected > 0 && used != (size_t) expected) || data[0] != 0xff ||
        data[1] != 0xd8 || data[used - 2] != 0xff || data[used - 1] != 0xd9 || generation != camera_store_generation())
    {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    *jpeg = data;
    *length = used;
    data = NULL;
    err = ESP_OK;
done:
    if (client)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    free(data);
    memset(c, 0, sizeof(*c));
    free(c);
    return err;
}
