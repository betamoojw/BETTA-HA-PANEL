/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief Panel HTTP server lifecycle and handle access.
 */
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/** @brief Start the singleton HTTP server with a bounded share of the lwIP socket pool.
 *
 * Route-registration errors may leave the server started. Stop it before
 * retrying registration through this entry point.
 */
esp_err_t http_server_start(void);

/** @brief Stop HTTPD and invalidate the stored handle; a previously borrowed handle must not be used. */
void http_server_stop(void);
/** @return Current server handle, or NULL when the server is not running. */
httpd_handle_t http_server_get_handle(void);
