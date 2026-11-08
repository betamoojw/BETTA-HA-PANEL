/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
typedef void *httpd_handle_t;
typedef struct { const char *query; bool query_error; } httpd_req_t;
enum { HTTPD_400_BAD_REQUEST = 400 };
size_t httpd_req_get_url_query_len(httpd_req_t *);
esp_err_t httpd_req_get_url_query_str(httpd_req_t *, char *, size_t);
esp_err_t httpd_query_key_value(const char *, const char *, char *, size_t);
esp_err_t httpd_resp_send_500(httpd_req_t *);
esp_err_t httpd_resp_send_err(httpd_req_t *, int, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
