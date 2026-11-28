/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Device-side OTA confirmation after the running image meets the health policy.
 *
 * An image started as pending verification can roll back if the panel resets
 * before the running image is marked valid.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Start health polling at the end of app_main, only for a pending image. */
void ota_validate_start(void);

/** @brief Read the flashed bootloader's version and optional build timestamp.
 * @param[out] version Required destination for the bootloader project version.
 * @param[out] built Optional destination for the build timestamp.
 * @param built_len Size of the timestamp buffer in bytes.
 * @return True when the bootloader description could be read.
 */
bool ota_validate_bootloader_info(uint32_t *version, char *built, size_t built_len);

/** @return Static state name for the running image, or `undefined` if not available. */
const char *ota_validate_image_state(void);
