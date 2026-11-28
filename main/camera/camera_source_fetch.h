/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Worker snapshot retrieval and polling policy by camera ID.
 */
#pragma once
#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>
/** @brief Fetch a snapshot on a worker, not from the LVGL task.
 *
 * Saved sources must return HTTP 200 without redirects and a complete response
 * of at most 1 MiB with JPEG start and end markers. Discard a result if the
 * settings generation changes during the fetch. With a non-NULL ID and both
 * output pointers supplied, failure leaves the outputs set to NULL and zero.
 *
 * @param id Built-in "local" ID or a saved source ID; arbitrary request targets are not accepted.
 * @param[out] jpeg JPEG buffer to release with free() on success.
 * @param[out] length Length of the returned JPEG in bytes.
 * @return ESP_OK on success, or a fetch or configuration error.
 */
esp_err_t camera_source_fetch(const char *id, uint8_t **jpeg, size_t *length);

/** @brief Resolve a polling interval without fetching an image or checking output permission.
 * @return Interval in milliseconds: 500 for local, or 10000 if the saved source cannot be read.
 */
int camera_source_refresh_ms(const char *id);
