/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Streaming box downscaler for cover art, with no full source image buffer.
 *
 * Each source pixel maps to one output bin by scaling and rounding its
 * coordinates down. Output color is weighted by alpha, while alpha itself
 * is averaged. Only shrinking or a 1:1 copy is supported.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Maximum source pixels per output pixel to keep the 32-bit sums in range. */
#define COVER_BOX_MAX_SAMPLES 60000U

/** @brief Row accumulator borrowing caller-owned scratch for the duration of a resize. */
typedef struct
{
    uint32_t src_w;
    uint32_t src_h;
    uint32_t out_w;
    uint32_t out_h;
    uint32_t next_sy;  /* next source row to be added */
    uint32_t rows;     /* source rows added to the current destination row */
    uint32_t ready_dy; /**< Destination row from the last add that reported a completed row. */
    uint32_t *sum;     /**< Four sums per output column: alpha-weighted RGB and alpha. */
    uint16_t *colcnt;  /**< Number of source columns assigned to each output column. */
} cover_box_t;

/** @return Required byte size of the accumulator buffer for out_w columns. */
size_t cover_box_sum_bytes(uint32_t out_w);
/** @return Required byte size of the column-count buffer for out_w columns. */
size_t cover_box_colcnt_bytes(uint32_t out_w);

/** @brief Initialize with source and destination dimensions in pixels.
 *
 * Supply scratch sized by cover_box_sum_bytes() and cover_box_colcnt_bytes().
 * Keep both buffers alive and exclusive to this resize until it finishes.
 *
 * @return False for missing buffers, zero sizes, upscaling, or a box beyond the sample limit.
 */
bool cover_box_init(cover_box_t *box, uint32_t src_w, uint32_t src_h, uint32_t out_w, uint32_t out_h, uint32_t *sum, uint16_t *colcnt);

/** @brief Add the next row of src_w RGBA pixels (four bytes per pixel).
 * @return True when an output row is ready. Take it before adding another row.
 *         Rows past src_h are ignored and return false.
 */
bool cover_box_add_rgba_row(cover_box_t *box, const uint8_t *rgba);
/** @brief Add the next opaque RGB row (three bytes per pixel), with the same readiness rule. */
bool cover_box_add_rgb24_row(cover_box_t *box, const uint8_t *rgb);
/** @brief Add the next opaque RGB565 row (src_w native 16-bit pixels), with the same readiness rule. */
bool cover_box_add_rgb565_row(cover_box_t *box, const uint16_t *src);

/** @return Zero-based index of the destination row signaled ready by the last add. */
uint32_t cover_box_ready_row(const cover_box_t *box);

/** @brief Write out_w native RGB565 pixels and reset the row accumulators.
 * @pre A row-add operation reported a completed row.
 * @param[in,out] box Accumulator for the current resize.
 * @param[out] dst Destination for the completed row.
 * @param[out] alpha Optional destination for out_w alpha bytes; may be NULL.
 */
void cover_box_take_rgb565_row(cover_box_t *box, uint16_t *dst, uint8_t *alpha);
