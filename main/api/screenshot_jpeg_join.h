/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Join MCU-row-aligned, baseline 4:4:4 JPEG bands with identical coding tables.
 * Output is not publishable until all rows and the final EOI have been written. */
typedef struct {
    uint8_t *data;
    size_t capacity, used, header_bytes;
    uint16_t width, height, band_rows, rows;
    unsigned bands;
} screenshot_jpeg_join_t;

bool screenshot_jpeg_join_init(screenshot_jpeg_join_t *join, uint8_t *out, size_t capacity,
                              uint32_t width, uint32_t height, uint32_t band_rows);
bool screenshot_jpeg_join_append(screenshot_jpeg_join_t *join, const uint8_t *jpeg, size_t size);
