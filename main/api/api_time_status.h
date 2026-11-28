/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
/** @file
 * @brief Clock status JSON and HTTP entry points.
 */
#pragma once
#include "cJSON.h"
#include "esp_http_server.h"
/** @brief Append current time and synchronization metadata to the caller's JSON object. */
void api_time_status_add(cJSON *obj);
/** @brief Serve clock status without loading the complete runtime settings document. */
esp_err_t api_time_status_get(httpd_req_t *req);
