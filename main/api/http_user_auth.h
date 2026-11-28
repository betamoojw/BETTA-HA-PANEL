/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Local account middleware and request-scoped identity access.
 */
#pragma once
#include "api/http_guard.h"
#include "settings/user_auth.h"
#include "cJSON.h"

/** @brief Enforce the route role and mutation-origin policy before invoking the callback.
 * @pre Call from the HTTP server task through the shared request guard.
 */
esp_err_t http_user_auth_handle(httpd_req_t *req, http_guard_handler_t next);

/** @brief Register sign-in, session and account-management routes with the shared guard. */
esp_err_t http_user_auth_register(httpd_handle_t server);

/** @return Borrowed identity for this handler invocation. Do not retain it across requests. */
const user_auth_info_t *http_user_auth_current(void);

/** @return Borrowed request cookie token, possibly empty. Capture a lease before handing work to another task. */
const char *http_user_auth_token(void);

/** @brief Restrict non-admin settings responses to permitted display preferences. */
void http_user_auth_filter_settings(cJSON *root);
