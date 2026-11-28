/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Capture-owner and worker interfaces for local camera image output.
 */
#pragma once
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/** @return Whether this firmware supports built-in JPEG output, independent of the saved opt-in. */
bool camera_output_available(void);
/** @return Clockwise camera correction in degrees for the applied display pose, or zero on unsupported boards. */
int camera_output_rotation(void);
/** @brief Check whether a picture is currently requested.
 * @pre Call from the existing CSI capture owner.
 * @return True only while demand is unexpired and local output settings can be read as enabled.
 */
bool camera_output_needed(void);
/** @brief Offer a frame from the CSI capture owner.
 *
 * Recheck demand, settings generation, and display pose after encoding before
 * attempting to publish the frame.
 *
 * @param rgb565 Borrowed sensor RGB565 bytes, held by the caller until return.
 * @param width Sensor frame width in pixels.
 * @param height Sensor frame height in pixels.
 */
void camera_output_publish(const uint8_t *rgb565, unsigned width, unsigned height);

/** @brief Try to drop the cached JPEG with a bounded mutex wait; does not change the publish sequence. */
void camera_output_invalidate(void);

/** @brief Request a fresh JPEG on a worker with a three-second polling deadline.
 *
 * Output must remain enabled. The cached frame must match the current settings
 * generation and display pose, and be less than 1.5 seconds old. With both
 * output pointers supplied, failure leaves them set to NULL and zero.
 *
 * @param[out] jpeg JPEG buffer to release with free() on success.
 * @param[out] length Number of bytes in the returned JPEG.
 * @retval ESP_OK A fresh JPEG copy was returned.
 * @retval ESP_ERR_INVALID_ARG A required output pointer was missing on a supported board.
 * @retval ESP_ERR_INVALID_STATE Output was disabled or local settings could not be read.
 * @retval ESP_ERR_NO_MEM The JPEG copy could not be allocated.
 * @retval ESP_ERR_TIMEOUT No fresh matching JPEG arrived before the polling deadline.
 * @retval ESP_ERR_NOT_SUPPORTED This firmware cannot encode built-in camera images.
 */
esp_err_t camera_output_snapshot(uint8_t **jpeg, size_t *length);

/** @return Sequence advanced only when a new JPEG is published; zero on unsupported boards. */
uint32_t camera_output_sequence(void);
