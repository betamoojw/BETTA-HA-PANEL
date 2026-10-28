/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool camera_output_available(void);
/* Clockwise camera correction for the applied display pose, not a deferred save. */
int camera_output_rotation(void);
/* Called only by the existing CSI owner, never by HTTP/UI tasks. */
bool camera_output_needed(void);
void camera_output_publish(const uint8_t *rgb565, unsigned width, unsigned height);
void camera_output_invalidate(void);
/* Worker-only; returns an owned JPEG, waits at most 3 seconds for a fresh frame.
 * No image can be obtained without the saved opt-in. */
esp_err_t camera_output_snapshot(uint8_t **jpeg, size_t *length);
uint32_t camera_output_sequence(void);
