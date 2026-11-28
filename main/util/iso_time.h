/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Parse timestamp text used by Home Assistant history and state fields.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @brief Convert a `YYYY-MM-DDTHH:MM:SS` timestamp from 1970 onward to UTC epoch seconds.
 *
 * Fractional seconds are discarded. The timezone suffix may be a numeric
 * `+HH:MM` or `-HH:MM` offset, `Z`, or absent (treated as UTC).
 *
 * @param text Borrowed NUL-terminated timestamp.
 * @param[out] epoch UTC epoch seconds, written only on success.
 * @return True if the text could be parsed and its fields passed the range checks.
 */
bool iso_time_parse(const char *text, int64_t *epoch);
