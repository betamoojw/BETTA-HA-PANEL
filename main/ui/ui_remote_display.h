/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 betamoojw
 * Adapted by andre68723 from betamoojw/BETTA-HA-PANEL-4-Guiton-ESP32S3,
 * commit 57df6a3, for the shared LVGL 9 adapter and all panel profiles.
 */
#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_REMOTE_TILE_SIZE 32U
#define UI_REMOTE_MAX_DIMENSION 1280U
typedef struct ui_remote_frame ui_remote_frame_t;

/* Capture and input operations serialize internally with the LVGL port. */
void ui_remote_display_prepare_dashboard(void);
bool ui_remote_display_supported(void);
esp_err_t ui_remote_display_init(void);
/* Open only if the display still matches the transport's tile allocation. */
esp_err_t ui_remote_display_open(uint16_t width, uint16_t height);
void ui_remote_display_close(void);
/* The HTTP task owns the frozen frame between capture and ack/close. */
bool ui_remote_display_capture(const ui_remote_frame_t **frame, uint16_t *width, uint16_t *height, unsigned *retry_ms);
const uint8_t *ui_remote_frame_row(const ui_remote_frame_t *frame, unsigned y);
bool ui_remote_frame_tile_changed(const ui_remote_frame_t *frame, unsigned tile);
bool ui_remote_display_memory_fits(uint16_t width, uint16_t height, uint16_t packet_bytes, uint16_t tile_bytes);
void ui_remote_display_ack(void);
bool ui_remote_display_control(bool enabled);
bool ui_remote_display_pointer(unsigned kind, int x, int y);
bool ui_remote_display_heartbeat(void);
void ui_remote_display_suspend(bool suspended);
bool ui_remote_display_available(void);

bool ui_remote_display_geometry(uint16_t *width, uint16_t *height);
