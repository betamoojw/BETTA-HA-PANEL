/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- see docs/release-notes.md
 */
#pragma once
#include "esp_lcd_panel_ops.h"
#include "drivers/display_rotation.h"

typedef struct display_present display_present_t;

/* Install before the first LVGL flush. Owns the DPI completion callback and
 * uses the two framebuffers already allocated by the LCD-X BSP. */
display_present_t *display_present_create(esp_lcd_panel_handle_t panel,
                                          uint16_t width, uint16_t height);
esp_err_t display_present_write(display_present_t *ctx, display_rect_t area,
                                const void *packed_rgb565, bool last);
