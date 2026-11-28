/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Geometry and rendering for software-transformed camera frames.
 */
#pragma once
#include "camera/camera_config.h"
/** @brief Compute output dimensions in pixels; half size rounds odd dimensions down.
 * @param width Source width in pixels.
 * @param height Source height in pixels.
 * @param resolution Zero for native size or one for half size.
 * @param rotation Clockwise degrees: 0, 90, 180, or 270.
 * @param[out] out_width Result width; swapped with height on a quarter turn.
 * @param[out] out_height Result height.
 * @return False for invalid geometry or arguments. Source dimensions must each be 2-2048 pixels.
 */
bool camera_image_size(int width, int height, int resolution, int rotation, int *out_width, int *out_height);

/** @brief Render to caller-owned RGB565 pixels without changing camera registers or auto exposure.
 * @param src Borrowed source pixels in tightly packed row-major order.
 * @param width Source width in pixels.
 * @param height Source height in pixels.
 * @param[out] dst Destination pixels, non-overlapping with src.
 * @param capacity Available destination bytes; at least two bytes per output pixel.
 * @param config Borrowed image settings validated by the camera configuration parser.
 * @param rotation Clockwise degrees, applied after sensor-axis mirroring.
 * @return True when the destination contains a complete transformed frame.
 *         Invalid input or insufficient capacity leaves the destination unchanged.
 */
bool camera_image_render(const uint16_t *src, int width, int height, uint16_t *dst, size_t capacity, const camera_config_t *config,
                         int rotation);
