/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_http_server.h"

/* HTTPD WebSocket writes must return the full byte count or a fatal error. */
int remote_display_send(httpd_handle_t server, int fd, const char *data, size_t len, int flags);
