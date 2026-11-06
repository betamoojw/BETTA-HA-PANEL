/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef void *httpd_handle_t;
typedef struct { size_t content_len; const char *body; size_t offset; } httpd_req_t;
typedef struct {
    const char *uri;
    int method;
    esp_err_t (*handler)(httpd_req_t *);
} httpd_uri_t;
enum { HTTP_GET, HTTP_POST, HTTPD_400_BAD_REQUEST = 400, HTTPD_404_NOT_FOUND = 404,
       HTTPD_408_REQ_TIMEOUT = 408 };
int httpd_req_recv(httpd_req_t *, char *, size_t);
esp_err_t httpd_resp_send_500(httpd_req_t *);
esp_err_t httpd_resp_send_err(httpd_req_t *, int, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
