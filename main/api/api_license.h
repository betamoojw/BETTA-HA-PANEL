/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Registration interface for device license routes.
 */
#pragma once
#include "esp_http_server.h"
/** @brief Register license management and the reduced setup/status route.
 * @return ESP_OK on success or the first registration error.
 */
esp_err_t api_license_register(httpd_handle_t server);
