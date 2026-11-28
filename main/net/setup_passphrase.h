/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The first-run setup access point's WPA2 password: two words and two digits ("maple-river-42"),
 * easier to read off the display and type on a phone than twelve random characters.
 */
/** @file
 * @brief Readable WPA2 setup passphrase formatting.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define SETUP_PASSPHRASE_WORD_COUNT 1024U /**< Number of entries in the fixed setup dictionary. */
/** @brief Buffer capacity including the terminator and space for the longest formatted value. */
#define SETUP_PASSPHRASE_MAX_LEN 20U

/** @brief Write a setup passphrase as two words and two decimal digits.
 * @param[out] out Caller-owned text buffer.
 * @param out_len Capacity in bytes including the NUL terminator.
 * @param r1 First raw random value, reduced to a word index.
 * @param r2 Second raw random value, reduced to a word index.
 * @param r3 Third raw random value, reduced to a two-digit suffix.
 * @return Text length excluding the NUL, or zero when no complete value fits.
 */
size_t setup_passphrase_format(char *out, size_t out_len, uint32_t r1, uint32_t r2, uint32_t r3);

/** @return Borrowed dictionary word for testing, or an empty string out of range. */
const char *setup_passphrase_word(size_t index);
