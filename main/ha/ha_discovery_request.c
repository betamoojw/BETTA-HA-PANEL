/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "ha/ha_discovery_request.h"
#include "ha/ha_entity_source.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *ha_discovery_calloc(size_t count, size_t size)
{
    void *ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if !defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    if (ptr == NULL) ptr = calloc(count, size);
#endif
    return ptr;
}

/* Samples bracket synchronous stages; they are not an allocator peak trace. */
typedef struct {
    size_t free_min, block_min;
} memory_samples_t;

static void sample_memory(memory_samples_t *samples)
{
    size_t available = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (available < samples->free_min) samples->free_min = available;
    if (block < samples->block_min) samples->block_min = block;
}

static bool request_memory_ok(bool connected)
{
#if defined(CONFIG_APP_PANEL_VARIANT_S3_480)
    size_t reserve = connected ? APP_HA_DISCOVERY_CONNECTED_INTERNAL_FREE_BYTES
                              : APP_HA_DISCOVERY_PREPARED_INTERNAL_FREE_BYTES;
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= reserve &&
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >=
            APP_HA_DISCOVERY_REQUEST_INTERNAL_BLOCK_BYTES;
#else
    (void)connected;
    return true;
#endif
}

esp_err_t ha_discovery_request_page(const ha_discovery_request_t *r, cJSON **out)
{
    *out = NULL;
    esp_err_t err = ESP_ERR_NO_MEM;
    memory_samples_t samples = {SIZE_MAX, SIZE_MAX};
    sample_memory(&samples);
    size_t internal_before = samples.free_min;
    int body_len = 0;
    size_t used = 0;
    const char *stage = "prepare";
    char *body = NULL, *payload = NULL, *query = NULL, *literal = NULL;
    cJSON *node = cJSON_CreateString(r->search);
    if (node != NULL) literal = cJSON_PrintUnformatted(node);
    cJSON_Delete(node);
    if (literal == NULL) goto done;
    query = ha_entity_source_template(r->domain, literal, r->offset, r->page_size);
    cJSON_free(literal);
    if (query == NULL) goto done;
    node = cJSON_CreateObject();
    if (node != NULL && cJSON_AddStringToObject(node, "template", query) != NULL)
        body = cJSON_PrintUnformatted(node);
    cJSON_Delete(node);
    heap_caps_free(query);
    if (body == NULL) goto done;

    char auth[640];
    snprintf(auth, sizeof(auth), "Bearer %s", r->token);
    if ((err = esp_http_client_set_url(r->client, r->url)) != ESP_OK ||
        (err = esp_http_client_set_method(r->client, HTTP_METHOD_POST)) != ESP_OK ||
        (err = esp_http_client_set_header(r->client, "Authorization", auth)) != ESP_OK ||
        (err = esp_http_client_set_header(r->client, "Accept", "text/plain")) != ESP_OK ||
        (err = esp_http_client_set_header(r->client, "Content-Type", "application/json")) != ESP_OK ||
        (r->host[0] && (err = esp_http_client_set_header(r->client, "Host", r->host)) != ESP_OK)) goto done;
    body_len = (int)strlen(body);
    sample_memory(&samples);
    if (!request_memory_ok(false)) { err = ESP_ERR_NO_MEM; goto done; }
    stage = "open";
    int64_t deadline_us = esp_timer_get_time() + 5000000;
    err = r->open(r->client, body_len, "light-discovery");
    sample_memory(&samples);
    if (err != ESP_OK) goto done;
    if (esp_timer_get_time() >= deadline_us) { err = ESP_ERR_TIMEOUT; goto done; }
    if (!request_memory_ok(true)) { err = ESP_ERR_NO_MEM; goto done; }
    stage = "write";
    if (esp_http_client_write(r->client, body, body_len) != body_len) {
        err = ESP_FAIL;
        goto done;
    }
    cJSON_free(body);
    body = NULL;
    if (esp_timer_get_time() >= deadline_us) { err = ESP_ERR_TIMEOUT; goto done; }
    stage = "headers";
    int64_t length = esp_http_client_fetch_headers(r->client);
    sample_memory(&samples);
    if (esp_timer_get_time() >= deadline_us) { err = ESP_ERR_TIMEOUT; goto done; }
    /* IDF returns zero for chunked/unknown length; negative values are errors,
     * even if a partial response has already supplied a 200 status line. */
    if (length < 0) {
        err = length == -ESP_ERR_HTTP_EAGAIN ? ESP_ERR_TIMEOUT : ESP_FAIL;
        goto done;
    }
    int status = esp_http_client_get_status_code(r->client);
    r->auth_status(status, true);
    if (status != 200) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    if (length >= APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES) {
        err = ESP_ERR_INVALID_SIZE;
        goto done;
    }
    size_t cap = length > 0 ? (size_t)length + 1U : APP_HA_DISCOVERY_PAYLOAD_MAX_BYTES;
    if (!request_memory_ok(true)) { err = ESP_ERR_NO_MEM; goto done; }
    payload = ha_discovery_calloc(cap, 1);
    if (payload == NULL) {
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    stage = "read";
    while (used < cap - 1U) {
        if (esp_timer_get_time() >= deadline_us) { err = ESP_ERR_TIMEOUT; goto done; }
        int n = esp_http_client_read(r->client, payload + used, (int)(cap - 1U - used));
        sample_memory(&samples);
        if (n < 0) { err = ESP_FAIL; goto done; }
        used += (size_t)n;
        if (esp_timer_get_time() >= deadline_us) { err = ESP_ERR_TIMEOUT; goto done; }
        if (n == 0) break;
        if (!request_memory_ok(true)) { err = ESP_ERR_NO_MEM; goto done; }
    }
    if (!esp_http_client_is_complete_data_received(r->client)) {
        err = used == cap - 1U ? ESP_ERR_INVALID_SIZE : ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    payload[used] = '\0';
    stage = "parse";
    if (!request_memory_ok(true)) { err = ESP_ERR_NO_MEM; goto done; }
    *out = cJSON_Parse(payload);
    /* Retain the parse boundary before either failure path releases its nodes. */
    sample_memory(&samples);
    if (!cJSON_IsObject(*out) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(*out, "items"))) {
        cJSON_Delete(*out);
        *out = NULL;
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    /* cJSON is PSRAM-first, but its allocator can fall back to internal RAM. */
    if (!request_memory_ok(true)) {
        cJSON_Delete(*out);
        *out = NULL;
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    err = ESP_OK;
done:
    sample_memory(&samples);
    esp_http_client_close(r->client);
    cJSON_free(body);
    heap_caps_free(payload);
    ESP_LOGI("ha_discovery", "Page memory: limit=%u body=%u response=%u stage=%s result=%d "
        "internal_before=%u internal_after=%u sampled_free_min=%u sampled_block_min=%u",
        (unsigned)r->page_size, (unsigned)body_len, (unsigned)used, stage, (int)err,
        (unsigned)internal_before, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)samples.free_min, (unsigned)samples.block_min);
    return err;
}
