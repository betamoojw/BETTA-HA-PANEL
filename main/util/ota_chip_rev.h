/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Check an ESP32-P4 image's silicon revision bounds before writing it.
 *
 * The bootloader checks compatibility at boot. The uploader can give an
 * earlier error using the revision bounds in the image prefix.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Inclusive silicon revision bounds encoded as major times 100 plus minor. */
typedef struct
{
    uint16_t min_rev; /**< Lowest supported chip revision. */
    uint16_t max_rev; /**< Highest supported chip revision, or 0xFFFF for no upper bound. */
} ota_image_revs_t;

/** @brief Result of comparing the chip revision with the image bounds. */
typedef enum
{
    OTA_REV_OK = 0,    /**< No revision mismatch detected. */
    OTA_REV_BELOW_MIN, /**< The chip is older than the image supports. */
    OTA_REV_ABOVE_MAX, /**< The chip is newer than the image supports. */
} ota_rev_result_t;

/** @brief Decode the little-endian revision bounds from the image prefix.
 * @param head Borrowed bytes beginning at the image magic.
 * @param len Number of available bytes.
 * @param[out] out Destination written only when the prefix can be read.
 * @return True for a long enough prefix with valid magic; this is not full image validation.
 */
bool ota_image_revs_parse(const uint8_t *head, size_t len, ota_image_revs_t *out);
/** @return The revision comparison, treating a NULL bound as no mismatch. */
ota_rev_result_t ota_image_revs_check(const ota_image_revs_t *image, uint16_t chip_rev);
/** @brief Render a revision such as 103 as `v1.3`.
 * @return The number of bytes written, excluding the terminator, after truncation if needed.
 */
size_t ota_rev_format(uint16_t rev, char *out, size_t out_len);
/** @brief Describe the image's supported range and the actual chip revision for the editor.
 * @return The number of bytes written, excluding the terminator.
 */
size_t ota_rev_mismatch_text(const ota_image_revs_t *image, uint16_t chip_rev, char *out, size_t out_len);
