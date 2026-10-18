/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Modified 2026 by andre68723 -- see docs/release-notes.md
 *
 * Minimal shim of the ESP-BSP `bsp/display.h` API for the panels3 (ESP32-S3)
 * variant. The Guition CYD board has no upstream BSP package, so we only
 * expose the symbols the application actually uses. Real implementations
 * live in main/drivers/display_init_panels3.c.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Turn the physical backlight off through the shared brightness policy. */
esp_err_t bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
