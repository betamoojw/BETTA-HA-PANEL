/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Single-file replacement for callers that may share the flash gate.
 */
#pragma once

#include <stddef.h>

#include "esp_err.h"

/** @brief Replace a file only after writing and flushing its sibling temporary file.
 *
 * A failed write does not truncate the destination. A subsequent call can
 * overwrite a stale `.tmp` file, which is not listed as a JSON store entry.
 *
 * @param path Destination file path.
 * @param data Borrowed source buffer, or NULL when len is zero.
 * @param len Number of bytes to write.
 * @return ESP_OK after rename, or an error from input validation, gate acquisition, or I/O.
 */
esp_err_t atomic_file_write(const char *path, const void *data, size_t len);

/** @brief Perform the same replacement without taking or releasing the flash gate.
 * @pre The caller holds the flash gate for the surrounding transaction.
 */
esp_err_t atomic_file_write_locked(const char *path, const void *data, size_t len);
