/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once

#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>
#define SCREENSHOT_BAND_ROWS 32U
#define SCREENSHOT_MAX_DIMENSION 4096U
#define SCREENSHOT_MAX_BANDS (SCREENSHOT_MAX_DIMENSION / SCREENSHOT_BAND_ROWS)
#define SCREENSHOT_MAX_ENCODED (1024U * 1024U)
typedef struct {
    uint32_t width, height, count;
    uint8_t *bands[SCREENSHOT_MAX_BANDS];
} screenshot_frame_t;
/* Complete immutable BGR888 frame in separate PSRAM bands. Workspace is borrowed
 * (JPEG reuses aligned output scratch). Failure releases all owned bands; busy
 * distinguishes lock timeout. The caller frees a successful frame. */
bool api_screenshot_capture(const char *format, screenshot_frame_t *frame, bool *busy,
                            uint8_t *workspace, size_t workspace_capacity);
const uint8_t *screenshot_frame_row(const screenshot_frame_t *frame, uint32_t y);
void screenshot_frame_free(screenshot_frame_t *frame);
