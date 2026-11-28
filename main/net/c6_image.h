/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Coprocessor image archive decoding and ownership contract.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/** @brief Decode the embedded archive into PSRAM and verify its decoded length and SHA-256.
 * @param archive Borrowed compressed image archive.
 * @param archive_size Archive length in bytes, including its header.
 * @param[out] data Decoded buffer; release with heap_caps_free() on success.
 * @param[out] size Decoded byte count.
 * @return ESP_OK for a verified image; neither output is retained by the decoder.
 *         When both output pointers are supplied, failure clears them.
 */
esp_err_t c6_image_unpack(const uint8_t *archive, size_t archive_size, uint8_t **data, size_t *size);
