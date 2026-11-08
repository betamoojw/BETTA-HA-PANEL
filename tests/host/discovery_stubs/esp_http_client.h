/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef void *esp_http_client_handle_t;
typedef struct {
    const char *url;
    int timeout_ms;
    bool keep_alive_enable;
    int buffer_size, buffer_size_tx;
    const char *common_name;
    esp_err_t (*crt_bundle_attach)(void *);
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
#define HTTP_METHOD_POST 1
#define ESP_ERR_HTTP_EAGAIN 0x7007
esp_err_t esp_http_client_set_url(void *, const char *);
esp_err_t esp_http_client_set_method(void *, int);
esp_err_t esp_http_client_set_header(void *, const char *, const char *);
int esp_http_client_write(void *, const char *, int);
int64_t esp_http_client_fetch_headers(void *);
int esp_http_client_get_status_code(void *);
int esp_http_client_read(void *, char *, int);
bool esp_http_client_is_complete_data_received(void *);
esp_err_t esp_http_client_close(void *);
