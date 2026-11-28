/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief HTTPD send callback for Live Display.
 */
#pragma once
#include "esp_http_server.h"

/** @brief Send a WebSocket header or payload within one bounded socket-write budget.
 * @return Full byte count on success or a fatal HTTPD socket error. Do not retry
 *         on the same connection: some bytes may already have been sent.
 */
int remote_display_send(httpd_handle_t server, int fd, const char *data, size_t len, int flags);
