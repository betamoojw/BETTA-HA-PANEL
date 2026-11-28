/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief Request guard callbacks for registered HTTP handlers.
 */
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/** @brief Synchronous callback invoked with a borrowed HTTPD request. */
typedef esp_err_t (*http_guard_handler_t)(httpd_req_t *req);

/** @brief Initialize shared admission state; repeated successful calls reuse it. */
esp_err_t http_guard_init(void);
/** @brief Apply the shared admission, media-type and authorization checks before dispatch.
 * @param req Borrowed HTTPD request.
 * @param next_handler Handler invoked only after the checks pass.
 * @return The handler result or the result of sending the rejection response.
 * @retval ESP_ERR_INVALID_ARG Either argument is NULL.
 */
esp_err_t http_guard_handle(httpd_req_t *req, http_guard_handler_t next_handler);
