/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Raw and display-formatted firmware build timestamps.
 */
#pragma once

#include <stddef.h>

/** @brief Compiler-provided civil build date and time, not the panel's runtime clock. */
extern const char app_build_stamp[];

/** @brief Format the build date in the panel's date order followed by HH:MM.
 *
 * This changes display order only; it does not convert the compiler's time to
 * the panel's timezone. If the stamp cannot be parsed, copy its raw text.
 *
 * @param[out] out Caller-owned destination buffer.
 * @param out_len Available bytes including the NUL terminator; short buffers truncate.
 */
void app_build_stamp_format(char *out, size_t out_len);
