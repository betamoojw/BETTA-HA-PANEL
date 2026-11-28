/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Bounded in-memory log history for `GET /api/log`.
 */
#pragma once

#include <stddef.h>

/** @brief Install the capture hook early in startup if the ring can be allocated.
 *
 * Capturing does not replace the previous console output.
 */
void log_buffer_init(void);

/** @brief Copy raw bytes from the current ring; no terminator is appended.
 * @param offset Byte offset relative to the oldest byte currently held.
 * @param[out] out Destination with room for at least len bytes.
 * @param len Maximum bytes to copy.
 * @return The bytes copied; each call observes the then-current ring.
 */
size_t log_buffer_read(size_t offset, char *out, size_t len);

/** @return Current occupancy in bytes, which can change before the next read. */
size_t log_buffer_size(void);
