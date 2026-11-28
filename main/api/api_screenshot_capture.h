/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
/** @file
 * @brief Shared banded screenshot buffer and capture interface.
 */
#pragma once

#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>
#define SCREENSHOT_BAND_ROWS     32U                                               /**< Maximum rows per captured band. */
#define SCREENSHOT_MAX_DIMENSION 4096U                                             /**< Maximum capture width or height, in pixels. */
#define SCREENSHOT_MAX_BANDS     (SCREENSHOT_MAX_DIMENSION / SCREENSHOT_BAND_ROWS) /**< Frame band pointer capacity. */
#define SCREENSHOT_MAX_ENCODED   (1024U * 1024U)                                   /**< Encoded screenshot size budget in bytes. */

/** @brief Owned PSRAM bands containing tightly packed BGR888 pixels. */
typedef struct
{
    uint32_t width,                       /**< Captured width in pixels. */
        height,                           /**< Captured height in pixels. */
        count;                            /**< Number of bands in a successful frame. */
    uint8_t *bands[SCREENSHOT_MAX_BANDS]; /**< Top-to-bottom bands; the final band can be short. */
} screenshot_frame_t;

/** @brief Capture an immutable BGR888 frame in separate PSRAM bands.
 * @param format Format label used in capture diagnostics.
 * @param[out] frame Destination frame; release a successful capture with screenshot_frame_free().
 * @param[out] busy Set on a display lock timeout, not on allocation or render failure.
 * @param workspace Borrowed render scratch, or NULL to allocate scratch for the call.
 * @param workspace_capacity Available render scratch bytes if workspace is supplied.
 * @return True for a complete frame. On failure capture releases any partially allocated bands.
 */
bool api_screenshot_capture(const char *format, screenshot_frame_t *frame, bool *busy, uint8_t *workspace, size_t workspace_capacity);
/** @return Borrowed pixels for a frame row, valid only while the frame is owned.
 * @pre The caller supplies a captured frame and a row index below its height.
 */
const uint8_t *screenshot_frame_row(const screenshot_frame_t *frame, uint32_t y);
/** @brief Release the frame's owned bands. */
void screenshot_frame_free(screenshot_frame_t *frame);
