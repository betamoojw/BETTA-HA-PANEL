/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_http_client.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    esp_http_client_handle_t client;
    const char *url, *token, *host, *domain, *search;
    uint16_t offset, page_size;
    esp_err_t (*open)(esp_http_client_handle_t, int, const char *);
    void (*auth_status)(int, bool);
} ha_discovery_request_t;

/* Uses the HA task's existing HTTP client and budget. Caller owns the returned JSON. */
esp_err_t ha_discovery_request_page(const ha_discovery_request_t *request, cJSON **out);
/* Large cache/work buffers may never fall back to internal memory on S3. */
void *ha_discovery_calloc(size_t count, size_t size);
