/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include "api/http_guard.h"
#include "settings/user_auth.h"
#include "cJSON.h"

esp_err_t http_user_auth_handle(httpd_req_t *req, http_guard_handler_t next);
esp_err_t http_user_auth_register(httpd_handle_t server);
const user_auth_info_t *http_user_auth_current(void);
const char *http_user_auth_token(void);

/* Only display preferences are exposed to non-admin settings readers. */
void http_user_auth_filter_settings(cJSON *root);
