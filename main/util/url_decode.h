/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Decode URL-encoded values returned by HTTP query parsing.
 */
#pragma once

/** @brief Decode `%HH` escapes and convert `+` to spaces in a writable NUL-terminated buffer.
 *
 * This is a single pass. Malformed percent escapes do not emit a decoded byte,
 * and `%00` is retained so it cannot truncate the value.
 */
void url_decode_in_place(char *s);
