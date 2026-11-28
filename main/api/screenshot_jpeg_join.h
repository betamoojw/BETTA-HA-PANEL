/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief JPEG band assembler with caller-owned output storage.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief State for joining MCU-row-aligned, baseline 4:4:4 JPEG bands with identical coding tables.
 * Output is not publishable until all rows and the final EOI have been written. */
typedef struct
{
    uint8_t *data;    /**< Borrowed output buffer. */
    size_t capacity,  /**< Available buffer bytes. */
        used,         /**< Bytes written, including the final EOI only after completion. */
        header_bytes; /**< Header length used to compare coding tables for later bands. */
    uint16_t width,   /**< Full frame width in pixels. */
        height,       /**< Full frame height in pixels. */
        band_rows,    /**< Standard band height; the final band may be shorter. */
        rows;         /**< Number of assembled rows. */
    unsigned bands;   /**< Appended bands; determines the next restart marker. */
} screenshot_jpeg_join_t;

/** @brief Initialize a join into caller-provided storage without allocating memory.
 * @param[out] join Join state to initialize.
 * @param out Borrowed output buffer, valid until the join is finished.
 * @param capacity Available output bytes.
 * @param width Full frame width in pixels.
 * @param height Full frame height in pixels.
 * @param band_rows Standard band height, a positive multiple of eight; the last band may be shorter.
 * @return True for valid geometry and output storage.
 */
bool screenshot_jpeg_join_init(screenshot_jpeg_join_t *join, uint8_t *out, size_t capacity, uint32_t width, uint32_t height,
                               uint32_t band_rows);
/** @brief Validate and append one complete JPEG band; no input buffer is retained.
 * @return True on append. Publish the output only when the accumulated rows equal the full height.
 */
bool screenshot_jpeg_join_append(screenshot_jpeg_join_t *join, const uint8_t *jpeg, size_t size);
