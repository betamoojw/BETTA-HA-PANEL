/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "camera/camera_config.h"
/* Rotation is clockwise, after sensor-axis mirroring. Quarter turns swap the
 * output dimensions; no camera register, AE or aspect-ratio changes. */
bool camera_image_size(int width, int height, int resolution, int rotation, int *out_width, int *out_height);
bool camera_image_render(const uint16_t *src, int width, int height, uint16_t *dst, size_t capacity,
                         const camera_config_t *config, int rotation);
