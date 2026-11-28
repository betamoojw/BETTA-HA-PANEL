/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Render a display region into borrowed scratch storage.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
/** @brief Render an LVGL region without taking ownership of the supplied pixels.
 * @pre The caller holds the display lock.
 * @param obj Object or layer root to redraw.
 * @param format RGB888 or ARGB8888.
 * @param area Borrowed inclusive LVGL pixel rectangle.
 * @param[out] pixels Caller-owned, tightly packed pixel storage.
 * @param capacity Available pixel storage in bytes.
 * @return True on complete redraw. On failure, discard partial pixels; LVGL
 *         refresh state is restored and temporary tasks and layers are released.
 */
bool screenshot_render_region(lv_obj_t *obj, lv_color_format_t format, const lv_area_t *area, uint8_t *pixels, size_t capacity);
