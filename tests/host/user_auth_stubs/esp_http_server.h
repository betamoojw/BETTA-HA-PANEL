/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    const char *uri;
    int method, content_len, consumed;
    const char *body, *cookie, *authorization, *origin, *marker, *content_type;
    char status[64], response[8192], set_cookie[256];
} httpd_req_t;
typedef void *httpd_handle_t;
enum { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE };
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
int httpd_req_to_sockfd(httpd_req_t *req);
size_t httpd_req_get_hdr_value_len(httpd_req_t *req, const char *name);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req, const char *name, char *out, size_t n);
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status);
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type);
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *name, const char *value);
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *text);
esp_err_t httpd_resp_send_500(httpd_req_t *req);
esp_err_t httpd_resp_send_404(httpd_req_t *req);
int httpd_req_recv(httpd_req_t *req, char *out, size_t n);
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *uri);
