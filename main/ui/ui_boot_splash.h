/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Add an optional status line before the first refresh of a new splash. */
esp_err_t ui_boot_splash_show(const char *initial_status);
/* Mark a persistent provisioning screen; applied by its LVGL timer. */
void ui_boot_splash_set_setup(void);
void ui_boot_splash_set_title(const char *title_text);
void ui_boot_splash_clear_status(void);
void ui_boot_splash_set_status(const char *status_text);
void ui_boot_splash_set_status_layout(bool centered, uint16_t width, int16_t x_offset);
void ui_boot_splash_set_status_x_offset(int16_t x_offset);
void ui_boot_splash_set_progress(uint8_t progress_percent);
/* Dismiss the splash on the LVGL task after its minimum display time. */
void ui_boot_splash_request_hide(void);
/* Attempt dismissal before returning when showing a coprocessor update result. */
void ui_boot_splash_hide(void);
