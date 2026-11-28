/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw
 * Adapted by andre68723 from betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3,
 * commit 57df6a3, for the shared LVGL 9 adapter and all panel profiles.
 */
/** @file
 * @brief HTTP registration interface for Live Display.
 */
#pragma once
#include "esp_http_server.h"
/** @brief Register the Live Display session and WebSocket endpoints when supported.
 * @return ESP_OK also when compiled out. If WebSocket registration fails, the session route is removed.
 */
esp_err_t api_remote_display_register(httpd_handle_t server);
