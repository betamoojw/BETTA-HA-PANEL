/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw
 * Adapted by andre68723 from betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3,
 * commit 57df6a3, for the shared LVGL 9 adapter and all panel profiles.
 */
#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Capture and input operations serialize internally with the LVGL port. */
esp_err_t ui_remote_display_init(void);
esp_err_t ui_remote_display_open(void);
void ui_remote_display_close(void);
bool ui_remote_display_capture(const uint8_t **pixels, uint16_t *width, uint16_t *height, unsigned *retry_ms);
void ui_remote_display_ack(void);
bool ui_remote_display_control(bool enabled);
bool ui_remote_display_pointer(unsigned kind, int x, int y);
bool ui_remote_display_heartbeat(void);
void ui_remote_display_suspend(bool suspended);
bool ui_remote_display_available(void);

bool ui_remote_display_geometry(uint16_t *width, uint16_t *height);
