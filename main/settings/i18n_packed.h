/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Decompression and ownership contract for built-in translation catalogs.
 */
#pragma once
#include <stddef.h>
#include "cJSON.h"

/** @brief Decode one complete zlib stream using per-call scratch.
 * @param data Borrowed compressed catalog.
 * @param size Compressed length in bytes.
 * @param json_size Expected decoded length in bytes, excluding the added NUL.
 * @return Caller-owned JSON object, or NULL for invalid input or allocation failure.
 */
cJSON *i18n_packed_parse(const unsigned char *data, size_t size, size_t json_size);
