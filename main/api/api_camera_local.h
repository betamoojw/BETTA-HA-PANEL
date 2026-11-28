/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Registration interface for local and saved-camera HTTP routes.
 */
#pragma once
#include "esp_http_server.h"
/** @brief Register guarded camera settings, status, snapshot and built-in stream routes.
 * @return ESP_OK if all routes registered, or the first registration error.
 */
esp_err_t api_camera_local_register(httpd_handle_t server);
