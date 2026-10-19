/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#pragma once
#include "cJSON.h"
#include "esp_http_server.h"
void api_time_status_add(cJSON *obj);
esp_err_t api_time_status_get(httpd_req_t *req);
