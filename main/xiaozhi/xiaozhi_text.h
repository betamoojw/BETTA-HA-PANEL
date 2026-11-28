/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Helpers for bounding assistant text in fixed-size buffers.
 */
#pragma once
#include <stddef.h>
#include <string.h>

/** @brief Preserve complete code points when bounding a valid UTF-8 JSON string.
 * This is display truncation, not validation of arbitrary byte streams.
 * @param[out] destination Destination buffer; left untouched if capacity is zero.
 * @param capacity Available bytes including the NUL terminator.
 * @param source NUL-terminated UTF-8 source, not overlapping the destination.
 */
static inline void xiaozhi_text_copy(char *destination, size_t capacity, const char *source)
{
    if (!capacity)
    {
        return;
    }
    size_t length = 0;
    while (length + 1 < capacity && source[length])
    {
        ++length;
    }
    if (source[length])
    {
        while (length && ((unsigned char) source[length] & 0xc0) == 0x80)
        {
            --length;
        }
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}
