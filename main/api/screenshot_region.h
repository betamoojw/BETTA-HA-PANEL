/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
/* Caller holds the display lock. Borrowed pixels are never freed. Temporary
 * draw tasks/layers and refreshing-display state are restored on failure. */
bool screenshot_render_region(lv_obj_t *obj, lv_color_format_t format, const lv_area_t *area,
                              uint8_t *pixels, size_t capacity);
